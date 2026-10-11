# 65C816 A/X/Y register-transfer audit

Date: 2026-10-10  
Branch: `feature/65c816-register-transfers`  
Reviewed base: `55887d1d85510333e342ba59d8d068909577f7b4`

## Scope

This increment adds exactly six implied A/X/Y transfers to the reference C
interpreter: TXA (`8A`), TYA (`98`), TXY (`9B`), TAY (`A8`), TAX (`AA`) and TYX
(`BB`). No stack/direct-register transfer, XBA, PPU, APU, DMA/HDMA, enhancement-chip,
dynarec, assembly optimization or RFAuds2 runtime work is included.

The implementation remains caller-owned and platform independent. It does not add a
bus access or platform dependency. Unknown opcodes still enter the sticky diagnostic
stop and never act as NOP.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4 and 2.7 for accumulator/index width and high-byte rules;
- opcode matrix Table 5-4 and instruction table 5-5 for the six opcode assignments;
- Table 5-7 implied-addressing entry 19a for the two-cycle timing;
- caveat 7.20 for full 16-bit transfer output and destination-selected storage/N/Z.

The opcode matrix is used for byte assignments because text extraction of the
instruction table can be ambiguous. No emulator implementation or external ROM was
copied or committed.

## Verification coverage

Deterministic host tests cover:

- all six opcodes and one-byte PC advancement;
- two cycles per implemented transfer;
- emulation-mode 8-bit index destinations and zeroed index high bytes;
- native 16-bit accumulator and index destinations;
- mixed `M=1/X=0` and `M=0/X=1` behavior;
- full A/B participation when a transfer targets a 16-bit index;
- accumulator B preservation for an 8-bit accumulator destination;
- zero extension from an 8-bit index into a 16-bit accumulator;
- N/Z from destination width while C/I/D/V remain unchanged.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang jobs remain required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository check, contracts, logging-disabled contracts, CPU/bus tests and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG -std=c99 -Wall -Wextra -Wpedantic -Werror' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=c99 -Wall -Wextra -Wpedantic -Werror' LDFLAGS='-fsanitize=address,undefined' test` | Passed ASan+UBSan; LeakSanitizer disabled only for the documented local `/proc` restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |

The CPU/bus executable reported
`PASS: logical bus and 65C816 register-transfer subset` in every local host suite.

## Console cross-build evidence

The repository installer downloaded, SHA-256 verified and extracted pinned PS2SDK
v2.0.0 and PSPSDK v20261001 archives after workspace maintenance removed the prior
copies. Both cross-compilers report GCC 15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make clean all` in `platform/ps2` | Passed; compiled the changed core and produced a 32-bit MIPS `mipsnes2-ps2.elf` |
| documented `PSPDEV`/`PATH` plus `make clean all` in `platform/psp` | Passed; compiled the changed core and produced ELF, PRX and `EBOOT.PBP` |

These are cross-build results only; the outputs were not executed.

## Pending evidence and limits

GitHub Actions results are pending at this snapshot. No PCSX2, real PS2, PPSSPP or
real PSP runtime test was performed. No performance, compatibility or complete-
transfer-family claim is made. Transfer instructions and the complete interpreter
remain in progress.
