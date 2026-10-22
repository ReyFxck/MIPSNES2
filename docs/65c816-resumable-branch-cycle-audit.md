# 65C816 resumable branch-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates the complete ten-opcode branch family from the
legacy-atomic remainder to the caller-owned peek/commit cursor:

- conditional BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ
  (`10/30/50/70/90/B0/D0/F0`);
- relative-always BRA (`80`);
- relative-long BRL (`82`).

Six shared phases expose the 8-bit or 16-bit operand reads, taken branch cycle,
emulation-only page-cross cycle and BRL internal cycle. Targets use signed offsets
from the post-operand PC, wrap within the current PBR and never change status flags.
There is no platform branch and no game-specific behavior.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially relative addressing, Table 5-7 entries 20 and 21 and notes 5 and 6.
They define the two-cycle not-taken path, one additional taken cycle, a second
additional cycle only for an emulation-mode page crossing, and BRL's fixed four
cycles.

XERJ `v1.0.0-rc.97` indexed only this MIPSNES2 checkout. Its dry run and index
covered 151 files, 54 code files and 1,162 live records with zero junk files. The
query localized the existing aggregate branch tests, branch audit, cursor tests and
shared CPU implementation. Feedback remained disabled. No source from another
emulator was consulted, indexed or copied.

## Deterministic coverage

- all eight flag conditions, each taken and not taken, with status preservation;
- exact read/internal descriptor order and aggregate cycle count for every path;
- BRA through the same 8-bit relative phases;
- BRL signed `+32767` and `-32768` extrema and fixed four-cycle sequence;
- positive/negative offsets, operand wrap at `FFFF/0000` and PBR preservation;
- the extra page-cross phase in emulation mode and its absence in native mode;
- no page-cross phase for a branch that is not taken;
- stale first-internal descriptor rejection before the page-cross internal cycle.

## Deliberate limits

The other 229 opcode remainders and reset remain legacy-atomic. Internal descriptors
do not claim a dummy read, VPA/VDA values or electrical address-bus state. This block
does not add mid-instruction `ABORT`, cursor cancellation, DMA/HDMA ownership or
scheduler event-budget propagation. The central timed-step bridge remains atomic.

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles first in the pinned PS2 toolchain and then in
the pinned PSP toolchain.

## Verification

- `make BUILD_DIR=build/branch-cycle-dev -j2 test` — passed all 14 C test
  executables, the host smoke test and repository checks.
- `make BUILD_DIR=build/branch-cycle-release CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' -j2 test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/branch-cycle-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with no ASan or UBSan
  diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-branch-cycle-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- Pinned PS2 toolchain `v2.0.0`, previously digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/ps2` passed with
  `mips64r5900el-ps2-elf-gcc 15.2.0`.
- Pinned PSP toolchain `v20261001`, previously digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/psp` passed with
  `psp-gcc 15.2.0` and produced `EBOOT.PBP`.
- Clang was unavailable in the local environment; no local Clang result is claimed.

The console builds prove compilation with the pinned SDKs; they do not claim runtime
behavior or performance on either console. No real SNES, PS2, PSP, PCSX2 or PPSSPP
runtime test and no performance measurement is claimed by this milestone. The exact
published implementation commit and tested tree are recorded after publication.
