# Debugging

## Scope and current state

Only the common logging sink exists. Traces, breakpoints, watchpoints, dumps and replay are future work. Logging performs no core-side I/O or allocation.

## Design direction

Design optional compile-time trace hooks with subsystem filters and bounded buffering. Avoid formatting, deep callbacks or synchronization in release hot paths. Record emulated timestamps and logical addresses; platform may store the output. Debug state must not change emulated results.

## Verification criteria

Compare deterministic execution with diagnostics on/off. Verify release overhead and trace-disabled binaries when trace infrastructure exists. Preserve reproducible bug metadata; never log secrets or ship commercial ROM dumps.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] CPU trace
- [ ] Register dump
- [ ] Memory trace
- [ ] Breakpoints
- [ ] Watchpoints
- [ ] PPU state dump
- [ ] Scanline log
- [ ] APU trace
- [ ] Scheduler events
- [ ] DMA trace
- [ ] Deterministic replay
- [ ] Trace filtering
- [ ] Release trace overhead verification

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
