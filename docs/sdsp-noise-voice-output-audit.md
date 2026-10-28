# S-DSP noise voice-output audit

## Scope

This slice connects the already verified shared noise source to the per-voice
envelope and voice-volume stages. For a voice selected by `NON`, the core now
records the signed-15 source, the post-envelope mono sample, `OUTX`, and the
post-`VxVOLL`/`VxVOLR` stereo pair. Public inspection APIs return these states
only while the source path is resolved. The later Gaussian milestone connects
BRR voices through the same envelope and volume path.

At this historical checkpoint this was the first numerically resolved voice
sample and there was no mixer or main volume. Later audited slices connect the
ordered dry mixer, Gaussian BRR source and PMON. Echo/FIR, mute/DAC, PCM queue and
platform delivery are still absent.

## Arithmetic and ordering

All operations use explicit signed arithmetic rather than implementation-defined
host shifts or `char` signedness:

- source-to-envelope for the core's right-aligned signed-15 source:
  `S = clear_low_bit(arithmetic_shift_right(source * envelope, 10))`;
- `OUTX`: the high byte of the signed post-envelope sample;
- left/right voice volume: `V = arithmetic_shift_right(S * signed_volume, 7)`.

The envelope is unsigned 11-bit. `S` is signed-15 data left-aligned in a signed
16-bit word, matching full-range `OUTX` and PMON. Voice volumes are converted
explicitly from eight-bit two's-complement register values, and their results use
explicit signed-16 wrapping.

At each sample event, the current source is multiplied by the current envelope;
only after that is the envelope advanced. This preserves the documented pipeline
ordering through Direct GAIN, ADSR and Release. Key-on exposes zero during the
five-event preparation window. `FLG.RES` clears the resolved mono/stereo state
and `OUTX`; when Release reaches zero, one final serviced event commits a zero
sample before idle bulk advancement resumes.

The model still operates at sample granularity. It does not claim the exact
per-voice S3/S4/S5/S8/S9 positions inside the 32-step hardware schedule, and a
`NON` change is synchronized at the next serviced sample rather than cycle 28.

## Sources

- The [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual) places
  `OUTX` after envelope multiplication and before per-voice volume, and defines
  signed left/right voice volumes.
- [Anomie's S-DSP Doc, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
  provides the envelope scaling, left-aligned 15-bit output, voice-volume `>>7`,
  signed phase inversion, `OUTX` high-byte rule and sample-before-envelope-update
  ordering. No emulator implementation or coefficient table was imported.

## Deterministic evidence

`tests/test_dsp.c` covers:

- invalid/unresolved mono and stereo inspection;
- five zero key-on samples followed by Direct GAIN output;
- positive and negative noise sources, including arithmetic rounding of `-3`;
- exact `OUTX` high bytes for negative and positive samples;
- positive and negative `VxVOLL`/`VxVOLR`, phase inversion and signed rounding;
- simultaneous voices sharing one noise source with distinct volume registers;
- Release output using the pre-decrement envelope, followed by a committed zero;
- `FLG.RES` zeroing and `NON` deselection invalidation;
- one-shot/chunked BRR/noise state equality including mono, stereo and `OUTX`.

The clean host suite and repository checks pass locally. Focused ASan/UBSan
runs cover the DSP, APU timing bridge and machine integration, and GCC
`-fanalyzer` reports no diagnostics for the DSP implementation or its focused
test. Revision `cb955698` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40603798701),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40603798698) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40603798694).
Cross-build success is not a target-runtime or audible-output result. Those CI
links validate the original checkpoint; the later left-alignment correction and
BRR integration are validated in the Gaussian/PMON audit.

## Open boundaries

Gaussian interpolation, pitch modulation, ordered voice mixing/clamping and main
volume are now connected and audited separately. Mute, echo/FIR, PCM buffering
and PS2/PSP output remain separate work.
