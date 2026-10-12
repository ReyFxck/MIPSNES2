# 65C816 logic/compare foundation audit

Date: 2026-10-11
Branch: `feature/65c816-logic-compare`
Reviewed base: `main` merge commit `86f39acdc021e2e4148facef20f9fdaddb1683c1`

## Scope

This increment adds exactly 23 opcodes to the platform-independent reference C
interpreter:

- ORA immediate (`09`), Direct (`05`) and absolute (`0D`);
- AND immediate (`29`), Direct (`25`) and absolute (`2D`);
- EOR immediate (`49`), Direct (`45`) and absolute (`4D`);
- CMP immediate (`C9`), Direct (`C5`) and absolute (`CD`);
- every CPX form: immediate (`E0`), Direct (`E4`) and absolute (`EC`);
- every CPY form: immediate (`C0`), Direct (`C4`) and absolute (`CC`);
- every BIT form: immediate (`89`), Direct (`24`), absolute (`2C`), Direct-X
  (`34`) and absolute-X (`3C`).

The verified interpreter subset grows from 82 to 105 opcodes. Remaining ORA, AND,
EOR and CMP address forms are deliberately not included. No ADC/SBC, decimal,
stack-relative, read-modify-write, interrupt, bus-map, cartridge, PPU, APU,
DMA/HDMA, enhancement-chip, dynarec, assembly, optimization or RFAuds2 runtime work
is part of this milestone. Unknown opcodes retain the sticky diagnostic stop.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.3, 2.4, 2.7 and 2.8 for ALU, accumulator/index widths and flags;
- sections 3.5.1, 3.5.3, 3.5.11, 3.5.17 and 3.5.18 for the included address modes;
- Tables 5-4 and 5-5 for opcode assignments, operations and status effects;
- the Table 5-5 BIT note: immediate BIT preserves N/V, while memory BIT copies the
  selected-width operand top bits to N/V;
- Table 5-7 entries 1a, 2, 10a/10b, 15 and 18 for base and conditional cycles;
- caveats 7.2, 7.3, 7.20 and 7.23 for Direct/index/width behavior.

ORA, AND and EOR write an M-selected result and update only N/Z. CMP is M-selected;
CPX/CPY are X-selected; each comparison preserves its operands and sets N/Z from the
subtraction plus C for no borrow. BIT preserves A, sets Z from A AND operand, applies
memory N/V from the operand, and leaves N/V unchanged for its immediate form. No
emulator source or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- every one of the 23 new opcodes;
- native 8-bit and 16-bit data widths plus an emulation-width logic case;
- independent M-selected accumulator and X-selected index comparisons;
- ORA/AND/EOR results, 8-bit B preservation and N/Z isolation;
- equality, less-than and greater-than comparison outcomes for N/Z/C;
- BIT zero tests, memory N/V replacement and immediate N/V preservation;
- unchanged comparison/BIT operands and unrelated registers/flags;
- Direct Page bank-zero selection and nonzero-DL cycle additions;
- absolute-X page/bank carry and indexed-read timing;
- 16-bit Direct reads and linear absolute reads across `7F:FFFF` to `80:0000`;
- exact instruction lengths and documented base/width/address cycle additions.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, logging-disabled contracts, all 105 CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed ASan+UBSan; leak detection disabled only for the documented local restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |
| `git diff --check` | Passed |

The CPU/bus executable reports `PASS: logical bus and 65C816 logic/compare subset`.

## Console cross-build evidence

`tools/setup_toolchain.py` downloaded, SHA-256 verified and extracted the pinned
ps2dev v2.0.0 and pspdev v20261001 archives. Both cross compilers report GCC 15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed on a sequential clean rebuild; produced a nonempty 1,320,096-byte, 32-bit little-endian N32 MIPS-III ELF |
| documented `PSPDEV`/`PATH` plus `make -C platform/psp clean all` | Passed; produced MIPS ELF/PRX and a nonempty 173,426-byte `EBOOT.PBP` |

The first PS2 build process returned zero but its immediate artifact inspection found
a zero-byte output, so that attempt was rejected. A sequential `clean all` rebuild
was run, returned zero and produced the valid ELF described above. Cross-build
success is not console execution.

## GitHub Actions evidence

Implementation commit: `ef737f3c6816b9b467cb8f1da2210342b295a6d9`.

| Workflow | Run | Result | Inspected artifact |
|---|---|---|---|
| Host CI | [38197728449](https://github.com/ReyFxck/MIPSNES2/actions/runs/38197728449) | Passed GCC 13.3 and Clang 18.1 release, sanitizer and test jobs | GCC host 21,472 bytes; Clang host 20,552 bytes |
| PS2 CI | [38197728331](https://github.com/ReyFxck/MIPSNES2/actions/runs/38197728331) | Passed pinned cross-build and checksum job | 1,320,232-byte MIPS ELF |
| PSP CI | [38197728330](https://github.com/ReyFxck/MIPSNES2/actions/runs/38197728330) | Passed pinned cross-build and checksum job | 173,426-byte `EBOOT.PBP` |

All four downloaded artifact archives are nonempty and unexpired. Their embedded
`SHA256SUMS` files validate every packaged binary and provenance file. GitHub artifact
metadata identifies branch head `ef737f3c6816b9b467cb8f1da2210342b295a6d9`;
artifact names and `BUILD.txt` use the pull request's synthetic merge commit
`4efaa84918014f568da3862eec4cd62a23a9949a`. The final documentation-only commit is
verified separately and recorded in the pull request conversation.

## Validation limits

Host unit tests do not establish complete CPU correctness. No PCSX2, real PS2,
PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution. No
performance or commercial-game compatibility measurement exists. The broad logic,
arithmetic, addressing, timing, regression and complete-interpreter tasks remain
in progress.
