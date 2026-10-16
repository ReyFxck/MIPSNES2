# 65C816 extended ORA/AND addressing audit

Date: 2026-10-12
Branch: `feature/65c816-ora-and-addressing`
Stacked base: `feature/65c816-logic-compare` commit
`f3ad2eba4db3770de373cfc0c85877768e791a0b`

## Scope

This increment adds exactly 20 opcodes to the platform-independent reference C
interpreter. It completes every ORA and AND form that can use the existing non-stack
address helpers:

- Direct-indexed-indirect `(direct,X)`: ORA `01`, AND `21`;
- Direct-indirect-long `[direct]`: ORA `07`, AND `27`;
- absolute long: ORA `0F`, AND `2F`;
- Direct-indirect-indexed `(direct),Y`: ORA `11`, AND `31`;
- Direct-indirect `(direct)`: ORA `12`, AND `32`;
- Direct-X: ORA `15`, AND `35`;
- Direct-indirect-long-indexed `[direct],Y`: ORA `17`, AND `37`;
- absolute-Y: ORA `19`, AND `39`;
- absolute-X: ORA `1D`, AND `3D`;
- absolute-long-X: ORA `1F`, AND `3F`.

The verified interpreter subset grows from 105 to 125 opcodes. ORA/AND stack-
relative `03`/`23` and stack-relative-indirect-indexed `13`/`33` remain explicitly
unimplemented because the CPU has no stack-relative address path yet. EOR/CMP,
arithmetic, interrupts, bus mapping, cartridge, PPU, APU, DMA/HDMA, enhancement
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

ORA/AND use M-selected operand/result width, preserve accumulator B in 8-bit mode,
and update only N/Z. The new dispatch cases reuse the already-tested Direct,
absolute-indexed, Direct-indirect, Direct-indirect-long and absolute-long helpers.
No emulator implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- every one of the 20 new opcodes in native 8-bit and 16-bit accumulator modes;
- selected-width result, N/Z isolation, preserved B and preserved unrelated flags;
- exact instruction length and base/DL/M/X/page-cross cycle additions;
- DBR selection for regular indirect modes and pointer-bank selection for long modes;
- Direct pre-index and indirect/long post-index address formation;
- emulation-mode aligned Direct-X low-byte wrapping;
- native 16-bit index cycle additions without a page crossing;
- linear 16-bit data reads and indexed carry across a 64 KiB bank boundary;
- explicit diagnostic stop for representative excluded stack-relative opcode `03`.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 125 implemented CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed ASan+UBSan; leak detection disabled only for the documented local restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |
| `git diff --check` | Passed |

The CPU/bus executable reports
`PASS: logical bus and 65C816 ORA/AND addressing subset`. Source dispatch,
documentation and checked STATUS opcode counts agree at 125/125/125.

## Console cross-build evidence

`tools/setup_toolchain.py` downloaded, SHA-256 verified and extracted the pinned
ps2dev v2.0.0 and pspdev v20261001 archives. Both target compilers report GCC 15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed; produced a nonempty 1,347,236-byte, 32-bit little-endian N32 MIPS-III ELF |
| documented `PSPDEV`/`PATH` plus `make -C platform/psp clean all` | Passed; produced MIPS ELF/PRX and a nonempty 177,106-byte `EBOOT.PBP` |

An initial PS2 compiler-version probe used the unavailable legacy name `ee-gcc`.
The build itself invoked the SDK's canonical `mips64r5900el-ps2-elf-gcc`, succeeded,
and that compiler was separately verified as GCC 15.2.0. Cross-build success is not
console execution.

## GitHub Actions evidence

Implementation commit: `6c03dabef36728184aa430ee4043bb32d3ad983a`.

| Workflow | Run | Result | Inspected artifact |
|---|---|---|---|
| Host CI | [38199978254](https://github.com/ReyFxck/MIPSNES2/actions/runs/38199978254) | Passed GCC 13.3 and Clang 18.1 release, sanitizer and test jobs | GCC host 21,472 bytes; Clang host 20,552 bytes |
| PS2 CI | [38199978255](https://github.com/ReyFxck/MIPSNES2/actions/runs/38199978255) | Passed pinned cross-build and checksum job | 1,347,360-byte MIPS ELF |
| PSP CI | [38199978233](https://github.com/ReyFxck/MIPSNES2/actions/runs/38199978233) | Passed pinned cross-build and checksum job | 177,106-byte `EBOOT.PBP` |

All four downloaded artifact archives are nonempty and unexpired. The archive
digests match GitHub artifact metadata, and their embedded `SHA256SUMS` files validate
every packaged binary and provenance file. Artifact metadata identifies branch head
`6c03dabef36728184aa430ee4043bb32d3ad983a`; artifact names and `BUILD.txt` use the
pull request's synthetic merge commit `4468fa226d7f35481e2d8fd23d76fdad68ddab1d`.
The final documentation-only commit is verified separately and recorded in the pull
request conversation.

## Validation limits

Host unit tests do not establish complete CPU correctness. No PCSX2, real PS2,
PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution. No
performance or commercial-game compatibility result exists. The broad logic,
addressing, timing, regression and complete-interpreter tasks remain in progress.
