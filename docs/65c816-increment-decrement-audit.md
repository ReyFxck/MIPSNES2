# 65C816 accumulator/index increment-decrement audit

Date: 2026-10-11
Branch: `feature/65c816-increment-decrement`
Reviewed base: `main` merge commit `06cb072739caa3529d4ca6fe5fde9971c170fabf`

## Scope

This increment adds exactly six register operations to the reference C interpreter:
INC accumulator (`1A`), DEC accumulator (`3A`), DEY (`88`), INY (`C8`), DEX (`CA`)
and INX (`E8`). No memory INC/DEC addressing mode is included. The broad arithmetic,
status, timing, CPU-test and complete-interpreter tasks remain in progress.

No bus-map, cartridge, PPU, APU, DMA/HDMA, enhancement-chip, dynarec, assembly,
optimization or RFAuds2 runtime work is included. The CPU remains caller-owned and
platform independent. Unknown opcodes still enter the sticky diagnostic stop and
never act as NOP.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4, 2.7 and 2.8 for accumulator/index widths and N/Z status meaning;
- Table 5-4 for the six opcode assignments;
- Table 5-5 for selected-width results and N/Z-only status effects;
- Table 5-7 entry 8 for two-cycle accumulator addressing;
- Table 5-7 entry 19a for one-byte, two-cycle implied index operations.

Accumulator INC/DEC use M-selected width and preserve B in 8-bit mode. Index
increments/decrements use X-selected width and leave an 8-bit result zero-extended.
All six wrap modulo the selected width, change only N/Z and take two cycles. No
emulator source or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- all six newly implemented opcodes;
- emulation, native 8-bit and native 16-bit cases;
- zero, negative and wraparound boundaries in both directions;
- accumulator B preservation in 8-bit mode;
- 8-bit index high-byte clearing/zero extension;
- exact N/Z changes and preservation of unrelated status bits;
- unchanged unrelated registers, E, DBR and PBR;
- one-byte PC movement and exactly two cycles per instruction.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so GitHub Clang jobs remain required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository check, contracts, logging-disabled contracts, all 82 CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed ASan+UBSan; leak detection disabled only for the documented local restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |

The CPU/bus executable reports `PASS: logical bus and 65C816 increment/decrement
subset`.

## Console cross-build evidence

The repository-pinned ps2dev v2.0.0 and pspdev v20261001 archives were downloaded,
SHA-256 verified and extracted by `tools/setup_toolchain.py`. Both toolchains report
GCC 15.2.0 and compile the changed shared core.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed; produced a 32-bit MIPS `mipsnes2-ps2.elf` |
| documented `PSPDEV`/`PATH` plus `make -C platform/psp clean all` | Passed; produced ELF, PRX and `EBOOT.PBP` |

Cross-build success is not console execution.

## GitHub Actions evidence

Implementation commit: `4a3fb02deef308992b7621707d3b4e5b6d381645`.

| Workflow | Run | Result | Inspected artifact |
|---|---|---|---|
| Host CI | [38187823622](https://github.com/ReyFxck/MIPSNES2/actions/runs/38187823622) | Passed GCC/Clang release, sanitizer and tests | GCC 18,301 bytes; Clang 17,778 bytes |
| PS2 CI | [38187823661](https://github.com/ReyFxck/MIPSNES2/actions/runs/38187823661) | Passed pinned cross-build and checksum jobs | 412,339 bytes |
| PSP CI | [38187823645](https://github.com/ReyFxck/MIPSNES2/actions/runs/38187823645) | Passed pinned cross-build and checksum jobs | 92,610 bytes |

All inspected artifacts are nonempty, unexpired and identify the correct head branch
and implementation SHA. Artifact names use GitHub's synthetic pull-request merge SHA,
while artifact metadata records the actual branch head SHA. The final documentation-
only commit is verified by its own workflow runs and recorded in the PR conversation.

## Validation limits

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No performance,
compatibility, complete-arithmetic, complete-CPU or complete-bus claim is made.
Memory INC/DEC and every other unlisted opcode remain explicitly unimplemented.
