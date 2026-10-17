# Bus

## Scope and current state

The CPU emits logical 24-bit SNES addresses. The reference bus now provides a small,
caller-owned region table sufficient for deterministic CPU tests. A region is either:

- direct caller-owned bytes with explicit read-only/read-write policy; or
- an MMIO slow path with caller callbacks and context.

Mappings reject zero length, addresses beyond `0xFFFFFF`, wrap beyond the logical
space, overlap and more than sixteen regions. Accesses normalize to 24 bits. No heap,
platform API or native pointer is stored as a CPU-visible address.

The bus does not decide addressing-mode wrapping. The CPU emits each final logical
address: PBR:PC fetches wrap PC within one program bank, while implemented data words
and absolute indexes advance linearly through the 24-bit data space. Implemented
Direct and Direct-indexed operations emit bank-zero addresses; the CPU applies the
E/DL page-wrap rule before the bus sees them. Implemented indirect pointer reads also
arrive as ordinary logical bank-zero accesses: regular 16-bit pointers use the CPU's
E/DL wrapping rule, while long 24-bit pointers use the documented linear byte advance
inside bank zero. DBR or the long pointer then selects the final data bank, and any
post-index is applied to the full 24-bit data address. The bus only normalizes the
address it receives.

This is not the SNES memory map. WRAM mirrors, cartridge ROM/SRAM, PPU/CPU/APU/DMA
registers, enhancement windows, side effects and access timing remain unimplemented.

The scheduler foundation now contains a pure 5A22 logical-address classifier for the
documented 6/8/12-master-clock access classes and FastROM-capable windows. It does
not make the reference region table a SNES map, own `$420D`, or attach elapsed time
to CPU callbacks. Full bus timing therefore remains in progress.

## CPU interface and future fast path

`mipsnes2_bus_interface` remains the CPU-facing reference/test contract. The concrete
bus can create that interface, but the per-access callback is not frozen as the final
hot path. A future measured page/block lookup can sit behind or replace interpreter
dispatch without changing logical CPU addresses. PS2 and PSP may use separate tested
low-level fast paths where profiling supports them.

The current linear search across at most sixteen regions favors auditable foundation
behavior. It is not a performance claim. Do not extend it into a full map without
first defining mapping ownership, mirroring and side-effect rules.

## Deterministic unmapped policy

The bus stores one `open_value`. A mapped read updates it; any write updates it;
an unmapped read returns it. Writes to read-only or unmapped space do not mutate
memory. This supplies deterministic test-fixture behavior only. It is not a verified
model of SNES open bus, register-specific latches or CPU/PPU bus separation, so the
canonical open-bus task remains in progress.

## Verification

`tests/test_cpu_bus.c` covers direct read/write, read-only writes, MMIO routing,
24-bit normalization, unmapped behavior, overlap/range/region-limit rejection, DBR
data accesses, Direct Page bank-zero accesses, regular/long indirect pointer rules,
cross-bank data words/indexing and PBR-local instruction fetches. These tests validate
the reference bus foundation;
they do not satisfy full mapping/mirror/MMIO timing gates.

## Remaining design work

The full bus must map WRAM, cartridge ROM/SRAM, PPU, CPU I/O, APU ports, DMA and
enhancement hardware; model relevant read/write side effects and open-bus domains;
connect each real access and FastROM state to scheduler time; and benchmark page/block layouts on real PS2
and PSP. See [STATUS.md](../STATUS.md).
