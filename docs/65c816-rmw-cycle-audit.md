# 65C816 resumable read/modify/write-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates thirty-six shift, rotate and memory-modification opcodes
from the legacy-atomic remainder to the caller-owned peek/commit cursor:

- accumulator ASL/ROL/LSR/ROR (`0A/2A/4A/6A`);
- Direct, Direct-X, absolute and absolute-X ASL/ROL/LSR/ROR
  (`06/16/0E/1E`, `26/36/2E/3E`, `46/56/4E/5E`, `66/76/6E/7E`);
- memory DEC/INC (`C6/D6/CE/DE`, `E6/F6/EE/FE`);
- TSB/TRB (`04/0C`, `14/1C`);
- STZ (`64/74/9C/9E`).

Accumulator forms commit through one internal operation. Memory forms share phases
for operand fetch, Direct Page penalty, fixed indexed timing, width-selected reads,
one modification operation and low-before-high writes. STZ uses the same addressing
and write phases but intentionally performs no data read or modification phase.
Distinct internal descriptor values separate Direct penalty, indexing and data
modification, so an earlier internal descriptor cannot be replayed at the next phase.
The implementation is entirely in the shared core and adds no backend or game-specific
branch.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
particularly the opcode and processor-cycle tables, Direct Page rules, M-selected
width rules and Table 5-7 read/modify/write entries and notes. The existing aggregate
implementation and tests provide the project-local semantic oracle while the cursor
exposes those already-defined effects at resumable boundaries.

XERJ `v1.0.0-rc.97` indexed only this MIPSNES2 checkout. Its dry run and index
covered 154 files, all 54 detected code files and 1,214 live records with zero junk
files. Search localized the new RMW matrix and stale-internal regression; definition
lookup found `rmw_memory_operation`. Feedback remained disabled. No source from
another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact opcode-read/internal sequence for all four accumulator forms at both M widths;
- exact meaningful read, write and internal-operation order for all thirty-two memory
  opcodes at both M widths;
- Direct Page low-byte penalty and fixed Direct-X/absolute-X internal cycles;
- 8-bit and 16-bit data reads, low-before-high writes, Direct high-byte wrapping and
  linear absolute bank crossing through the existing semantic suite;
- ASL/LSR/ROL/ROR C/N/Z behavior and carry input, with accumulator B preservation;
- INC/DEC N/Z-only behavior, TSB/TRB original-value Z tests and STZ flag preservation;
- STZ's absence of memory reads and modify phase;
- missing-write-bus preflight for every memory opcode before operand or data access;
- stale Direct-penalty descriptor rejection at the following index phase without a
  memory or cycle effect;
- unchanged aggregate semantics and cycle totals across all 256 opcode values.

## Deliberate limits

The other 150 opcode remainders and reset remain legacy-atomic. Internal descriptors
do not claim a W65C816 dummy read/write, MLB signaling or electrical address-bus
state. This block does not add mid-instruction `ABORT`, cursor cancellation,
DMA/HDMA ownership or scheduler event-budget propagation. The central timed-step
bridge remains instruction-atomic.

## Completion criterion

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles first in the pinned PS2 toolchain and then in
the pinned PSP toolchain. Console compilation is not a runtime or performance claim.

## Verification

Published implementation commit `8c6d8281306c8e8047fdce92e27470664e312a6f`
used exact source tree `e34be6738739a301dcb840ab82d82505ff0c6dc4` for every
verification below.

- `make BUILD_DIR=build/rmw-cycle-debug -j2 test` — passed all 14 C test
  executables, the host smoke test and repository checks.
- `make BUILD_DIR=build/rmw-cycle-release CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' -j2 test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/rmw-cycle-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j1 test` — passed with no ASan or UBSan
  diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-rmw-cycle-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- Pinned PS2 toolchain `v2.0.0`, previously digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/ps2 clean all` passed with
  `mips64r5900el-ps2-elf-gcc 15.2.0` and produced `mipsnes2-ps2.elf`.
- Pinned PSP toolchain `v20261001`, previously digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/psp clean all` passed with
  `psp-gcc 15.2.0` and produced `EBOOT.PBP`.
- Clang was unavailable in the local environment; no local Clang result is claimed.

The console builds prove compilation with the pinned SDKs; they do not claim runtime
behavior or performance on a real PS2 or PSP. No real SNES, PS2, PSP, PCSX2 or PPSSPP
runtime test and no performance measurement is claimed by this milestone.
