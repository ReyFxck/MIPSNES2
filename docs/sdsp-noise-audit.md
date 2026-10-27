# S-DSP noise generator audit

## Scope

This state-only slice adds the single S-DSP noise source shared by all eight
voices. It initializes the documented 15-bit LFSR to `$4000`, clocks it from the
existing global rate counter using `FLG.NCK`, and exposes its signed-15 value only
for voices selected by `NON`. Rate zero holds the generator; rates 1 through 31
use the same documented periods and relative offsets as the envelope engine.

`NON` does not stop or retime BRR transport. Pitch still advances the BRR stream,
end/loop processing still updates `ENDX`, and an end-without-loop header can still
select Release. The new inspection API reports the pre-envelope noise input; it
does not claim Gaussian output, envelope multiplication, volume, mixing or PCM.

## Arithmetic and time

The recurrence is implemented directly over a 15-bit unsigned state:

`N = (N >> 1) | (((N bit 0) XOR (N bit 1)) << 14)`

Bit 14 is sign-extended when the state is inspected as the signed-15 source
sample. Per-sample service checks the counter after its decrement, matching the
existing envelope convention. Idle advancement counts due rate events
arithmetically. Large event counts apply the linear recurrence with exponentiation
over GF(2), while short runs use direct steps; neither path assumes an LFSR period.
This keeps one-shot and arbitrarily chunked clock advancement equivalent without
iterating once per emulated sample while all voices are idle.

The current model observes `FLG.NCK` and `NON` at sample-event granularity. It does
not claim the exact cycle-28 `NON` latch or cycle-30 noise update inside the
32-step hardware schedule.

## Sources

- The [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual) identifies
  `NON` as the per-voice replacement of source data by one shared, unmodulated
  noise source and `FLG.NCK` as its clock selector.
- [Anomie's S-DSP Doc, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
  records the `$4000` reset seed, 15-bit feedback recurrence, global-counter
  scheduling, signed source role, and continued BRR progression while noise is
  selected. No emulator source or coefficient table was imported.

## Deterministic evidence

`tests/test_dsp.c` covers:

- reset seed and signed conversion;
- invalid arguments and per-voice `NON` selection, including identical values for
  multiple selected voices;
- rate-zero hold, rate-31 consecutive states, the slow rate-1 boundary and a
  phase-offset rate-2 event;
- a million-event scalar recurrence cross-check against the logarithmic bulk
  transform;
- one-shot/chunked equality for cycles, sample phase, shared counter and LFSR;
- continued BRR preload, ring rotation and traversal while `NON` is selected.

The completion gate is the clean host suite, focused ASan/UBSan, repository check,
GCC `-fanalyzer`, and successful host/PS2/PSP CI. Cross-build success is not a
target-runtime or audio-output result.

Local validation on 2026-10-27 passed:

- `make clean && make -j2 test`;
- focused GCC ASan/UBSan executions of `test-dsp`, `test-apu-timing` and
  `test-machine` with leak detection disabled;
- GCC `-fanalyzer` compilation of `dsp.c` and `test_dsp.c`;
- `python3 tools/check_repository.py` and `git diff --check`.

## Open boundaries

Gaussian interpolation and signed rounding remain blocked on independently
sourced coefficients. Envelope/sample multiplication, `OUTX`, pitch modulation,
voice/main volumes, mixing, echo/FIR, PCM buffering and platform delivery remain
separate work. Exact per-voice and global 32-step microphases still need stronger
hardware-timing evidence.
