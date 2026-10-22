# 65C816 resumable indexed-subroutine-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates indexed-indirect `JSR` (`FC`) from the explicit
legacy-atomic remainder to the caller-owned peek/commit cursor. It is the final
subroutine-call opcode still using the atomic remainder. Other stack instructions,
reset and the scheduler bridge remain outside this block.

The exposed sequence is opcode read, address-low read, return-address PCH write,
return-address PCL write, address-high read, one internal operation, pointer-low
read and pointer-high read. The operand and pointer fetches stay in PBR, including
16-bit wrapping from `FFFF` to `0000`; PBR itself is unchanged.

The two stack writes preserve WDC's linear multi-byte exception in emulation mode:
an initial `S=0100` writes at `0100` then `00FF`, and `SH=01` is restored after the
complete transfer. Native mode uses ordinary 16-bit wrapping.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially Table 5-7 entry 2b and section 7.1. Entry 2b specifies the eight-cycle
order above; section 7.1 identifies `JSR (a,X)` as an emulation stack-boundary
exception.

XERJ `v1.0.0-rc.97` was installed through the pinned digest-checked helper and used
only against this MIPSNES2 checkout. The first-party query localized the existing
stack helpers, aggregate bus regressions and stack/control documentation. XERJ was
retrieval support, not correctness evidence, and feedback remained disabled.

The cursor's `INTERNAL` operation deliberately carries no address-bus claim. The
WDC table informs its position and aggregate processor-cycle count, while dummy
data, VPA/VDA/MLB and electrical phase behavior remain unimplemented. No source
code from another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact descriptor order, logical addresses, values and eight-cycle total;
- PCH/PCL writes before the high operand read, matching WDC Table 5-7 entry 2b;
- return address is the address of the final operand byte;
- emulation-mode linear stack crossing below `0100`, followed by `SH=01` restore;
- native-mode 16-bit stack wrapping at `0000`/`FFFF` through the public step driver;
- bank-local operand-PC and indirect-pointer wrapping while PBR remains unchanged;
- stale write-descriptor rejection after the first stack write with no second effect;
- missing write callback retains fail-stop behavior after opcode fetch and before
  operand or stack effects;
- existing aggregate CPU/bus regressions remain on the same public API.

## Deliberate limits

The other 244 opcode remainders and reset remain legacy-atomic. This block does not
claim dummy reads, signal-level activity, mid-instruction `ABORT`, DMA/HDMA ownership,
general cursor cancellation or event-budget propagation. The central timed-step
bridge still completes one CPU boundary before scheduler dispatch.

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles first in the pinned PS2 toolchain and then in
the pinned PSP toolchain.

## Verification

- `make BUILD_DIR=build/indexed-subroutine-dev -j2 test` — passed all 14 C test
  executables, the host smoke test and repository checks.
- `make BUILD_DIR=build/indexed-subroutine-release CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' -j2 test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make
  BUILD_DIR=build/indexed-subroutine-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with no ASan or
  UBSan diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-indexed-subroutine-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- Pinned PS2 toolchain `v2.0.0`, digest verified by `tools/setup_toolchain.py`:
  `make -C platform/ps2` passed with `mips64r5900el-ps2-elf-gcc 15.2.0`.
- Pinned PSP toolchain `v20261001`, digest verified by
  `tools/setup_toolchain.py`: `make -C platform/psp` passed with
  `psp-gcc 15.2.0`.
- Clang was unavailable in the local environment; no local Clang result is claimed.

Published implementation commit `b574c38b30979b2b30ad6fbcabfeedc46df54d01` used
exact source tree `e596e1b0565412c6723e0da162b54feef57fda15` for every
verification above.

The console builds prove compilation with the pinned SDKs; they do not claim
runtime behavior or performance on either console. No real SNES, PS2, PSP, PCSX2
or PPSSPP runtime test and no performance measurement is claimed by this milestone.
