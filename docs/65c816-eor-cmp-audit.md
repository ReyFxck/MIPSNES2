# 65C816 extended EOR/CMP addressing audit

Date: 2026-10-12
Branch: `feature/65c816-eor-cmp-addressing`
Stacked base: `feature/65c816-ora-and-addressing` commit
`fee0f8246f1051a774784d63bb73230235b60e2d`

## Scope

This increment adds exactly 20 opcodes to the platform-independent reference C
interpreter. It completes every EOR and CMP form that can use the existing non-stack
address helpers:

- Direct-indexed-indirect `(direct,X)`: EOR `41`, CMP `C1`;
- Direct-indirect-long `[direct]`: EOR `47`, CMP `C7`;
- absolute long: EOR `4F`, CMP `CF`;
- Direct-indirect-indexed `(direct),Y`: EOR `51`, CMP `D1`;
- Direct-indirect `(direct)`: EOR `52`, CMP `D2`;
- Direct-X: EOR `55`, CMP `D5`;
- Direct-indirect-long-indexed `[direct],Y`: EOR `57`, CMP `D7`;
- absolute-Y: EOR `59`, CMP `D9`;
- absolute-X: EOR `5D`, CMP `DD`;
- absolute-long-X: EOR `5F`, CMP `DF`.

The verified interpreter subset grows from 125 to 145 opcodes. EOR/CMP stack-
relative `43`/`C3` and stack-relative-indirect-indexed `53`/`D3` remain explicitly
unimplemented because the CPU has no stack-relative address path yet. The matching
ORA/AND forms `03`/`13`/`23`/`33` are also kept in the diagnostic regression set.
Arithmetic, interrupts, bus mapping, cartridge, PPU, APU, DMA/HDMA, enhancement
chips, dynarec, assembly optimization and RFAuds2 runtime integration are outside
this milestone.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.3, 2.4, 2.5, 2.7 and 2.8 for ALU, DBR, M/X widths and flags;
- sections 3.5.3, 3.5.4, 3.5.6, 3.5.7 and 3.5.10-3.5.16 for the included address modes;
- Tables 5-4 and 5-5 for opcode assignments, operations and status effects;
- Table 5-7 and Table 3-1 for base and conditional cycle counts;
- caveats 7.2, 7.3, 7.20 and 7.23 for Direct/index/width behavior.

EOR uses M-selected operand/result width, preserves accumulator B in 8-bit mode and
updates only N/Z. CMP uses M-selected width, preserves the accumulator, updates N/Z
from the selected-width subtraction and sets C on no borrow. The new dispatch cases
reuse already-tested Direct, absolute-indexed, Direct-indirect, Direct-indirect-long
and absolute-long helpers. No emulator implementation or external ROM was copied or
committed.

## Verification coverage

Deterministic host tests cover:

- every one of the 20 new opcodes in native 8-bit and 16-bit accumulator modes;
- EOR selected-width result, N/Z isolation, preserved B and unrelated flags;
- CMP selected-width N/Z/C, unchanged accumulator and preserved unrelated flags;
- exact instruction length and base/DL/M/X/page-cross cycle additions;
- DBR selection for regular indirect modes and pointer-bank selection for long modes;
- Direct pre-index and indirect/long post-index address formation;
- emulation-mode aligned Direct-X low-byte wrapping;
- native 16-bit index cycle additions without a page crossing;
- linear 16-bit data reads and indexed carry across a 64 KiB bank boundary;
- explicit diagnostic stops for all eight excluded stack-relative accumulator-ALU
  opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 145 implemented CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed ASan+UBSan; leak detection disabled only for the documented local restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |
| `git diff --check` | Passed |

The CPU/bus executable reports
`PASS: logical bus and 65C816 accumulator ALU addressing subset`. Source dispatch,
documentation and checked STATUS opcode counts agree at 145/145/145.

## Console cross-build evidence

Previously SHA-256-verified pinned ps2dev v2.0.0 and pspdev v20261001 archives were
reused. Both target compilers report GCC 15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed; produced a nonempty 1,376,820-byte, 32-bit little-endian N32 MIPS-III ELF |
| documented `PSPDEV`/`PATH` plus `make -C platform/psp clean all` | Passed; produced MIPS ELF/PRX and a nonempty 181,138-byte `EBOOT.PBP` |

Cross-build success is not console execution. GitHub Actions evidence remains to be
recorded after the implementation revision is published.

## Validation limits

Host unit tests do not establish complete CPU correctness. No PCSX2, real PS2,
PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution. No
performance or commercial-game compatibility result exists. The broad logic,
arithmetic, addressing, timing, regression and complete-interpreter tasks remain in
progress.
