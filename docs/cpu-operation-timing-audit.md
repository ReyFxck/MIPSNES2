# CPU-operation timing bridge audit — 2026-10-23

## Scope and decision

The platform-neutral `core/scheduler/cpu_timing.[ch]` now has an opt-in,
caller-owned state connecting the complete 65C816 operation cursor to the 5A22
access classifier, deterministic scheduler and optional H/V/interrupt domains.
The existing `mipsnes2_cpu_timed_step*` family remains instruction-atomic and
source-compatible. No PS2/PSP backend code or game-specific path changed.

The new `mipsnes2_cpu_timed_resume*` family executes one instruction or an
explicitly requested RESET sequence. A zero-time CONTROL commits immediately.
READ/WRITE latch their 6/8/12-master-clock cost using the pre-operation FastROM
state; INTERNAL costs one processor cycle and six master clocks regardless of its
phase-token `value`. A timed descriptor remains in flight while the coordinator
advances through callbacks, requested horizons and event-budget retries. Only at
its original endpoint does the CPU cursor commit the operation, followed by the
SNES-bus internal-cycle charge when applicable. An event exactly at that endpoint
is dispatched before the CPU/bus effect. No speculative MMIO read or write occurs.

The report is cumulative for the active logical sequence. Its `target_time` is
the scheduler timestamp; `master_clocks` is the bus counter delta, so during an
in-flight pause the two need not match. Previously dispatched callbacks are not
rolled back when the event budget runs out; the pending CPU operation has not
committed and is retryable with a fresh budget. The state binds to its CPU, bus,
scheduler, optional H/V and interrupt-line objects until completion. Counter,
FastROM and CPU-snapshot divergence while pending is rejected before its bus effect.
Callbacks must not mutate the active CPU/cursor or bus timing/FastROM state; device
and interrupt-line changes are allowed. Full cancellation/checkpoint recovery is
not supported.

RESET is a distinct requested mode: its zero-time control transition is followed
by two timed vector reads at `00:FFFC-D`. Unlike instruction reads, these do not
increment `instruction_cycles`, which remains zero after the high-vector commit.
This does not model electrical RESET assertion, dummy/internal reset cycles or
hardware power-state retention.

The interrupt-aware variant samples CPU-I/O edges while time advances, retaining
them in `mipsnes2_interrupt_lines` without mutating the active CPU cursor. It
drives CPU inputs before a new cursor and after instruction completion; NMI
generated mid-operation affects the following boundary, not the active one.

## Deterministic evidence

`tests/test_snes_bus.c` now checks:

- an event inside an 8-clock fetch sees no partial CPU/bus effect; resume finishes
  NOP at clock 14 rather than restarting the fetch at the event timestamp;
- an event tied with the fetch endpoint runs before the read commits;
- slicing each master clock of fetch plus INTERNAL yields the same PC, counters and
  clock 14 as one-shot execution;
- two same-timestamp events with budget one leave a `$2180` write uncommitted,
  scheduler at clock 27, the WRAM port unchanged and a resumable descriptor; a
  second call commits exactly one write at clock 30, matching the one-shot run;
- FastROM fetch costs 6 and ordinary fetch 8, slow `$4000` fetch 12; their NOP
  internal operations each cost 6. A `STA $420D` charges its own access using the
  old speed and changes the speed of the following fetch;
- explicit RESET pauses between control/vector reads, then reaches PC `1234` at
  clock 16 with two accesses and zero processor cycles;
- H/V and scheduler stay aligned on a zero-budget event failure; callbacks at
  timestamp zero are drained before interrupt selection; NMI wins over IRQ;
- overflow of access, bus-clock and scheduler-clock counters is rejected before
  a read effect; a mutated CPU descriptor or register is stale.

The checked source remains under `core/`, without platform headers, heap
allocation or title-specific conditions. XERJ is an optional host-only first-party
indexer, not part of the emulator or validation of hardware behavior. No source
from another emulator was used for this implementation.

## Validation record

Local validation on this revision:

- `make test` — PASS (GCC C99, warnings as errors; all host suites and repository
  checks);
- `make BUILD_DIR=build/host-release CFLAGS='-O3 -DNDEBUG' test` — PASS;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' test` — PASS; the initial run without
  `detect_leaks=0` failed because LeakSanitizer could not inspect `/proc` in this
  managed runtime, not because an emulator assertion failed;
- pinned SHA-256-verified PS2 toolchain `v2.0.0` and PS2SDK cross-build via
  `make -C platform/ps2` — PASS, `mipsnes2-ps2.elf` generated;
- pinned SHA-256-verified PSP toolchain `v20261001` and PSPSDK cross-build via
  `make -C platform/psp` — PASS, `EBOOT.PBP` generated;
- pinned XERJ `v1.0.0-rc.97`, feedback disabled, loopback-only, first-party
  `autoindex` dry-run and index under `mipsnes2` — PASS: 159 files, 1,307 live
  records, 54/54 code files indexed, zero junked; a search found the new API in
  core and its deterministic test. No external emulator-source corpus was indexed.

GitHub Actions for implementation commit
[`26db500`](https://github.com/ReyFxck/MIPSNES2/commit/26db500375012bb46994f15035302010e4bb7704)
were inspected separately:

- [Host CI run 40006566910](https://github.com/ReyFxck/MIPSNES2/actions/runs/40006566910)
  — SUCCESS, GCC and Clang release and sanitizer steps; two nonempty development
  artifacts uploaded;
- [PS2 CI run 40006566929](https://github.com/ReyFxck/MIPSNES2/actions/runs/40006566929)
  — SUCCESS, pinned-toolchain cross-build, checksum staging and nonempty artifact;
- [PSP CI run 40006566865](https://github.com/ReyFxck/MIPSNES2/actions/runs/40006566865)
  — SUCCESS, pinned-toolchain cross-build, checksum staging and nonempty artifact.

Cross-compilation and CI artifacts are not real-console execution or performance
measurements. No compatibility result is claimed.

## Explicit limits and next milestone

This is an operation-endpoint logical scheduling contract, not cycle-exact SNES
electrical behavior. Dummy reads/writes, exact internal phase placement, WRAM
refresh, DMA/HDMA ownership, MLB/RDY/BE/VPA/VDA, final subsystem tie priority,
exact IRQ/NMI sampling, signal-level RESET and in-flight ABORT remain open. The
next primary milestone is **mid-instruction ABORT at cursor operation boundaries**:
latched restart address, suppression of an uncommitted write, priority against
NMI/IRQ and deterministic resume/error tests, grounded in manufacturer evidence.
