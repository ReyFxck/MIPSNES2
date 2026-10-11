# 65C816 special-transfer completion audit

Date: 2026-10-11  
Branch: `feature/65c816-special-transfers`  
Reviewed stacked base: `55b6995a38c567291967c8ad24fe3ad60cf68d92`

## Scope

This increment adds exactly seven implied instructions to the reference C
interpreter: TCS (`1B`), TSC (`3B`), TCD (`5B`), TDC (`7B`), TXS (`9A`), TSX
(`BA`) and XBA (`EB`). Together with the previously verified A/X/Y transfers and
XCE, this completes the documented register-transfer instruction family. It does
not implement stack memory operations or any other instruction family.

No bus mapping, PPU, APU, DMA/HDMA, enhancement-chip, dynarec, assembly optimization
or RFAuds2 runtime work is included. The implementation remains caller-owned and
platform independent. Unknown opcodes still enter the sticky diagnostic stop and
never act as NOP.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4, 2.6, 2.7 and 2.11 for accumulator, Direct Page, index and stack
  register behavior;
- opcode matrix Table 5-4 and instruction table 5-5 for opcode assignments, flags
  and base cycles;
- Table 5-7 implied-addressing entries 19a/19b for two- and three-cycle forms;
- caveat 7.20 for full 16-bit transfer output and destination-selected storage/N/Z;
- caveat 7.21 for emulation/native TCS/TSC/TXS/TSX stack-high behavior.

No emulator implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- all seven opcodes and one-byte PC advancement;
- full 16-bit TCD/TDC/TSC/TCS paths regardless of M;
- 16-bit N/Z for TCD, TDC and TSC;
- status preservation for TCS and TXS;
- `SH=01` enforcement for TCS/TXS only in emulation mode;
- native-mode full-width TCS/TXS and X-selected 8/16-bit TSX behavior;
- XBA byte exchange independently of M/E and N/Z from the resulting low byte;
- two cycles for TCS/TSC/TCD/TDC/TXS/TSX and three cycles for XBA.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang jobs remain required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository check, contracts, logging-disabled contracts, CPU/bus tests and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG -std=c99 -Wall -Wextra -Wpedantic -Werror' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=c99 -Wall -Wextra -Wpedantic -Werror' LDFLAGS='-fsanitize=address,undefined' test` | Passed ASan+UBSan; LeakSanitizer disabled only for the documented local `/proc` restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |

The CPU/bus executable reported
`PASS: logical bus and 65C816 special-transfer subset` in every local host suite.

## Console cross-build evidence

The installed pinned PS2SDK v2.0.0 and PSPSDK v20261001 toolchains both report GCC
15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make clean all` in `platform/ps2` | Passed; compiled the changed core and produced a 32-bit MIPS `mipsnes2-ps2.elf` |
| documented `PSPDEV`/`PATH` plus `make clean all` in `platform/psp` | Passed; compiled the changed core and produced ELF, PRX and `EBOOT.PBP` |

An initial version probe used the unavailable legacy command name `ee-gcc`; the
Makefile still completed with the configured `mips64r5900el-ps2-elf-gcc`. The probe
and clean build were then repeated with that actual compiler name and passed. These
are cross-build results only; the outputs were not executed.

## GitHub Actions evidence

Pending publication. Workflow success, artifacts and checksums are not claimed by
the existence of local files or workflow YAML.

## Validation limits

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No performance,
compatibility, complete-CPU or complete-stack-instruction claim is made. The shared
bus behavior is unchanged by this increment. STATUS promotes only the completed
transfer-instruction family and the seven verified opcode/behavior rows; opcode
dispatch, timing, automated CPU tests and the complete interpreter remain in progress.
