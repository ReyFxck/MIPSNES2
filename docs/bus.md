# Bus

## Scope and current state

The CPU emits logical 24-bit SNES addresses. Two platform-independent,
caller-owned implementations now serve different purposes:

- `mipsnes2_bus` is a small region-table fixture for isolated CPU tests. Regions
  provide direct caller-owned bytes or a callback MMIO slow path.
- `mipsnes2_snes_bus` is the first partial real SNES map. It owns no heap storage;
  its configuration receives caller-owned 128 KiB WRAM and separate PPU, APU,
  CPU-I/O, DMA and cartridge interfaces.

Reference-table mappings reject zero length, addresses beyond `0xFFFFFF`, wrap,
overlap and more than sixteen regions. Both implementations normalize every address
to 24 bits. Neither stores a native PS2/PSP pointer as a CPU-visible address.

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

## Implemented partial SNES map

| Logical range | Implemented ownership |
|---|---|
| `7E:0000-7F:FFFF` | Direct 128 KiB WRAM |
| `00-3F/80-BF:0000-1FFF` | Mirror of WRAM `00000-01FFF` |
| `00-3F/80-BF:2100-213F` | PPU callback route |
| `00-3F/80-BF:2140-217F` | APU-port callback route |
| `00-3F/80-BF:2180-2183` | WRAM data/address ports |
| `00-3F/80-BF:4000-421F` | CPU-I/O route, including optional register state; bus owns `$420D` |
| `00-3F/80-BF:4300-437F` | DMA callback route |
| Cartridge-visible ranges | Cartridge slot route; base LoROM/HiROM and Mode 25/35 ExHiROM subsets available |

The `$2180` data port auto-increments and wraps its 17-bit address; `$2181-$2183`
select that address. `$420D` write bit 0 controls the bus's FastROM state. Reserved
holes inside the B-bus/internal-I/O windows use the current deterministic open latch
instead of falling through to cartridge space. A cartridge read can explicitly
decline an address, preserving that latch. PPU, APU and DMA callbacks remain routes
only. The optional caller-owned CPU-I/O state implements `$4200`, `$4202-$420A`,
`$4210-$4212` and `$4214-$4217`; unimplemented CPU-I/O addresses continue through
the fallback route.

NMITIMEN stores only the documented NMI, H/V IRQ-mode and automatic-joypad-enable
bits. The compare registers retain nine bits. The non-interlaced H/V timing component
supplies separate VBlank, HBlank and delayed NMI-flag transitions; other external
code supplies auto-joypad-busy. The H/V component now supplies stable-register H/V
timer-match signals. RDNMI/TIMEUP expose and acknowledge their latches, while HVBJOY
reports current inputs. Reads combine implemented bits with the bus latch for
currently unused/open bits. The components expose a gated NMI edge and timer IRQ
level, but do not yet model exact mid-instruction compare-register writes, the narrow
TIMEUP read-at-match exception or within-instruction CPU delivery. Atomic-boundary
delivery exists through the interrupt-line coordinator.

`$4202/$4203` start unsigned 8-bit multiplication and `$4204-$4206` start unsigned
16-by-8 division. Results remain at their previous values until 8 or 16 processor
cycles complete, then update together in `$4214-$4217`; divide-by-zero produces
`FFFF` and the original dividend. The bus advances the unit once per completed CPU
access regardless of that address's 6/8/12 master-clock class, and the timing bridge
advances remaining aggregate internal cycles. Reads sample before their own cycle is
advanced, while writes advance an older job before a possible new trigger. Exact
busy-operation collisions and internal-cycle placement inside an instruction remain
unverified; the current deterministic retrigger policy replaces the pending job.

Every SNES-bus read/write updates access counters and charges the 5A22 6/8/12-master-
clock class selected by its normalized address and current FastROM state. This is
real per-meaningful-access accounting, but not complete CPU bus sequencing: the
interpreter does not yet emit all dummy accesses, internal-cycle positions or MLB.
WRAM refresh and DMA/HDMA bus ownership also remain unimplemented.

## CPU interface and future fast path

`mipsnes2_bus_interface` remains the CPU-facing reference/test contract. Both buses
can create that interface, but the per-access callback is not frozen as the final
hot path. A future measured page/block lookup can sit behind or replace interpreter
dispatch without changing logical CPU addresses. PS2 and PSP may use separate tested
low-level fast paths where profiling supports them.

The reference table's linear search across at most sixteen regions favors auditable
tests. The SNES bus uses direct range decisions. Neither is a performance claim;
benchmark a page/block map separately on real PS2 and PSP before replacing it.

## Deterministic unmapped policy

Each implementation stores one `open_value`. A read updates it to the returned byte;
a write updates it to the driven byte; an unmapped SNES-bus read returns it. The
region fixture additionally ignores writes to read-only/unmapped storage. This policy
is deterministic and testable but is not a verified model of SNES open-bus domains,
register-specific read masks or CPU/PPU bus separation. The open-bus task therefore
remains in progress.

## Verification

`tests/test_cpu_bus.c` covers direct read/write, read-only writes, MMIO routing,
24-bit normalization, unmapped behavior, overlap/range/region-limit rejection, DBR
data accesses, Direct Page bank-zero accesses, regular/long indirect pointer rules,
cross-bank data words/indexing and PBR-local instruction fetches. These tests validate
the reference bus foundation;
they do not satisfy the SNES map gates.

`tests/test_cpu_io.c` verifies register masks/storage, blank and timer inputs,
NMI gate transitions, IRQ latch/acknowledgement, open-bus composition and invalid
operations, including distinct VBlank and RDNMI/NMI-flag signals. It also covers
math operands/results, 8/16-cycle boundaries, old-result visibility and division by
zero.
`tests/test_snes_bus.c` verifies 128 KiB WRAM, both low-bank mirror groups, 24-bit
normalization, WRAM port address construction/increment/wrap, `$420D`, FastROM timing
boundaries, CPU-I/O state/fallback routing, all current MMIO routes, cartridge fallthrough, deterministic holes,
overflow handling, math-cycle advancement and timed CPU/scheduler steps. The
integration cases cover an executed math STA/NOP/LDA sequence, a slow
NOP, mixed FastROM-fetch/slow-WRAM LDA, an event inside an atomic instruction, and
emulation-mode NMI entry. These tests do not prove the missing peripheral behavior,
cartridge mapping or cycle-level event placement.

## Remaining design work

The full bus must add broader cartridge boards, unofficial ExLoROM only if justified,
implemented peripheral registers,
enhancement windows, accurate read/write side effects and open-bus domains; expose
every relevant CPU dummy/internal cycle; add WRAM refresh and DMA/HDMA arbitration;
and benchmark page/block layouts on real PS2 and PSP. See [STATUS.md](../STATUS.md).

## Sources

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
  chapter 15 and internal pages 2-28-1 through 2-28-2 for CPU math operands,
  results, division by zero and 8/16-machine-cycle completion, inspected 2026-10-20;
- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
  pinned at repository revision `eeb98f1e85b7db6574ca0804455819ec81117198`;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), memory,
  WRAM-port, MEMSEL, PPU-interrupt, CPU-clock and `SNES Maths Multiply/Divide`
  sections, inspected 2026-10-20;
- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  processor bus-cycle and aggregate instruction-cycle reference.

No emulator implementation was used.
