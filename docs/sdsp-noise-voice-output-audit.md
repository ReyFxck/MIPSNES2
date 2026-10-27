# S-DSP noise voice-output audit

## Scope

This slice connects the already verified shared noise source to the per-voice
envelope and voice-volume stages. For a voice selected by `NON`, the core now
records the signed-15 source, the post-envelope mono sample, `OUTX`, and the
post-`VxVOLL`/`VxVOLR` stereo pair. Public inspection APIs return these states
only while the noise path is resolved. A BRR voice remains deliberately invalid
until independently sourced Gaussian coefficients make its interpolated source
available.

This is the first numerically resolved voice sample, not final audio. There is no
eight-voice mixer, master volume, echo/FIR, mute/DAC stage, PCM queue or platform
delivery in this slice.

## Arithmetic and ordering

All operations use explicit signed arithmetic rather than implementation-defined
host shifts or `char` signedness:

- source-to-envelope: `S = arithmetic_shift_right(source * envelope, 11)`;
- `OUTX`: the high byte of the signed post-envelope sample;
- left/right voice volume: `V = arithmetic_shift_right(S * signed_volume, 7)`.

The envelope is unsigned 11-bit. Voice volumes are converted explicitly from
eight-bit two's-complement register values. Products fit in signed 32 bits and
the resolved results fit in signed 16 bits.

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
  provides the exact envelope `>>11`, voice-volume `>>7`, signed phase inversion,
  `OUTX` high-byte rule and sample-before-envelope-update ordering. No emulator
  implementation or coefficient table was imported.

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
test. Successful host/PS2/PSP CI remains the publication gate. Cross-build
success is not a target-runtime or audible-output result.

## Open boundaries

Gaussian interpolation remains blocked on independently sourced coefficients.
Pitch modulation, ordered voice mixing/clamping, master volume, mute, echo/FIR,
PCM buffering and PS2/PSP output remain separate work.
