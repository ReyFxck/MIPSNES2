# DSP

## Scope and current state

Emulate the SNES DSP: eight voices, BRR, interpolation, pitch/envelopes, mixing, noise, modulation and echo/FIR. SPU2 and PSP audio are output devices, not replacements for DSP semantics. No DSP implementation exists.

## Design direction

Define fixed-point widths, saturation/wrap and sample-event ordering explicitly. Keep voice state deterministic. Consider packed integer or vector kernels only after correct reference output and per-console profiling; approximation needs documented impact and refinement criteria.

## Verification criteria

Use independently authored BRR vectors, envelope transitions, key-on/off timing, echo-ring boundaries, FIR accumulation and signed-overflow edges. Compare sample sequences, not merely whether sound is audible. Verify optimized kernels against reference outputs.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] Register state
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
- [ ] Sample timing
- [ ] DSP register side effects
- [ ] Golden sample tests

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
