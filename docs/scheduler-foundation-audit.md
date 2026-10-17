# Scheduler and 5A22 Timing Foundation Audit

Date: 2026-10-17

## Scope

This milestone adds the first real central scheduler foundation:

- caller-owned, heap-free storage for 64 events;
- unsigned 64-bit SNES master-clock timestamps;
- deterministic timestamp, explicit-priority and insertion-token ordering;
- cancellation, same-time callback insertion and next-event horizons;
- bounded dispatch and recursive-run rejection;
- exact positive rational clock conversion with carried remainder;
- a pure Ricoh 5A22 logical-address classifier for 6/8/12 master clocks;
- deterministic host fixtures and scheduled IRQ/NMI delivery to the CPU boundary.

The PS2 and PSP scaffolds compile the new shared files. No platform API enters the
core. This change does not implement PPU/APU/DMA, the SNES memory map, CPU bus-cycle
tracing, WRAM refresh, FastROM register ownership or a runnable machine.

## Sources and independent implementation

The scheduler design is original MIPSNES2 C99 code. SNES-specific access expectations
come from the independent [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm),
sections `SNES Timing Oscillators` and `CPU Clock Cycles / SNES Memory Speed`, inspected
2026-10-17. These sections document SNES master clocks, internal 6-clock cycles and
logical memory regions using 6, 8 or 12 master clocks, including FastROM-capable
upper ROM windows.

The [WDC W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf)
supplies the processor-cycle and bus-signal reference. WDC does not define the
SNES-specific 5A22 address decoder, so the two source responsibilities remain
separate. No emulator source or external test ROM was used.

## Verification boundary

`tests/test_scheduler.c` verifies queue order, cancellation, capacity, time and token
errors, callback insertion, dispatch budgets, deterministic replay, fractional clock
carry, chunked/one-shot equality, overflow, 5A22 timing boundaries and scheduled
NMI/IRQ delivery to a WAI-state CPU. These tests validate the implemented foundation.

The CPU still exposes aggregate W65C816 instruction cycles rather than every program,
data, dummy and internal cycle. Multiplying that aggregate by a fixed divider would
be wrong for mixed-speed accesses. Per-access 5A22 timing, refresh stalls, DMA/HDMA
arbitration and hardware subsystem tie priorities remain unchecked.

## Local verification

The following commands passed in the milestone worktree:

```text
make clean && make test
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
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
PASS: host banner and unsupported argument rejection
```

AddressSanitizer and UndefinedBehaviorSanitizer remained enabled; only leak detection
was disabled for the already documented local `/proc`/ptrace restriction. Local
Clang was unavailable. Local PS2 and PSP attempts stopped before compilation because
`PS2SDK` and `psp-config` were unavailable respectively. GitHub's pinned cross-builds
remain the console compiler gate.

## GitHub Actions

GitHub's pinned workflows passed for implementation commit
`e1dc763ad81601bdcfbc44885eee08b2f770dd0d`:

- [Host CI run 39050212756](https://github.com/ReyFxck/MIPSNES2/actions/runs/39050212756)
  — GCC and Clang release/sanitizer jobs succeeded and staged artifacts;
- [PS2 CI run 39050212797](https://github.com/ReyFxck/MIPSNES2/actions/runs/39050212797)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded;
- [PSP CI run 39050212757](https://github.com/ReyFxck/MIPSNES2/actions/runs/39050212757)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded.

The final documentation-evidence commit must also pass applicable workflows before
merge. Cross-compilation is not console execution.

## Runtime and performance

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No performance
measurement or full-speed claim is made.
