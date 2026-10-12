# 65C816 binary immediate ADC/SBC audit

Date: 2026-10-12
Branch: `feature/65c816-binary-adc-sbc-foundation`
Stacked base: `feature/65c816-stack-relative-load-store` commit
`0d5ed9e2020ff8e82ded9d86566f82e106371036`

## Scope

This increment adds exactly two opcodes to the platform-independent reference C
interpreter:

- ADC immediate `69` in binary mode;
- SBC immediate `E9` in binary mode.

The dispatch subset grows from 157 to 159 opcodes. These two opcodes deliberately do
not calculate packed BCD yet. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
operand. This prevents a recognized decimal operation from silently producing a
binary result. Every other ADC/SBC addressing mode, decimal calculation, stack
instructions, interrupts, bus mapping, cartridge, PPU, APU, DMA/HDMA, enhancement
chips, dynarec, assembly optimization and RFAuds2 runtime integration are outside
scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4 and 2.8 for A and P;
- section 3.5.18 and Table 3-1 for M-selected immediate operands;
- Tables 5-4 and 5-5 for opcodes, `A + M + C`, subtraction and C/V/N/Z effects;
- Table 5-7 immediate timing and note 1 for the M=0 byte/cycle addition.

The binary ADC path adds A, the selected-width operand and carry input. SBC treats C
as no-borrow and subtracts the selected-width operand plus `1-C`. C reports unsigned
carry/no-borrow. V detects a signed result outside the selected-width range; N and Z
come from the selected-width result. M=1 updates only the low accumulator byte and
preserves B. No emulator implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- ADC and SBC immediate in native 8-bit and 16-bit accumulator modes;
- carry input and borrow input;
- carry out, no-borrow and unsigned wrap;
- positive-plus-positive, negative-plus-negative and subtraction signed overflow;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- two-/three-byte PC advancement and exact 2/3-cycle timing;
- D=1 diagnostic stops for both opcodes at both widths;
- no decimal operand fetch, register/status mutation or cycle addition;
- sticky repeated-step behavior after the decimal diagnostic stop.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 159 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -f Makefile.ps2 clean all` | Passed with the pinned PS2 toolchain v2.0.0 / GCC 15.2.0; produced a nonempty 1,400,296-byte `mipsnes2-ps2.elf` |
| `make -f Makefile.psp clean all` | Passed with the pinned PSP toolchain v20261001 / GCC 15.2.0; produced a nonempty 184,082-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed after the implementation and documentation updates |

The PS2 and PSP SDKs were reused from the locally cached pinned toolchains. GitHub
GCC/Clang, cross-build and artifact/checksum evidence remains pending until this
branch is published and its workflows finish.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC is not
implemented, and only the immediate binary addressing form exists. No PCSX2, real
PS2, PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution.
No performance or commercial-game compatibility result exists. The broad arithmetic,
addressing, timing, regression and complete-interpreter tasks remain in progress.
