# Bus

## Scope and current state

The bootstrap bus header defines byte read/write callback types and context ownership only. No bus implementation or memory map exists. CPU produces 24-bit logical SNES addresses; mapping owns WRAM, ROM, SRAM, PPU/CPU/APU/DMA registers, mirrors, open bus and enhancement windows.

## Design direction

Evaluate direct page/block lookup for common RAM/ROM and a slow MMIO path. Define read/write side effects and access timing before integrating the CPU. Page size and callbacks in the final hot path remain undecided until target measurements. Do not cast logical addresses to native pointers.

## Verification criteria

Table-driven boundary tests must cover each mapping window, mirrored aliases, ROM write behavior, SRAM limits, side-effect registers and open-bus state. Validate out-of-range inputs and loader-owned lifetime. Address helper tests do not satisfy these mapping gates.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] WRAM allocation and mapping
- [ ] Logical 24-bit access integration
- [ ] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] SRAM windows
- [ ] PPU MMIO routing
- [ ] CPU I/O routing
- [ ] APU port routing
- [ ] DMA register routing
- [ ] Enhancement windows
- [ ] Open bus behavior
- [ ] Read side effects
- [ ] Write side effects
- [ ] Access timing
- [ ] Mapping boundaries and mirror tests
- [ ] Page/block mapping benchmark

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
