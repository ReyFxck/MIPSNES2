# Ricoh 5A22 Math-Register Audit

Date: 2026-10-20

## Scope

This milestone adds original C99 state and behavior for `$4202-$4206` and
`$4214-$4217`. It implements unsigned 8x8 multiplication, unsigned 16-by-8 division,
division by zero, old-result visibility while a job is pending and atomic result
commit after 8 or 16 processor cycles. The state is caller-owned and platform-free.

The partial SNES bus advances an existing math job after a read samples data and
before a write is latched. A trigger write therefore begins with the full documented
latency. Aggregate internal instruction cycles use the existing timing bridge. Math
latency is counted in processor cycles and is independent of 6/8/12-master-clock
address classes.

## Evidence

- Nintendo SNES Development Manual Book I, chapter 15 and internal pages 2-28-1
  through 2-28-2, inspected 2026-10-20: manufacturer definitions for unsigned
  operands/results, division by zero and 8/16-machine-cycle completion;
- fullsnes, `SNES Maths Multiply/Divide` and `Timing Notes`, inspected 2026-10-20:
  cross-check for multiplication's RDDIV side effect and CPU-clock-domain timing;
- SNESdev Wiki `Multiplication` and `Division`, search results inspected 2026-10-20:
  register/timing cross-check and explicit warning not to start a new operation while
  one is active;
- no emulator source or implementation was consulted or copied.

## Deterministic tests

`tests/test_cpu_io.c` covers initialization, operand storage, old-result visibility,
one-cycle-before and exact completion, all four result bytes, multiplication's RDDIV
value, ordinary division, division by zero, the current deterministic retrigger
policy, large cycle advances, null/zero advances, write-only open-bus reads and
ignored writes to read-only results.

`tests/test_snes_bus.c` covers system-bank mirrors, a result read on the final pending
cycle, bus and aggregate-internal advancement, overflow without partial math advance,
and an executed `LDA/STA`, four-`NOP`, `LDA` sequence through the CPU timing bridge.

## Deliberate limits

- Writes that start another operation while one is pending deterministically replace
  the pending job. Hardware references warn software not to do this; exact collision
  results are not claimed and the broad STATUS task remains in progress.
- The CPU exposes aggregate instruction cycles and meaningful bus accesses, not every
  internal cycle position. Internal cycles are advanced after the atomic CPU step,
  so exact observation inside instructions with earlier internal cycles is pending.
- Initial math-result storage is deterministic zero from component initialization;
  this is not a claim about unspecified power-on silicon contents.
- DMA/HDMA writes, WRAM refresh interaction and real-console revision differences are
  outside this milestone.

## Verification

- `make clean && make test` — passed with local GCC on 2026-10-20;
- `make BUILD_DIR=build/release-math CC=gcc CFLAGS='-O2 -g -DNDEBUG'
  clean test` — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-math CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' test` — passed ASan+UBSan;
- `python3 tools/check_repository.py` and `git diff --check` — passed;
- GPL-3.0 `LICENSE` SHA-256 remained
  `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

The first sanitizer execution with LeakSanitizer enabled compiled successfully but
the runtime stopped because it could not inspect `/proc/.../task` in this managed
environment. The documented local retry disabled leak detection only; ASan and UBSan
remained enabled and passed. GitHub's sanitizer job remains the full remote gate.

Local Clang, PS2SDK and PSPSDK executables were unavailable. GitHub Host/PS2/PSP
workflow evidence will be appended only after those exact runs complete.
- No emulator, PCSX2, PPSSPP, real PS2 or real PSP runtime was executed.
