# 65C816 resumable long-subroutine-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates absolute-long `JSL` (`22`) and `RTL` (`6B`) from the
explicit legacy-atomic remainder to the caller-owned peek/commit cursor. It is a
two-opcode long-call/return slice. Indirect calls, other stack instructions, reset
and the scheduler bridge remain outside this block.

The exposed `JSL` sequence is opcode read, target-low read, target-high read, PBR
write, one internal operation, target-bank read, return-address high write and
return-address low write. The exposed `RTL` sequence is opcode read, two distinct
internal operations, return-low read, return-high read and PBR read; the last read
also increments and commits PC. Consecutive internal descriptors carry distinct
tokens so an earlier descriptor cannot be committed twice.

Both instructions preserve WDC's linear multi-byte stack exception in emulation
mode: the individual accesses may leave page `01`, while `SH=01` is restored only
after the full three-byte transfer. Native mode uses ordinary 16-bit wrapping.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially Table 5-7 entries 4c and 22i. Entry 4c specifies eight-cycle `JSL` with
target-low/target-high, PBR push, internal position, target-bank read, PCH and PCL
ordering. Entry 22i specifies six-cycle `RTL` with two internal positions followed
by PCL, PCH and PBR stack reads. Section 7.1 identifies both opcodes as emulation
stack-boundary exceptions.

XERJ `v1.0.0-rc.97` was installed through the pinned digest-checked helper and used
only against this MIPSNES2 checkout. The dry run and first-party index covered 148
files and 1,141 live records with zero junk files; the query localized the existing
stack helpers, aggregate regressions and stack/control audit. XERJ was retrieval
support, not correctness evidence, and feedback remained disabled.

The cursor's `INTERNAL` operation deliberately carries no address-bus claim. The
WDC table informs its position and aggregate processor-cycle count, while dummy
data, VPA/VDA/MLB and electrical phase behavior remain unimplemented. No source
code from another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact descriptor order, addresses, values and eight-cycle total for `JSL`;
- PBR-before-target-bank access order and high-before-low return-address writes;
- exact descriptor order, linear stack reads, PBR restore, PC increment and
  six-cycle total for `RTL`;
- emulation-mode linear crossing below `0100` for `JSL` and above `01FF` for `RTL`,
  followed by restoration of `SH=01`;
- native-mode 16-bit stack wrapping at `0000`/`FFFF` through the public step driver;
- bank-local operand fetch wrapping for `JSL`;
- stale rejection between consecutive `RTL` internal descriptors with no effect;
- missing `JSL` write callback retains the established fail-stop behavior before
  operand fetch, without changing S or charging a partial instruction;
- existing aggregate CPU/bus regressions remain on the same public API.

## Deliberate limits

The other 245 opcode remainders and reset remain legacy-atomic. This block does not
claim dummy reads, signal-level activity, mid-instruction `ABORT`, DMA/HDMA ownership,
general cursor cancellation or event-budget propagation. The central timed-step
bridge still completes one CPU boundary before scheduler dispatch.

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles in the pinned PS2 and PSP CI toolchains.

## Verification

- `make BUILD_DIR=build/long-subroutine-dev -j2 test` — passed all 14 C test
  executables, the host smoke test and repository checks.
- `make BUILD_DIR=build/long-subroutine-release CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' -j2 test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make
  BUILD_DIR=build/long-subroutine-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with no ASan or
  UBSan diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-long-subroutine-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- Pinned PS2 toolchain `v2.0.0`, digest verified by `tools/setup_toolchain.py`:
  `make -C platform/ps2` passed with `mips64r5900el-ps2-elf-gcc 15.2.0`.
- Pinned PSP toolchain `v20261001`, digest verified by
  `tools/setup_toolchain.py`: `make -C platform/psp` passed with
  `psp-gcc 15.2.0`.
- Clang was unavailable in the local environment; no local Clang result is claimed.

Published implementation commit `02b9e1ca86241c656fc8ed58d2f5c482ea15aaba` used exact
source tree `b2c13694fdd599b18b1bdb45a3d98e8e6daa2f12` for every verification
above. The console builds prove compilation with the pinned SDKs; they do not claim
runtime behavior or performance on either console.

No real SNES, PS2, PSP, PCSX2 or PPSSPP runtime test and no performance measurement
is claimed by this milestone.
