# 65C816 implied status-control audit

Date: 2026-10-11
Branch: `feature/65c816-status-control`
Reviewed base: `main` merge commit `043d894b413624d10b4a7d7b1331ada2eb5ffa82`

## Scope

This increment adds exactly five opcodes to the reference C interpreter: CLI (`58`),
SEI (`78`), CLV (`B8`), CLD (`D8`) and SED (`F8`). Together with the previously
verified CLC (`18`) and SEC (`38`), this completes the implied single-flag clear/set
group. REP, SEP and XCE remain separately implemented mode/status operations; broad
status behavior remains in progress because most ALU instructions are absent.

No bus-map, cartridge, PPU, APU, DMA/HDMA, enhancement-chip, dynarec, assembly,
optimization or RFAuds2 runtime work is included. The CPU remains caller-owned and
platform independent. Unknown opcodes still enter the sticky diagnostic stop and
never act as NOP.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- section 2.8 for the D, I and V Processor Status Register bits;
- opcode tables 5-4 and 5-5 for assignments and exact affected status bits;
- Table 5-7 entry 19a for one-byte implied encoding and fixed two-cycle timing.

CLD/SED change only D, CLI/SEI change only I, and CLV changes only V. The instructions
do not change E, M, X, registers or any unrelated status bit. No emulator source or
external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- every newly implemented opcode in sequence;
- exact D/I/V clear and set results while preserving N/M/X/Z/C and unrelated bits;
- identical operation in emulation and native modes;
- unchanged A, X, Y, S, D, DBR, PBR and E state where applicable;
- one-byte PC movement and exactly two cycles per instruction.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so GitHub Clang jobs remain required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository check, contracts, logging-disabled contracts, all 76 CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed ASan+UBSan; leak detection disabled only for the documented local restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |

The CPU/bus executable reports `PASS: logical bus and 65C816 status-control subset`.

## Console cross-build evidence

The repository-pinned ps2dev v2.0.0 and pspdev v20261001 toolchains both report GCC
15.2.0 and compile the changed shared core.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed; produced a 32-bit MIPS `mipsnes2-ps2.elf` |
| documented `PSPDEV`/`PATH` plus `make -C platform/psp clean all` | Passed; produced ELF, PRX and `EBOOT.PBP` |

An initial `make -C platform/ps2 clean` invocation without the required environment
failed with the documented `PS2SDK is unset` guard before compiling anything. The
correct environment-scoped clean/build command above then passed. Cross-build success
is not console execution.

## GitHub Actions evidence

Pending publication. The PR must pass Host GCC/Clang, sanitizer, pinned PS2/PSP
cross-build, artifact and checksum jobs before this milestone is review-ready.

## Validation limits

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No performance,
compatibility, complete-status, complete-CPU or complete-bus claim is made. STATUS
promotes only the five implemented opcodes and their directly verified behavior;
opcode dispatch, general status behavior, timing, CPU tests and the complete
interpreter remain in progress.
