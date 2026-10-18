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
| `00-3F/80-BF:4000-421F` | CPU-I/O route, except bus-owned `$420D` |
| `00-3F/80-BF:4300-437F` | DMA callback route |
| Cartridge-visible ranges | Cartridge slot route; base LoROM/HiROM and Mode 25/35 ExHiROM subsets available |

The `$2180` data port auto-increments and wraps its 17-bit address; `$2181-$2183`
select that address. `$420D` write bit 0 controls the bus's FastROM state. Reserved
holes inside the B-bus/internal-I/O windows use the current deterministic open latch
instead of falling through to cartridge space. A cartridge read can explicitly
decline an address, preserving that latch. Peripheral callbacks are routes only:
there is no PPU, APU, CPU-I/O or DMA implementation behind them yet.

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

`tests/test_snes_bus.c` verifies 128 KiB WRAM, both low-bank mirror groups, 24-bit
normalization, WRAM port address construction/increment/wrap, `$420D`, FastROM timing
boundaries, all current MMIO routes, cartridge fallthrough, deterministic holes,
overflow handling and timed CPU/scheduler steps. The integration cases cover a slow
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

- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
  pinned at repository revision `eeb98f1e85b7db6574ca0804455819ec81117198`;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), memory,
  WRAM-port, MEMSEL and CPU-clock sections, inspected 2026-10-17;
- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  processor bus-cycle and aggregate instruction-cycle reference.

No emulator implementation was used.
