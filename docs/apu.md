# APU and audio

## Scope and current state

APU integration owns SPC700/DSP coordination, timers, CPU communication and generation of final emulated samples. Platform code owns delivery buffers and device APIs. No audio pipeline exists.

## Design direction

Define rational clock relationship to the main scheduler, port synchronization and sample buffer ownership before implementation. Platform pacing must not alter deterministic core state. Decide output format/resampling and underrun policy based on correctness and target latency measurements.

## Verification criteria

Test bidirectional communication, event ordering, long-run sample counts, buffer limits and deterministic sample hashes. Console audio tests must include underruns and sustained playback; host offline tests do not prove SPU2/PSP delivery.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] SPC700/DSP interconnect
- [ ] CPU/APU port synchronization
- [ ] Timer integration
- [ ] Sample generation pipeline
- [ ] Deterministic sample buffering
- [ ] Output sample format contract
- [ ] Resampling policy
- [ ] Underflow/overflow behavior
- [ ] Host offline audio tests
- [ ] Integrated APU regression tests

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
