# CPU access-timing experiment post-mortem

Date: 2026-10-20

## Decision

The synchronous clock-sink experiment merged in PR #46 was removed before further
development. It dispatched scheduler events from SNES-bus read/write callbacks while
`mipsnes2_cpu_step` was still evaluating an instruction. That placement was unsafe
with the current instruction-atomic CPU API.

The project has returned to the earlier atomic bridge: events due at the starting
boundary run first, the CPU executes one complete instruction or interrupt entry,
aggregate access/internal clocks are charged, and the scheduler/H/V domain advances
afterward. This remains a documented approximation. It is safer because no routine
scheduler result can strand half of an instruction in caller-owned state.

## Failure found during review

The clock sink advanced the scheduler before each emitted read/write effect. If the
event budget was exhausted or a scheduler callback failed after the CPU had started,
the bus returned its open value or suppressed the write, but the C instruction
evaluator continued. PC, registers, earlier memory/MMIO effects and scheduler events
could already be committed. The bridge then marked the machine as requiring a full
checkpoint restore.

Event-budget exhaustion is an ordinary bounded-execution result in the scheduler
contract. Converting it into unrecoverable, partially committed CPU state is not an
acceptable foundation. Tests in that change accurately asserted the fail-stop
behavior but did not make the architecture safe.

The same experiment blocked callbacks from reentering the bus, including events at
the current CPU boundary. That would conflict with future DMA/HDMA ownership and
other scheduled hardware activity. A reentrancy guard alone cannot substitute for
explicit bus arbitration and CPU suspension.

## Required successor contract

Meaningful-access event dispatch may be reintroduced only after the CPU can stop
before an externally visible operation and resume it exactly once. The design must:

- represent the pending instruction/cycle or micro-operation in caller-owned state;
- yield before a bus effect when the scheduler horizon is reached;
- retry safely after event-budget exhaustion without repeating prior effects;
- preserve byte order, stack order, PC/PBR changes and interrupt sequencing;
- expose dummy reads/writes and internal-cycle positions needed by SNES software;
- allow future DMA/HDMA bus ownership without recursive CPU-bus calls;
- keep H/V, scheduler, CPU-I/O and CPU state aligned on every return path;
- test cancellation/error paths as well as successful execution;
- remain platform-neutral and use only 24-bit SNES logical addresses.

The exact electrical sampling phase of 5A22 MMIO, TIMEUP's narrow read exception,
WRIO-to-SLHV latency, WRAM refresh and simultaneous subsystem priorities remain
hardware-research tasks. A resumable software contract does not by itself settle
those hardware questions.

## Scope preserved

The experiment added no opcode. Removing it does not change the verified 256-opcode
decode coverage or the existing instruction tests. The 5A22 6/8/12-clock access
classifier, aggregate bus accounting, atomic CPU/scheduler bridge, H/V fanout and
boundary interrupt handoff remain. PPU rendering, SPC700, S-DSP, DMA/HDMA,
enhancement chips and platform audio remain unimplemented.

No emulator implementation was consulted. No platform API, assembly, game-specific
condition or RFAuds2 runtime dependency was introduced.

## Verification

The correcting source/build/test paths match the reviewed tree immediately before
PR #46. Local GCC 13.3.0 verification from a clean build directory passed:

- `make BUILD_DIR=build/review-default -j2 test`;
- `make BUILD_DIR=build/review-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2 test`;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/review-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test`;
- `python3 tools/check_repository.py`;
- `git diff --check`.

Each host configuration ran all 13 C test executables, the host smoke test and the
repository checker. ASan and UBSan remained enabled; leak detection alone was
disabled for the documented managed-runtime `/proc` restriction. Local Clang was
unavailable. The local PS2 build was unavailable because `PS2SDK` was unset; the
local PSP build was unavailable because `psp-config` was absent. GitHub Host, PS2
and PSP workflow results must pass before merge and are recorded in the correcting
pull request.

No real SNES, PS2, PSP, PCSX2 or PPSSPP validation was performed. No performance or
compatibility result is claimed. LICENSE remains byte-for-byte GPL-3.0 with SHA-256
`3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

## Status accounting

The three completed rows introduced for per-access clock fanout, shared in-flight
budgeting and fail-stop cleanup were removed. One unchecked prerequisite now tracks
the resumable CPU cycle/micro-operation contract. Atomic timing rows remain complete
only for their previously verified aggregate scope.
