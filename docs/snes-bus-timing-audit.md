# Partial SNES Bus and CPU Timing Integration Audit

Date: 2026-10-17

## Scope

This milestone adds the first partial real SNES logical bus and connects meaningful
65C816 accesses to scheduler time:

- caller-owned, heap-free 128 KiB WRAM;
- banks `7E-7F` and low 8 KiB mirrors in `00-3F/80-BF`;
- `$2180-$2183` WRAM data/address ports with 17-bit increment/wrap;
- distinct callback routes for PPU, APU ports, CPU I/O, DMA and cartridge space;
- bus-owned `$420D` bit 0 FastROM selection;
- deterministic open/unmapped fallback;
- 24-bit address normalization and per-access 6/8/12-master-clock accounting;
- an atomic CPU-step bridge that advances the central scheduler;
- host tests and PS2/PSP cross-build coverage of the new shared C99 files.

The change does not implement cartridge mappers/SRAM, PPU, APU, CPU-I/O registers,
DMA/HDMA, enhancement chips or a runnable SNES machine.

## Sources and independent implementation

The code is an original MIPSNES2 implementation. The map and access-speed boundaries
were derived from:

- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
  pinned at repository revision `eeb98f1e85b7db6574ca0804455819ec81117198`;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), memory,
  WRAM-port, MEMSEL and CPU-clock sections, inspected 2026-10-17;
- [WDC W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  for processor bus/aggregate-cycle behavior.

No emulator implementation or external ROM was used.

## Timing contract and intentional approximation

The SNES bus charges every read/write callback at the access speed selected by its
logical address and current FastROM state. `mipsnes2_cpu_timed_step` drains events
already due at the current boundary, executes one CPU instruction/interrupt boundary,
then computes:

```text
remaining internal cycles = aggregate processor cycles - observed bus accesses
```

Those remaining cycles are provisionally charged at six master clocks. The scheduler
then runs to the resulting end timestamp. Events scheduled inside the elapsed interval
retain their exact timestamps but are dispatched after the atomic CPU step and can
affect only the next CPU boundary.

This approximation is explicit. The interpreter does not yet emit all dummy accesses,
the exact position of internal cycles or MLB state. A cycle that is actually a dummy
access may therefore have a different address speed than the provisional six-clock
charge. WRAM refresh and DMA/HDMA arbitration are also absent. The broad CPU timing
and event-horizon tasks remain in progress until those limitations are resolved.

## Host verification

The following commands passed before the review commit:

```text
make clean && make test
make BUILD_DIR=build/release clean
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize clean
ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
git diff --check
python3 tools/check_repository.py
```

Meaningful output included:

```text
PASS: required files, license, identity, links, checkboxes, core boundaries and action pins
PASS: logical address and logging contracts (not SNES emulation)
PASS: logical bus and all 256 65C816 opcode values
PASS: deterministic scheduler and 5A22 timing foundation
PASS: SNES WRAM/MMIO routing and timed CPU step foundation
PASS: host banner and unsupported argument rejection
```

GCC completed the default, release and ASan+UBSan suites. Leak detection alone was
disabled for the existing local environment restriction. Clang was unavailable.
Local console attempts stopped before compilation: PS2SDK was unset and `psp-config`
was unavailable. GitHub's pinned PS2/PSP workflows remain the console compiler gate;
their results are recorded only after they execute.

## Test coverage

`tests/test_snes_bus.c` verifies:

- configuration rejection, interface ownership and caller-owned WRAM;
- direct WRAM, both low-bank mirror groups and 24-bit normalization;
- WRAM port address formation, auto-increment and 17-bit wrap;
- FastROM enable/disable and representative 6/8/12-clock boundaries;
- all current MMIO callback routes, cartridge fallthrough and reserved holes;
- open-latch behavior and timing-counter overflow;
- slow-ROM NOP and mixed FastROM-fetch/slow-WRAM LDA elapsed clocks;
- scheduler delivery inside an atomic instruction and NMI at the next boundary;
- emulation-mode NMI stack/vector access count and aggregate elapsed clocks;
- mismatched/invalid timing-bridge arguments.

These synthetic tests establish the implemented contract only. They do not establish
complete SNES mapping, cycle-perfect CPU timing, console runtime or compatibility.

## GitHub Actions

CI run URLs and exact commit evidence are added only after the Host, PS2 and PSP
workflows finish. Workflow files or local builds alone are not CI evidence.

## Runtime and performance

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No benchmark or
full-speed claim is made. RFAuds2 remains future PS2-only output infrastructure and
is not referenced by the core or this timing path.
