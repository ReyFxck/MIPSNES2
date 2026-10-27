# DSP

## Scope and current state

Emulate the SNES DSP: eight voices, BRR, interpolation, pitch/envelopes, mixing,
noise, modulation and echo/FIR. SPU2 and PSP audio are output devices, not
replacements for DSP semantics. A register/address layer, 32-S-SMP-cycle event
cadence, eight voice-control records and a pure BRR block decoder now exist. They
do not yet produce PCM.

## Design direction

Define fixed-point widths, saturation/wrap and sample-event ordering explicitly. Keep voice state deterministic. Consider packed integer or vector kernels only after correct reference output and per-console profiling; approximation needs documented impact and refinement criteria.

## Verification criteria

Use independently authored BRR vectors, envelope transitions, key-on/off timing, echo-ring boundaries, FIR accumulation and signed-overflow edges. Compare sample sequences, not merely whether sound is audible. Verify optimized kernels against reference outputs.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Document
implementation evidence before promotion.

- [ ] [IN PROGRESS] Register state — 128 bytes, address latch and `$F2/$F3` access exist; most side effects/polling remain
- [ ] [IN PROGRESS] Eight voice state — key waiting/delay/active/release control and source-directory latches exist; sample pipeline state remains
- [ ] [IN PROGRESS] BRR decode — exact pure nine-byte block arithmetic exists; voice ring/stream integration remains
- [ ] BRR loop/end behavior
- [ ] Pitch progression
- [ ] Interpolation
- [ ] ADSR
- [ ] GAIN envelopes
- [ ] [IN PROGRESS] Key-on/key-off — alternating-sample polling, five-event preparation, release marker and soft reset exist; exact microsteps and release envelope remain
- [ ] Noise generator
- [ ] Pitch modulation
- [ ] Voice volumes
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
[voice/BRR audit](sdsp-voice-brr-foundation-audit.md). Register writes currently
become visible at register or sample-event granularity; exact within-32-cycle-loop
phases are not modeled. The readable
power-on shadow is deterministically zero while internal flags begin at `$E0`, so
the code does not claim undefined electrical values. SDK documentation is not a
source for SNES behavior.
