# core/apu/dsp

`dsp.[ch]` owns the first deterministic S-DSP boundary: the `$F2` address latch,
128-byte `$F3` register file, address-bit-7 write inhibition, `ENDX` write clear,
internal `FLG` reset state and one sample event per 32 S-SMP cycles.

It also owns eight observable voice-control records, alternating-sample
`KON`/`KOFF` polling, the five-event key-on preparation delay, ARAM source-directory
latches and a pure exact-arithmetic BRR block decoder. Each attached voice now
streams BRR in four-sample groups through a physical twelve-sample ring, advances
an unmodulated 14-bit pitch counter, follows end/loop blocks and exposes the four
samples plus fractional position needed by the future Gaussian interpolator. An
11-bit envelope now follows the shared 32-rate counter through ADSR, Direct GAIN,
the four timed GAIN modes and release, updating `ENVX` deterministically.

This is not audio synthesis. Gaussian coefficients/output, envelope multiplication,
noise, pitch modulation, echo/FIR, mixing, PCM samples and platform output remain
absent.
See the [APU clock/DSP register audit](../../../docs/apu-clock-dsp-register-audit.md)
and [voice/BRR audit](../../../docs/sdsp-voice-brr-foundation-audit.md)
and [BRR stream/pitch audit](../../../docs/sdsp-brr-stream-pitch-audit.md)
and [envelope audit](../../../docs/sdsp-envelope-audit.md)
and [canonical status](../../../STATUS.md).
