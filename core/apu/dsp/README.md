# core/apu/dsp

`dsp.[ch]` owns the first deterministic S-DSP boundary: the `$F2` address latch,
128-byte `$F3` register file, address-bit-7 write inhibition, `ENDX` write clear,
internal `FLG` reset state and one sample event per 32 S-SMP cycles.

It also owns eight observable voice-control records, alternating-sample
`KON`/`KOFF` polling, the five-event key-on preparation delay, ARAM source-directory
latches and a pure exact-arithmetic BRR block decoder.

This is not audio synthesis. BRR streaming/rings, pitch, interpolation, envelopes,
release decay, noise, echo/FIR, mixing, PCM samples and platform output remain absent.
See the [APU clock/DSP register audit](../../../docs/apu-clock-dsp-register-audit.md)
and [voice/BRR audit](../../../docs/sdsp-voice-brr-foundation-audit.md)
and [canonical status](../../../STATUS.md).
