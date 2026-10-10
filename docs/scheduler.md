# Scheduler

## Scope and current state

Central deterministic timing coordinates 65C816, PPU, DMA/HDMA, timers, IRQ/NMI, SPC700, DSP and chips. No scheduler or clock constants are implemented.

## Design direction

Before coding define integer timestamp units, rational regional clocks, event priority/tie order, counter widths, rollover policy and the CPU event horizon. Bound stepping at observable events. Wall clocks only pace presentation; they must not decide emulated state.

## Verification criteria

Use event-order fixtures, simultaneous IRQ/NMI/DMA cases, CPU pause/resume checks, frame/scanline boundaries and long-run drift tests. Same ROM/state/input must give identical traces regardless of host speed. Avoid unrelated magic timing fixes.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] Master timestamp units
- [ ] Clock ratio representation
- [ ] Event tie ordering
- [ ] CPU event horizon
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [ ] Determinism tests
- [ ] Long-run clock drift tests

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
