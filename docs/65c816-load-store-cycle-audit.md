# 65C816 resumable load/store-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates the forty-three remaining members of the
`LDA/STA/LDX/LDY/STX/STY` family from the legacy-atomic remainder to the
caller-owned peek/commit cursor. Together with the previously resumable immediate
`LDA` (`A9`) and absolute `STA` (`8D`), all forty-five opcodes in these six
families now use one shared core state machine:

- all fifteen `LDA` modes and all fourteen `STA` modes;
- all five modes each for `LDX` and `LDY`;
- all three modes each for `STX` and `STY`.

The earlier planning estimate said forty-four opcodes remained. The audited opcode
table contains forty-three: the six families contain forty-five opcodes in total,
two were already resumable, and `STA` has no immediate form. The repository totals
therefore move from 106 to 149 resumable opcodes, leaving 107 legacy-atomic opcode
remainders.

The shared phases expose program operands, Direct Page penalties, stack-relative
address formation, regular and long pointer reads, conditional indexed cycles and
M/X-selected data transfers. Direct data high bytes retain the E/DL wrapping rule;
stack-relative words wrap in bank zero; ordinary absolute, indirect and long data
transfers advance linearly in the 24-bit address space. Store callbacks are
preflighted before any operand byte is consumed. No platform API, backend branch or
game-specific behavior was added.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
particularly the opcode table, processor-cycle tables, M/X width rules, Direct Page
notes and indexed-read timing notes. The established aggregate interpreter and its
generated deterministic fixtures remain the project-local semantic oracle.

XERJ is restricted to this MIPSNES2 checkout, with feedback disabled. No source from
another emulator was consulted, indexed or copied.

## Deterministic coverage

- an exact 45-opcode matrix at both selected widths: 90 cases total;
- every immediate, Direct, Direct-indexed, regular/long indirect,
  stack-relative, absolute and long addressing path;
- exact operand, pointer and data read/write addresses and order;
- Direct Page low-byte penalty, fixed address/index cycles and conditional
  absolute/indirect indexed-read cycles;
- M-selected accumulator transfers and X-selected index-register transfers;
- 8-bit accumulator-B preservation, 8-bit index high-byte clearing, 16-bit values,
  load N/Z effects and store flag preservation;
- low-before-high stores and exact aggregate instruction-cycle totals;
- missing-write-bus preflight for all twenty store opcodes before operand access;
- stale Direct-penalty descriptor rejection at the following indexed phase without
  an extra bus or cycle effect;
- unchanged aggregate semantics, edge addressing and cycle totals across all 256
  opcode values.

## Deliberate limits

The other 107 opcode remainders and reset remain legacy-atomic. Internal descriptors
do not claim a W65C816 dummy read/write or electrical address-bus state. This block
does not add mid-instruction `ABORT`, cursor cancellation, DMA/HDMA ownership or
scheduler event-budget propagation. The central timed-step bridge remains
instruction-atomic.

## Completion criterion

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
XERJ can index and resolve the new first-party code with feedback disabled, and the
unchanged shared core compiles first in the pinned PS2 toolchain and then in the
pinned PSP toolchain. Console compilation is not a runtime or performance claim.

## Verification

Published implementation commit `ffcf60fa0d9ae327bf193b9f227804bfd100540f`
used exact source tree `daa4a7164576059738ef9739da46861de2892e14` for every
verification below.

- `make BUILD_DIR=build/load-store-debug clean && make
  BUILD_DIR=build/load-store-debug -j2 test` — passed all 14 C test executables,
  the host smoke test and repository checks.
- `make BUILD_DIR=build/load-store-release CC=gcc CFLAGS='-O2 -g -DNDEBUG'
  clean && make BUILD_DIR=build/load-store-release CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' -j2 test` — passed.
- `make BUILD_DIR=build/load-store-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean && ASAN_OPTIONS=detect_leaks=0
  make BUILD_DIR=build/load-store-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j1 test` — passed with no ASan or UBSan
  diagnostic; leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-load-store-cpu.o` — passed.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- XERJ `v1.0.0-rc.97` dry-run and first-party index covered 156 files, all 54
  detected code files and 1,240 live records with zero junk files. Search found the
  load/store matrix and stale-internal regression; definition lookup resolved
  `load_store_apply_load`. Feedback remained disabled.
- Pinned PS2 toolchain `v2.0.0`, digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/ps2 clean all` passed with
  `mips64r5900el-ps2-elf-gcc 15.2.0` and produced `mipsnes2-ps2.elf`.
- Pinned PSP toolchain `v20261001`, digest-verified by
  `tools/setup_toolchain.py`: `make -C platform/psp clean all` passed with
  `psp-gcc 15.2.0` and produced `EBOOT.PBP`.

The console builds prove compilation with the pinned SDKs; they do not claim runtime
behavior or performance on a real PS2 or PSP. No real SNES, PS2, PSP, PCSX2 or
PPSSPP runtime test and no performance measurement is claimed by this milestone.
