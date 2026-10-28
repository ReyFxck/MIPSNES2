# S-DSP Gaussian output and pitch-modulation audit

## Scope

This slice resolves the BRR dry path from the existing twelve-sample ring through
the S-DSP's 512-entry Gaussian coefficient ROM, four-point interpolation,
envelope, `OUTX`, signed per-voice volume and the ordered dry mixer. It also adds
sample-level `PMON` pitch modulation using the current post-envelope output of the
preceding voice.

This is deterministic intermediate sample state, not final audio. Echo send,
echo RAM/FIR/feedback, `EVOL`, `FLG.MUTE`, DAC behavior, PCM buffering and platform
delivery remain absent.

## Coefficient provenance

The 512 unsigned 12-bit coefficients were transcribed from the published table on
[SnesLab's Gaussian Filter page](https://sneslab.net/wiki/S-DSP/Gaussian_Filter)
and cross-checked against the table and four-tap formula in
[fullsnes](https://problemkaputt.de/fullsnes.htm). The import was validated as one
continuous index range from 0 through 511. No emulator source file or coefficient
array was copied.

The repository records a deterministic FNV-1a checksum over every coefficient as
little-endian `uint16_t`: `b1216dfae1423abe`. Tests also pin indices 0, 15, 16,
255, 256 and 511, require monotonic values no larger than `$0FFF`, and verify that
every four-coefficient phase sum lies between `$07FF` and `$0801`.

## Gaussian arithmetic

For fraction `i`, the four ring samples are ordered oldest through newest. Each
product uses an explicit arithmetic shift right by 10:

```text
partial  = gauss[$0ff-i] * oldest >> 10
partial += gauss[$1ff-i] * older  >> 10
partial += gauss[$100+i] * old    >> 10
partial  = signed16_wrap(partial)
newest   = gauss[$000+i] * new    >> 10
sample   = signed16_clamp(partial + newest) >> 1
```

The last shift converts the interpolator result to the signed-15 source domain.
The deliberate partial wrap and final clamp preserve the documented overflow
behavior; four `$4000`-negative inputs at phase zero produce the known positive
`$3FF8` pop rather than a simplified saturated result.

## Envelope, OUTX and volume scale

BRR interpolation and the noise LFSR expose right-aligned signed-15 sources in
`[-16384, 16383]`. The hardware's post-envelope sample is signed-15 data
left-aligned in a signed 16-bit word. With this core's right-aligned source
representation, the equivalent operation is:

```text
output = clear_low_bit(arithmetic_shift_right(source * envelope, 10))
OUTX   = high_byte(output)
left   = signed16_wrap(arithmetic_shift_right(output * VxVOLL, 7))
right  = signed16_wrap(arithmetic_shift_right(output * VxVOLR, 7))
```

Clearing the low bit is explicit for positive and negative values. This correction
also gives `OUTX` and pitch modulation their full documented signed range. BRR and
noise voices then enter the existing Voice-0-to-7, per-addition-clamped dry mixer
and signed `MVOLL`/`MVOLR` stage.

## Pitch modulation

Let `P` be the target voice's 14-bit pitch register value and `O` the preceding
voice's current signed 16-bit post-envelope output. For voices 1 through 7 whose
`PMON` bit is set and whose own `NON` bit is clear, the effective step is computed
with the two documented signed shifts in their original order:

```text
P' = P + arithmetic_shift_right(
             arithmetic_shift_right(O, 5) * P, 10)
position = min(position + P', $7fff)
```

Voice 0 ignores `PMON` bit 0. A target voice selected by `NON` uses its raw pitch;
noise itself is not pitch-modulated. An unresolved preceding voice contributes
zero modulation. Voice processing remains ordered from 0 to 7, so each target
observes the preceding voice's post-envelope output for the same sample-level
event. The effective step is kept wider than 14 bits and can approach twice the
register pitch before the interpolation-position cap.

## Deterministic evidence

`tests/test_dsp.c` covers:

- all 512 coefficients, checksum, anchors, monotonic/range checks and all phase
  quartet sums;
- independent Gaussian vectors at fractions 0, 127 and 255 plus the signed-wrap
  pop vector;
- a complete BRR key-on path through interpolation, envelope, `OUTX`, voice
  volume, main volume and live BRR/noise source switching;
- left-aligned envelope output, low-bit clearing and signed volume wrapping;
- positive PMON, maximum negative cancellation, staged negative rounding,
  target-`NON` suppression, ignored voice-0 bit, near-double positive step and
  the `$7fff` position cap;
- one-shot/chunked equality for the connected DSP state.

The clean `make -j2 test` host suite passes. Focused ASan/UBSan runs cover the
DSP, APU timing bridge and machine integration with leak detection disabled only
for the managed runtime. GCC `-fanalyzer` reports no diagnostics for `dsp.c` or
`test_dsp.c`; `git diff --check` and `tools/check_repository.py` also pass.
Revision `5d71e198` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40731608107),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40731607932) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40731607900).
Cross-build success is not target-runtime or audible-output evidence.

## Open boundaries

The model services one aggregate event per 32 S-SMP cycles. It does not yet claim
the exact S1-S9 register-read/write microphases, cycle-27 `PMON` fetch, cycle-28
`NON` visibility, or every write-race edge. Echo/FIR, mute/DAC, PCM buffering,
resampling and PS2/PSP audio delivery remain separate work.
