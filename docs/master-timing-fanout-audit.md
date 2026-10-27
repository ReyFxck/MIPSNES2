# Shared Master-Clock Fanout Audit

Date: 2026-10-19

## Scope

This milestone connects the existing deterministic event scheduler and non-interlaced
H/V component through one explicit SNES master-clock domain. It also adds an optional
CPU timing entry point that advances both components from the elapsed master clocks
of one atomic interpreter boundary.

Implemented behavior:

- the scheduler and H/V elapsed timestamp must agree before advancement;
- H/V state advances to every pending scheduler-event timestamp before callbacks at
  that timestamp are dispatched;
- a same-time callback consequently observes already-applied blank, NMI-flag and
  timer-IRQ transitions;
- a caller-provided dispatch budget covers the whole requested interval;
- budget exhaustion and same-time callback insertion leave the two domains aligned
  at the stopped timestamp;
- each H/V slice is committed only after successful advancement, preventing an H/V
  overflow from partially advancing that domain alone;
- time regression, domain mismatch, invalid H/V state, reentrant scheduler use and
  scheduler errors remain explicit;
- `mipsnes2_cpu_timed_step_hv` requires SNES-bus CPU-I/O state and feeds an atomic
  CPU boundary's clock delta through the shared coordinator;
- the existing scheduler-only CPU timing entry point remains intact for focused
  tests and partially constructed callers.

All state is caller-owned. The implementation uses logical SNES state and master
clocks only, with no heap, host clock, platform API or target physical pointer.

## Deliberate limits

This is not exact CPU/subsystem arbitration:

- one 65C816 instruction or interrupt entry remains atomic;
- an H/V transition whose timestamp falls inside an instruction is processed after
  that instruction and can affect only the following CPU boundary;
- the coordinator does not automatically transfer the CPU-I/O NMI edge or IRQ level
  into `mipsnes2_cpu` inputs;
- H/V-before-general-callback ordering is deterministic and testable but provisional;
  exact simultaneous hardware priority needs subsystem evidence;
- dummy/internal CPU cycle placement, WRAM refresh and DMA/HDMA bus ownership remain
  absent;
- interlace, exact timer-register write windows, repeated reprogrammed matches and
  TIMEUP's narrow read-at-match exception remain absent;
- This historical audit did not integrate PPU, SPC700, DSP or enhancement clock
  domains. SPC700/timer/DSP cadence was added later in the
  [APU clock/DSP register audit](apu-clock-dsp-register-audit.md); exact phases remain.
- long regional soak tests and timestamp renormalization remain future work.

No limit above is hidden by a completion claim.

## Tests

`tests/test_master_timing.c` verifies:

- argument validation, time regression and deliberate domain mismatch;
- event-free advancement and one-shot/chunked equivalence;
- H-timer assertion before a general callback at the same timestamp;
- zero-budget rejection without time movement;
- same-time insertion with budget exhaustion and resumable aligned state;
- reentrant-run and invalid-H/V rejection without domain divergence;
- frame-counter overflow rejection without partial H/V or CPU-I/O mutation.

`tests/test_snes_bus.c` additionally executes four NOPs through the H/V-aware CPU
bridge. Each contributes fourteen master clocks, scheduler/H/V clocks remain equal,
and the H-timer latch asserts when the fourth step crosses master clock 54. The test
also proves the CPU IRQ input remains untouched, documenting the still-missing handoff.

Verified locally on 2026-10-19 with GCC 13.3.0:

- `make clean && make test` — passed;
- `make BUILD_DIR=build/release-fanout CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean
  test` — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-fanout CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed;
- `python3 tools/check_repository.py` and `git diff --check` are included in the
  audit before publication;
- `sha256sum LICENSE` remained
  `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

LeakSanitizer is disabled locally because this environment cannot inspect the
required `/proc` state. Local Clang, PS2SDK and PSPSDK are unavailable.

For implementation commit `f0c863d16c31140f8fd23352c9e68bffaf534c24`, GitHub
Actions passed:

- Host CI run `39341652070`: GCC and Clang release, sanitizer and artifact jobs;
- PS2 CI run `39341652061`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps;
- PSP CI run `39341652091`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps.

No emulator or real PS2/PSP executed the code. Cross-compilation proves that the
shared source remains accepted by the pinned scaffold toolchains, not scheduler/H/V
runtime correctness or performance on target hardware.

## Evidence basis

This milestone composes already sourced timing behavior rather than adding a new
hardware formula. The relevant source trail remains:

- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  for processor-cycle and interrupt-boundary behavior;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), sections
  `SNES Timing Oscillators`, `CPU Clock Cycles / SNES Memory Speed`, `SNES Timing H/V
  Counters` and `SNES Timing H/V Events`, inspected 2026-10-19;
- [Super Famicom Development Wiki: Timing](https://wiki.superfamicom.org/timing),
  `Interrupts`, inspected 2026-10-19.

These are manufacturer or independent hardware-research sources, not emulator
implementations. No emulator source or commercial ROM was consulted or copied.
