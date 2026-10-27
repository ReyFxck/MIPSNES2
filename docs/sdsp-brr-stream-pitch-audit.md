# S-DSP BRR stream and pitch-input audit

Date: 2026-10-27

## Scope

This change connects the existing exact BRR arithmetic to each of the eight
portable S-DSP voice records. It adds deterministic source transport and the
input boundary for interpolation, not audible output:

- BRR decode in four-sample groups with predictor history carried continuously;
- a physical twelve-sample ring with independent logical read and write cursors;
- three group preloads during the five-event key-on preparation period;
- 16-bit ARAM wrapping for headers, data, adjacent blocks and directory words;
- completed-block advance, `ENDX` assertion and loop-address traversal;
- end-without-loop release state while source decoding continues;
- live `DIR`/`SRCN` loop-pointer refresh when an end block completes;
- raw 14-bit pitch-register capture and 2.12 interpolation-position advance; and
- a read-only four-sample plus eight-bit-fraction interpolation-input API.

The state name `ACTIVE` still means that key-on preparation is complete. No
Gaussian result, envelope level, volume, mixed sample or PCM buffer exists.

## Hardware evidence and decisions

The manufacturer [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual)
defines `VxPITCHL/H`, uses only the low six bits of the high register and gives
the pitch ratio as `P / 2^12`. It also defines the BRR block, directory and
end/loop flags. The core therefore masks pitch to 14 bits and treats `$1000` as
one decoded sample per output sample before modulation.

[Anomie's S-DSP hardware research, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
describes the twelve-sample physical ring, four-sample decode groups, predictor
carry, key-on preload groups, pitch-position threshold/cap, four-point Gaussian
input indexing and block-end behavior. The implemented sample-event model decodes
one new group when the interpolation position entering an event has crossed four
samples. Position is then reduced by four samples, raw pitch is added and the
result is capped at `$7FFF`.

On key-on, the ring bytes are deliberately preserved and the predictor histories
start from physical ring entries 11 and 10. The cursors restart at zero and the
first three four-sample groups replace the ring during preparation. `KOFF` and
`FLG.RES` alter voice control state but do not cancel that BRR transport. When an
end block completes, the loop pointer is reread through the then-current `DIR`
and `SRCN`; the stream jumps there whether or not the loop flag is set. A clear
loop flag separately enters release state.

These operations occur at deterministic sample-event boundaries. The exact
register-read/decode/write placement within the physical 32-step DSP loop remains
open. Pitch modulation and noise replacement also remain open, so this slice uses
only the unmodulated voice pitch registers.

No coefficient table or implementation source from another emulator was imported.
The code was written from the manufacturer description and independent hardware
research; the Gaussian calculation is intentionally deferred until its constants
and signed rounding can be added and audited as their own output-producing block.

## Executable evidence

`tests/test_dsp.c` covers:

- interpolation API rejection before a three-group preload and for invalid calls;
- exact three-group preload values, cursors and predictor histories;
- raw `$1080` pitch advance, integer sample selection and eight-bit fractions;
- the four-sample threshold, ring rotation and fourth-group decode order;
- traversal from a normal block into an end+loop block;
- `ENDX`, live `SRCN` directory refresh and the resulting loop address;
- end-without-loop release without stopping decode;
- continued decode after `FLG.RES` changes the control state to waiting;
- byte reads across `$FFFF` to `$0000` and nine-byte block-address wrapping; and
- identical stream position, ring contents and timing after one-shot versus
  irregularly chunked cycle advancement.

The standalone BRR vectors remain an independent regression layer for all shifts,
filters, clamp and signed-15 wrap arithmetic.

## Validation

| Check | Result |
|---|---|
| `make clean && make -j2 test` | Pass: full host suite and repository checker |
| Focused GCC ASan/UBSan build and DSP/APU/S-SMP/machine tests | Pass with leak detection disabled for the traced environment |
| GCC `-fanalyzer` on `dsp.c` and `test_dsp.c` | Pass with warnings promoted to errors |
| `git diff --check` | Pass |

The pinned host/PS2/PSP cross-build workflows are the publication gate.
Cross-compilation does not claim physical-console audio behavior.

## Deliberate limits

The exposed four samples and fraction are inputs only. The Gaussian coefficient
table, multiply/round/clamp sequence and actual voice sample are absent. ADSR,
GAIN, release decay, noise, pitch modulation, voice/main volumes, echo/FIR,
mixing, PCM buffering, resampling and platform delivery are also absent. Exact
32-step DSP microphases and register-write visibility remain unclaimed.
