# S-DSP resolved dry-mix audit

## Scope

This slice connects currently resolved per-voice stereo samples to an ordered
dry mixer. It records the saturated pre-main-volume pair, the pair after signed
`MVOLL`/`MVOLR`, and an eight-bit mask identifying every voice included in that
sample. The mask is part of the contract because only `NON` noise sources are
numerically resolved: an active BRR voice remains excluded until independently
sourced Gaussian coefficients provide its interpolated sample.

This is deterministic intermediate state, not final audio. Echo/FIR, echo
volume, mute/DAC behavior, PCM buffering and platform delivery remain absent.

## Arithmetic and order

For each channel, the mixer starts at zero and visits Voice 0 through Voice 7.
After adding each resolved `VxVOL` result it clamps immediately to
`[-32768, 32767]`. This order matters once an intermediate sum saturates; a
single wider sum followed by one final clamp is not equivalent.

After the eighth voice, the current pre-main-volume pair is multiplied by the
signed eight-bit `MVOLL`/`MVOLR` value and arithmetic-shifted right by seven.
The 16-bit result uses explicit two's-complement wrapping, including the corner
case `-32768 * -128 >> 7 = +32768`, whose low 16 bits represent `-32768`.
No C implementation-defined signed shift or out-of-range narrowing is used.

Register changes become visible at the next sample event in this sample-level
model. It does not claim the hardware's exact cycles 26/27 main-volume fetches
or the individual per-voice S4/S5 microphases.

## Sources

- The [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual)
  identifies signed voice and main-volume registers and places main and echo
  volume on the final left/right output path.
- [Anomie's S-DSP Doc, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
  specifies Voice-0-to-7 accumulation, signed-16 clamping after every addition,
  and the post-mix signed main-volume `>>7` step. No emulator source or Gaussian
  coefficient table was imported.

## Deterministic evidence

`tests/test_dsp.c` covers:

- null/initialized API behavior and an empty zero mix;
- an order-sensitive left-channel saturation vector whose wider final sum would
  produce a different result;
- independent left/right accumulation order and positive/negative voice volume;
- signed main-volume arithmetic, including negative fractional rounding;
- explicit 16-bit wrap for the `+32768` multiplication corner;
- all eight noise voices active simultaneously and a full `$FF` resolved mask;
- `FLG.RES` zeroing while retaining the explicit resolved-voice mask;
- one-shot/chunked state equality including the complete dry-mix record.

The clean host suite and repository checks pass locally. Focused ASan/UBSan runs
cover the DSP, APU timing bridge and machine integration, and GCC `-fanalyzer`
reports no diagnostics for the DSP implementation or its focused test.
Successful host/PS2/PSP CI remains the publication gate. Cross-build success is
not target-runtime or audible-output evidence.

## Open boundaries

BRR Gaussian interpolation is the primary missing dry-path source. Pitch
modulation, echo send mixing, echo RAM/FIR/feedback, `EVOL`, mute/DAC behavior,
PCM buffering and PS2/PSP output remain separate work.
