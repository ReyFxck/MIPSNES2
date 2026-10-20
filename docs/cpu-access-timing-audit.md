# Meaningful CPU access-clock audit

Date: 2026-10-20

## Scope and evidence boundary

No new opcode is added. The reference CPU still dispatches all 256 byte values and
is not a complete SNES CPU subsystem. The timing bridge now borrows a synchronous,
stack-owned clock sink in the SNES bus. Each emitted meaningful read/write advances
its 5A22 6/8/12-master-clock duration before taking effect. Optional H/V state and
the scheduler advance together. Untimed bus use remains accounting-only.

At an endpoint the chosen order is H/V transition, scheduler callback, CPU memory/
MMIO effect. This is an explicit provisional end-access policy, not a hardware claim
about the 5A22's electrical sampling phase, TIMEUP's read-at-match exception or the
measured WRIO-versus-SLHV latch delay. The previous start-of-instruction H/V snapshot
is superseded. Internal cycles not emitted by the CPU still form a 6-clock remainder
at the end. No dummy accesses, MLB, refresh, arbitration or complete microcycle
pause/resume protocol is introduced. Interrupt entry remains boundary-selected.

This is correctness-oriented reference infrastructure, not an optimization or
performance result. The per-access callback is not frozen as the final hot path.
No platform API, assembly, game hack, external ROM, copied implementation, PPU
rendering, APU, SNES DMA/HDMA, enhancement chip or RFAuds2 runtime is added.

## Sources and intentional policies

Re-inspected on 2026-10-20:

- [WDC W65C816S datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  sections 2.16 and 2.22-2.23, the page-28 General Timing Diagram and Table 5-7.
  The manufacturer distinguishes program/data bus transactions and their cycle
  ordering; it does not establish the SNES-specific address-speed decoder or all
  Ricoh MMIO phases. The existing callback order is retained.
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm),
  `CPU Clock Cycles / SNES Memory Speed` and `SNES Timing Oscillators`.
  Independent hardware research specifies the address-dependent divisors and
  six-master-clock internal class already used by MIPSNES2.

End-access sampling and callback-before-access ties are MIPSNES2's declared
foundation policies, not conclusions proven by these sources. Important remaining
hardware discrepancies are left unchecked. No emulator implementation was consulted.

## Failure and ownership contract

One caller budget covers events before execution, during every access and in the
internal remainder. The clock sink is detached on every returning path. Event/MMIO
callbacks must not reenter the same CPU bus/step or modify clock/accounting objects;
peripheral input setters remain allowed. External WRIO pin changes still require
immediate explicit latch synchronization at the event timestamp.

An in-flight clock failure, overflow or reentrant bus access latches a fail-stop
fault. A failed clock advance prevents the current access effect from starting;
subsequent accesses have no new bus side effects. Earlier memory/MMIO writes and
callbacks are not rolled back, including effects a violating MMIO callback committed
before recursive bus use was detected. The C
instruction evaluator may finish using the unchanged open byte after a failed read;
its registers/result are therefore not trustworthy even if `cpu_result` is OK.
`recovery_required` prohibits resuming. Restore a valid complete checkpoint or
reinitialize the complete machine; clearing the bus alone does not restore CPU or
device state. Later timed steps reject the latched fault without executing the CPU.

Reports distinguish completed bus-access/clock charges from actual scheduler elapsed
time and the last requested endpoint; a callback can commit partway through an
access that ultimately fails. Pre-CPU budget/validation errors without a bus fault
do not require machine recovery and can be corrected/retried under the scheduler
contract. Neither case is a resumable mid-instruction yield.

## Automated coverage

`tests/test_cpu_access_timing.c` adds:

- 24 LDA/STA trace combinations: emulation 8-bit, native 8/16-bit, slow/FastROM
  fetches, ordinary 6-clock and slow 12-clock MMIO, ordered byte values/addresses;
- every event tick 0-31 surrounding an absolute load and store, including endpoint
  ties, before-effect observations and changed input visibility;
- HBlank reads just before/at/after transition, H/V-before-callback ties, SLHV across
  a scanline boundary and WRIO capture at its emitted write endpoint;
- shared-budget failure before execution, during a data access and in the internal
  remainder, including a callback committed partway through a failed access;
- raw read/write/internal and nested-step reentry, sticky rejection, suppressed
  WRAM-port/math/open-bus effects, sink cleanup, bus/accounting/time overflows and
  retry after a pre-CPU validation failure.

Existing CPU/bus/math/interrupt/cartridge suites remain active. The earlier counter
LDA regression now expects H=7 after three eight-clock fetches plus a six-clock
register access, rather than H=0 at the instruction start.

## Verification record

Local GCC 13.3.0 commands from the repository root, all test suites exit 0:

- `make BUILD_DIR=build/access-default -j2 test`;
- `make BUILD_DIR=build/access-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2 test`;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/access-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test`.

Every suite runs all 14 C test executables, the host smoke script and repository
checker. ASan and UBSan remain active; leak detection alone is disabled for the
documented managed-runtime `/proc` restriction. Local Clang is unavailable.
`make -C platform/ps2` exits 2 (PS2SDK unset); `make -C platform/psp` exits 2
(`psp-config` absent). GitHub cross-build verification remains a pre-merge gate.
No real SNES, PS2, PSP, PCSX2 or PPSSPP validation has been performed. There are no
performance or compatibility measurements. `PERFORMANCE.md` and `COMPATIBILITY.md`
remain unchanged.

`python3 tools/check_repository.py` and `git diff --check` pass. The modified source
scan found no TODO/FIXME/placeholder, platform SDK/RFAuds2 reference, game-identity
hack or heap allocation. The CPU decoder itself is unchanged, including its
defensive unsupported-opcode result. LICENSE remains byte-for-byte GPL-3.0 with
SHA-256 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

An initial new-test build failed because two fixture calls used incorrect helper
names/signatures; those calls were corrected. A later incremental run encountered
an old `build/host/test-master-timing` file without execute permission. A fresh
build directory compiled every executable and passed all tests; no permission
override or test exclusion was used.

## STATUS changes and next gate

Three narrow verified rows are added in STATUS/Phase 4: meaningful-access clock
fanout; shared per-step event budget; in-flight fail-stop/reentrancy/sink cleanup.
No existing incomplete category is promoted or completed row demoted. Broad CPU
timing, event horizon, exact capture latency, dummy/internal sequencing, refresh,
arbitration and complete interpreter integration remain incomplete.

Primary next milestone: explicit 65C816 internal/dummy cycle sequencing foundation.
