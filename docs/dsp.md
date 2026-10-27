# DSP

## Scope and current state

Emulate the SNES DSP: eight voices, BRR, interpolation, pitch/envelopes, mixing,
noise, modulation and echo/FIR. SPU2 and PSP audio are output devices, not
replacements for DSP semantics. A first register/address state plus 32-S-SMP-cycle
sample-event cadence exists; it does not produce samples.

## Design direction

Define fixed-point widths, saturation/wrap and sample-event ordering explicitly. Keep voice state deterministic. Consider packed integer or vector kernels only after correct reference output and per-console profiling; approximation needs documented impact and refinement criteria.

## Verification criteria

Use independently authored BRR vectors, envelope transitions, key-on/off timing, echo-ring boundaries, FIR accumulation and signed-overflow edges. Compare sample sequences, not merely whether sound is audible. Verify optimized kernels against reference outputs.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Document
implementation evidence before promotion.

- [ ] [IN PROGRESS] Register state — 128 bytes, address latch and `$F2/$F3` access exist; most side effects/polling remain
- [ ] Eight voice state
- [ ] BRR decode
- [ ] BRR loop/end behavior
- [ ] Pitch progression
- [ ] Interpolation
- [ ] ADSR
- [ ] GAIN envelopes
- [ ] Key-on/key-off
- [ ] Noise generator
- [ ] Pitch modulation
- [ ] Voice volumes
- [ ] Main volumes
- [ ] Echo buffer
- [ ] FIR filter
- [ ] Feedback
- [ ] Clamping and signed arithmetic
- [ ] [IN PROGRESS] Sample timing — deterministic 32-cycle event cadence exists; DSP pipeline phases and sample values remain
- [ ] [IN PROGRESS] DSP register side effects — bit-7 write inhibition, `ENDX` clear and internal `FLG` reset/write exist; remaining effects pending
- [ ] Golden sample tests

## Research and decisions

The implemented subset is documented in the
[APU clock/DSP register audit](apu-clock-dsp-register-audit.md). Register writes
currently become visible immediately except for the documented inhibited/clear
cases; actual within-32-cycle-loop polling delays are not modeled. The readable
power-on shadow is deterministically zero while internal flags begin at `$E0`, so
the code does not claim undefined electrical values. SDK documentation is not a
source for SNES behavior.
