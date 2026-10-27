# DSP

## Scope and current state

Emulate the SNES DSP: eight voices, BRR, interpolation, pitch/envelopes, mixing,
noise, modulation and echo/FIR. SPU2 and PSP audio are output devices, not
replacements for DSP semantics. A register/address layer, 32-S-SMP-cycle event
cadence, eight voice-control records and a pure BRR block decoder now exist.
Per-voice BRR traversal decodes four samples at a time into a twelve-sample ring,
advances the raw 14-bit pitch and exposes a four-point interpolation input. It
also advances an 11-bit ADSR/GAIN envelope from the shared rate counter and writes
`ENVX`. That counter also clocks a shared 15-bit noise LFSR at `FLG.NCK`; `NON`
selects its signed pre-envelope input per voice while BRR transport continues. It
now multiplies resolved noise inputs by the current envelope, updates `OUTX` and
applies signed left/right voice volumes. It does not yet resolve BRR Gaussian
output, mix voices or produce PCM.

## Design direction

Define fixed-point widths, saturation/wrap and sample-event ordering explicitly. Keep voice state deterministic. Consider packed integer or vector kernels only after correct reference output and per-console profiling; approximation needs documented impact and refinement criteria.

## Verification criteria

Use independently authored BRR vectors, envelope transitions, key-on/off timing, echo-ring boundaries, FIR accumulation and signed-overflow edges. Compare sample sequences, not merely whether sound is audible. Verify optimized kernels against reference outputs.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Document
implementation evidence before promotion.

- [ ] [IN PROGRESS] Register state — 128 bytes, address latch and `$F2/$F3` access exist; most side effects/polling remain
- [ ] [IN PROGRESS] Eight voice state — key/source, BRR ring/traversal, raw pitch and envelope state exist; Gaussian/sample output remains
- [ ] [IN PROGRESS] BRR decode — exact block arithmetic and four-sample per-voice streaming exist; exact 32-step phase placement remains
- [ ] [IN PROGRESS] BRR loop/end behavior — completed end blocks set `ENDX` and jump through the current directory loop pointer; exact microsteps/envelope zero remain
- [ ] [IN PROGRESS] Pitch progression — unmodulated 14-bit 2.12 stepping and documented cap exist; PMON/noise phase behavior remains
- [ ] [IN PROGRESS] Interpolation — four ring samples and the eight-bit fractional input are exposed; Gaussian table/math/output remain
- [ ] [IN PROGRESS] ADSR — 11-bit state and shared counter drive resolved noise samples; BRR/Gaussian multiplication and exact per-voice microphases remain
- [ ] [IN PROGRESS] GAIN envelopes — Direct, linear/exp decrease, linear/bent increase and release progression exist; exact per-voice microphases remain
- [ ] [IN PROGRESS] Key-on/key-off — alternating-sample polling, five-event preparation and release decay exist; exact microsteps remain
- [ ] [IN PROGRESS] Noise generator — reset seed, recurrence, all rates, `NON`, envelope/`OUTX` and stereo voice output exist; exact microphases and mixing remain
- [ ] Pitch modulation
- [ ] [IN PROGRESS] Voice volumes — signed left/right arithmetic exists for resolved noise voices; BRR and mixer integration remain
- [ ] Main volumes
- [ ] Echo buffer
- [ ] FIR filter
- [ ] Feedback
- [ ] [IN PROGRESS] Clamping and signed arithmetic — BRR signed-16 clamp and signed-15 wrap are explicit; mixer/echo paths remain
- [ ] [IN PROGRESS] Sample timing — deterministic 32-cycle event cadence exists; DSP pipeline phases and sample values remain
- [ ] [IN PROGRESS] DSP register side effects — bit-7 write inhibition, `ENDX` clear and internal `FLG` reset/write exist; remaining effects pending
- [ ] Golden sample tests

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
Register writes currently
become visible at register or sample-event granularity; exact within-32-cycle-loop
phases are not modeled. The readable
power-on shadow is deterministically zero while internal flags begin at `$E0`, so
the code does not claim undefined electrical values. SDK documentation is not a
source for SNES behavior.
