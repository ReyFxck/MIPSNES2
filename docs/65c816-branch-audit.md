# 65C816 branch-family completion audit

Date: 2026-10-11
Branch: `feature/65c816-conditional-branches`
Reviewed stacked base: `62e930121656181ef64fb6d08645393be73a37bd`

## Scope

This increment adds exactly nine opcodes to the reference C interpreter: BPL (`10`),
BMI (`30`), BVC (`50`), BVS (`70`), BRL (`82`), BCC (`90`), BCS (`B0`), BNE
(`D0`) and BEQ (`F0`). Together with the previously verified BRA (`80`), this
completes the W65C816S conditional, relative-always and relative-long branch family.

No SNES bus mapping, PPU, APU, DMA/HDMA, enhancement-chip, dynarec, assembly
optimization or RFAuds2 runtime work is included. The CPU remains caller-owned and
platform independent. Unknown opcodes still enter the sticky diagnostic stop and
never act as NOP.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- section 3.4 for the PBR behavior of relative addressing and PC wrap;
- sections 3.5.20 and 3.5.21 for signed 16-bit/8-bit offsets applied to the
  already-advanced PC without changing PBR;
- operation-code tables 5-4 and 5-5 for opcode assignments, tested flags and
  unchanged status results;
- Table 5-7 entries 20 and 21 plus notes 5 and 6 for conditional/BRA 2/3/4-cycle
  timing, the emulation-only page-cross addition and BRL's fixed four cycles.

No emulator implementation or external ROM was copied or committed. Signed offsets
are extended with explicit arithmetic so behavior does not depend on an
implementation-defined unsigned-to-signed cast.

## Verification coverage

Deterministic host tests cover:

- all eight conditional branch opcodes in taken and non-taken paths;
- N, V, C and Z selection plus exact status-register preservation;
- positive and negative signed 8-bit offsets from the post-operand PC, including
  the `+127` and `-128` extrema;
- two cycles not taken, three cycles taken, and a fourth cycle only for a taken
  emulation-mode branch that crosses a page;
- no page-cross penalty in native mode and no penalty for a non-taken branch;
- BRA through the same relative helper;
- BRL positive and negative signed 16-bit offsets, including `+32767` and `-32768`,
  at a fixed four cycles in emulation and native modes;
- operand-fetch and target wrapping at `FFFF/0000` without changing PBR.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so GitHub Clang jobs remain required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository check, contracts, logging-disabled contracts, all 71 CPU opcodes and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed with `NDEBUG` |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize clean` followed by the documented ASan+UBSan `make ... test` | Passed ASan+UBSan; leak detection disabled only for the local environment restriction |
| `python3 tools/check_repository.py` | Passed required files, license, identity, links, checkbox, core-boundary and action-pin checks |

The CPU/bus executable reports `PASS: logical bus and 65C816 branch subset`.

## Console cross-build evidence

Fresh installs of the pinned ps2dev v2.0.0 and pspdev v20261001 archives passed their
repository SHA-256 checks before extraction. Both toolchains report GCC 15.2.0.

| Command | Result |
|---|---|
| documented `PS2DEV`/`PS2SDK`/`PATH` plus `make -C platform/ps2 clean all` | Passed; compiled the changed core and produced a 32-bit MIPS `mipsnes2-ps2.elf` |
| documented `PSPDEV`/`PATH`, `make -C platform/psp clean`, then `make -C platform/psp all` | Passed; compiled the changed core and produced ELF, PRX and `EBOOT.PBP` |

These are cross-build results only; the outputs were not executed.

## GitHub Actions evidence

Pending publication and inspection of the Host, PS2 and PSP workflow runs for the
reviewable implementation commit.

## Validation limits

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No performance,
compatibility, complete-CPU or complete-SNES-bus claim is made. The shared bus is
unchanged by this increment. STATUS promotes only the complete branch family and its
verified opcode/behavior rows; opcode dispatch, overall timing, automated CPU tests
and the complete interpreter remain in progress.
