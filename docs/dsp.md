# DSP

## Scope and current state

Emulate the SNES DSP: eight voices, BRR, interpolation, pitch/envelopes, mixing,
noise, modulation and echo/FIR. SPU2 and PSP audio are output devices, not
replacements for DSP semantics. A register/address layer, 32-S-SMP-cycle event
cadence, eight voice-control records and a pure BRR block decoder now exist.
Per-voice BRR traversal decodes four samples at a time into a twelve-sample ring,
advances the raw 14-bit pitch and exposes a four-point interpolation input. The
published 512-entry table and documented wrap/clamp arithmetic now resolve the
Gaussian BRR source. An 11-bit ADSR/GAIN envelope advances from the shared rate
counter and writes `ENVX`; the same counter clocks a shared 15-bit noise LFSR at
`FLG.NCK`. `NON` chooses BRR or noise without stopping transport. Both paths now
produce left-aligned signed-15 envelope output, `OUTX`, signed left/right voice
volume and sample-level `PMON`. Voices are accumulated 0 through 7 with signed-16
saturation after each addition, then signed `MVOLL`/`MVOLR` is applied. `EON`
drives a separate ordered send mix into the `ESA`/`EDL` circular ARAM buffer and
eight-tap signed FIR. Signed `EFB` closes the protected feedback path, signed
`EVOLL`/`EVOLR` joins the final clamped mix and `FLG.MUTE` gates an inspectable
DAC-facing sample without stopping processing. Exact DSP microphases, post-DAC
polarity, PCM buffering and device delivery remain absent.

## Design direction

Define fixed-point widths, saturation/wrap and sample-event ordering explicitly. Keep voice state deterministic. Consider packed integer or vector kernels only after correct reference output and per-console profiling; approximation needs documented impact and refinement criteria.

## Verification criteria

Use independently authored BRR vectors, envelope transitions, key-on/off timing, echo-ring boundaries, FIR accumulation and signed-overflow edges. Compare sample sequences, not merely whether sound is audible. Verify optimized kernels against reference outputs.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Document
implementation evidence before promotion.

- [ ] [IN PROGRESS] Register state — 128 bytes, address latch and `$F2/$F3` access exist; most side effects/polling remain
- [ ] [IN PROGRESS] Eight voice state — BRR/noise, Gaussian, envelope, PMON, dry mixing and `EON` send selection exist; exact microphases remain
- [ ] [IN PROGRESS] BRR decode — exact block arithmetic and four-sample per-voice streaming exist; exact 32-step phase placement remains
- [ ] [IN PROGRESS] BRR loop/end behavior — completed end blocks set `ENDX` and jump through the current directory loop pointer; exact microsteps/envelope zero remain
- [ ] [IN PROGRESS] Pitch progression — raw and PMON-adjusted 2.12 steps plus the documented cap exist; exact register phases remain
- [ ] [IN PROGRESS] Interpolation — the 512-entry table, four-point wrap/clamp arithmetic and BRR output exist; exact microphases remain
- [ ] [IN PROGRESS] ADSR — 11-bit state and shared counter drive resolved BRR/noise samples; exact per-voice microphases remain
- [ ] [IN PROGRESS] GAIN envelopes — Direct, linear/exp decrease, linear/bent increase and release progression exist; exact per-voice microphases remain
- [ ] [IN PROGRESS] Key-on/key-off — alternating-sample polling, five-event preparation and release decay exist; exact microsteps remain
- [ ] [IN PROGRESS] Noise generator — seed/recurrence/rates, `NON`, envelope/`OUTX`, stereo and dry mixing exist; exact microphases remain
- [ ] [IN PROGRESS] Pitch modulation — preceding-voice output, staged signed shifts, voice-0 ignore, target-`NON` suppression and cap exist; exact PMON/NON fetch phases remain
- [ ] [IN PROGRESS] Voice volumes — signed left/right arithmetic feeds BRR/noise voices into the resolved ordered mixer; exact microphases remain
- [ ] [IN PROGRESS] Main volumes — signed left/right dry output is integrated with echo volume and final clamping; exact fetch phases remain
- [x] Echo buffer — `ESA` one-sample latch, `EDL` zero/fixed lengths, stereo reads/writes and 16-bit wrap are host verified
- [x] FIR filter — eight signed 1.7 taps, partial wrap, final clamp, low-bit clear and independent channels are host verified
- [x] Feedback — ordered `EON` send mix, signed `EFB`, final clamp, low-bit clear and `FLG` write protection are host verified
- [ ] [IN PROGRESS] Clamping and signed arithmetic — BRR, dry, FIR, feedback and final-mix rules are explicit; post-DAC polarity remains
- [ ] [IN PROGRESS] Sample timing — deterministic sample values and 32-cycle event cadence exist; exact DSP pipeline phases remain
- [ ] [IN PROGRESS] DSP register side effects — write inhibition, `ENDX`, `FLG` reset/mute/echo gate and sample-level `ESA`/`EDL` latching exist; exact phases remain
- [ ] [IN PROGRESS] Golden sample tests — Gaussian, PMON, dry mix, echo/FIR/feedback, mute and chunk vectors exist; buffered PCM vectors remain

## Research and decisions

The implemented subset is documented in the
[APU clock/DSP register audit](apu-clock-dsp-register-audit.md) and
[voice/BRR audit](sdsp-voice-brr-foundation-audit.md). The connected traversal is
recorded in the [BRR stream/pitch audit](sdsp-brr-stream-pitch-audit.md).
ADSR/GAIN progression is recorded in the
[envelope audit](sdsp-envelope-audit.md).
Shared noise state and selection are recorded in the
[noise audit](sdsp-noise-audit.md).
Noise envelope/`OUTX`/voice-volume output is recorded in the
[noise voice-output audit](sdsp-noise-voice-output-audit.md).
Ordered saturation and signed main volume are recorded in the
[dry-mix audit](sdsp-dry-mix-audit.md).
Gaussian coefficients, BRR output scaling and pitch modulation are recorded in
the [Gaussian/PMON audit](sdsp-gaussian-pmon-audit.md).
The circular ARAM echo path, FIR/feedback arithmetic and mute-gated final state
are recorded in the [echo/output audit](sdsp-echo-output-audit.md).
Register writes currently
become visible at register or sample-event granularity; exact within-32-cycle-loop
phases are not modeled. The readable
power-on shadow is deterministically zero while internal flags begin at `$E0`, so
the code does not claim undefined electrical values. SDK documentation is not a
source for SNES behavior.
