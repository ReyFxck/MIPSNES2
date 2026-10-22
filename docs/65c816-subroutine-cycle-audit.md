# 65C816 resumable subroutine-cycle audit

Date: 2026-10-21

## Scope

This milestone migrates absolute `JSR` (`20`) and `RTS` (`60`) from the explicit
legacy-atomic remainder to the caller-owned peek/commit cursor. It is deliberately a
two-opcode call/return slice: long calls/returns, indirect calls, other stack
instructions, reset and the scheduler bridge remain outside this block.

The exposed `JSR` sequence is opcode read, target-low read, target-high read, one
internal operation, return-address high write and return-address low write. The
exposed `RTS` sequence is opcode read, two distinct internal operations, return-low
read, return-high read and one final internal operation that increments and commits
PC. Internal descriptors carry distinct tokens where they are consecutive so a
descriptor from an earlier phase cannot be committed twice.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially Table 5-7 entries 1c and 22h. Entry 1c specifies six-cycle absolute JSR
with target-low/target-high, internal, PCH and PCL ordering. Entry 22h specifies
six-cycle RTS with two internal positions, PCL/PCH stack reads and a final internal
position. Sections 2.9-2.11 and caveat 7.1 provide PBR, PC and emulation-stack rules.

The cursor's `INTERNAL` operation deliberately carries no address-bus claim. The
table informs its position and aggregate processor-cycle count, while dummy data,
VPA/VDA/MLB and electrical phase behavior remain unimplemented. No source code from
another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact descriptor order, addresses, write values and six-cycle total for JSR;
- high-before-low saved return address and unchanged PBR;
- exact descriptor order, stack reads, PC increment and six-cycle total for RTS;
- emulation-mode `01xx` stack wrapping in both directions;
- native-mode 16-bit stack wrapping at `0000`/`FFFF` through the public step driver;
- bank-local operand fetch wrapping for JSR;
- stale rejection between consecutive RTS internal descriptors with no effect;
- missing JSR write callback retains the established fail-stop behavior before
  operand fetch, without changing S or charging a partial instruction;
- existing aggregate CPU/bus regressions remain on the same public API.

## Deliberate limits

The other 247 opcode remainders and reset remain legacy-atomic. This block does not
claim dummy reads, signal-level activity, mid-instruction `ABORT`, DMA/HDMA ownership,
general cursor cancellation or event-budget propagation. The central timed-step
bridge still completes one CPU boundary before scheduler dispatch.

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles in the pinned PS2 and PSP CI toolchains.

## Verification

- `make BUILD_DIR=build/subroutine-dev -j2 test` — passed all 14 C test executables,
  the host smoke test and repository checks.
- `make BUILD_DIR=build/subroutine-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2
  test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/subroutine-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with no ASan or UBSan
  diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-subroutine-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- `make -C platform/ps2` — did not reach compilation locally because `PS2SDK` is
  unset.
- `make -C platform/psp` — did not reach compilation locally because `psp-config`
  is unavailable.

Pinned PS2 and PSP cross-builds remain pending GitHub workflow execution for the
exact published head. The local SDK absence is not reported as a successful build.

No real SNES, PS2, PSP, PCSX2 or PPSSPP runtime test and no performance measurement
is claimed by this milestone.
