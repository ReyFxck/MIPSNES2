# 65C816 resumable jump-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates the complete five-opcode `JMP/JML` family from the
legacy-atomic remainder to the caller-owned peek/commit cursor:

- absolute `JMP` (`4C`);
- absolute-long `JMP`/`JML` (`5C`);
- absolute-indirect `JMP` (`6C`);
- absolute-indexed-indirect `JMP` (`7C`);
- absolute-indirect-long `JMP`/`JML` (`DC`).

The implementation shares seven jump phases in the portable core. Direct operands
are fetched from PBR. `6C` and `DC` read their wrapping pointer tables from bank
zero; `7C` adds X modulo 16 bits and reads its wrapping table from PBR. Only the
long forms replace PBR, and they do so when the final target byte commits.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially Table 5-7 entries 1b, 2a, 3a, 3b and 4b. Those entries define the
three-cycle absolute sequence, four-cycle absolute-long sequence, five-cycle
bank-zero indirect sequence and six-cycle indexed-indirect/indirect-long sequences.

XERJ `v1.0.0-rc.97` was installed through the pinned digest-checked helper and used
only against this MIPSNES2 checkout. The first-party dry run and index covered 150
files and 1,153 live records with zero junk files; the query localized the current
interpreter helpers, aggregate jump regressions and stack/control documentation.
XERJ was retrieval support, not correctness evidence, and feedback remained disabled.

The indexed form's WDC I/O position is represented as an address-free `INTERNAL`
descriptor. Dummy data, VPA/VDA and electrical phase behavior remain unimplemented.
No source code from another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact descriptor kind, logical address and order for all five opcodes;
- exact aggregate totals of three, four, five or six processor cycles;
- program-counter operand wrapping inside PBR for direct and long jumps;
- bank-zero 16-bit pointer wrapping for `6C` and three-byte `DC` pointers;
- X addition and 16-bit pointer wrapping inside PBR for `7C`;
- PBR preservation for `4C/6C/7C` and final-byte PBR commit for `5C/DC`;
- stale operand descriptor rejection without a repeated read or state change;
- the existing public-step aggregate regressions remain on the same cursor driver.

## Deliberate limits

The other 239 opcode remainders and reset remain legacy-atomic. This block does not
claim dummy reads, signal-level activity, mid-instruction `ABORT`, DMA/HDMA ownership,
general cursor cancellation or scheduler event-budget propagation. The central
timed-step bridge still completes one CPU boundary before scheduler dispatch.

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles first in the pinned PS2 toolchain and then in
the pinned PSP toolchain.

## Verification

- `make BUILD_DIR=build/jump-cycle-dev -j2 test` — passed all 14 C test executables,
  the host smoke test and repository checks.
- `make BUILD_DIR=build/jump-cycle-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2
  test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/jump-cycle-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with no ASan or UBSan
  diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-jump-cycle-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- Pinned PS2 toolchain `v2.0.0`, digest verified by `tools/setup_toolchain.py`:
  `make -C platform/ps2` passed with `mips64r5900el-ps2-elf-gcc 15.2.0`.
- Pinned PSP toolchain `v20261001`, digest verified by
  `tools/setup_toolchain.py`: `make -C platform/psp` passed with
  `psp-gcc 15.2.0`.
- Clang was unavailable in the local environment; no local Clang result is claimed.

The console builds prove compilation with the pinned SDKs; they do not claim runtime
behavior or performance on either console. No real SNES, PS2, PSP, PCSX2 or PPSSPP
runtime test and no performance measurement is claimed by this milestone.
