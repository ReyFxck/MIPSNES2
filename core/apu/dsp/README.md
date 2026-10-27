# core/apu/dsp

`dsp.[ch]` owns the first deterministic S-DSP boundary: the `$F2` address latch,
128-byte `$F3` register file, address-bit-7 write inhibition, `ENDX` write clear,
internal `FLG` reset state and one sample event per 32 S-SMP cycles.

This is not audio synthesis. Voices, BRR, interpolation, envelopes, KON/KOFF
polling, noise, echo/FIR, mixing, PCM samples and platform output remain absent.
See the [APU clock/DSP register audit](../../../docs/apu-clock-dsp-register-audit.md)
and [canonical status](../../../STATUS.md).
