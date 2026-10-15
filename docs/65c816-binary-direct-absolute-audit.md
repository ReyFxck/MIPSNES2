# 65C816 binary Direct Page/absolute ADC/SBC audit

Date: 2026-10-15
Branch: `feature/65c816-binary-adc-sbc-direct-absolute`
Stacked base: `feature/65c816-binary-adc-sbc-foundation` commit
`0d162d6475a186dc121f2ca21452eacbbed71a31`

## Scope

This increment adds exactly four binary-mode opcodes to the platform-independent
reference C interpreter:

- ADC Direct Page `65`;
- ADC absolute `6D`;
- SBC Direct Page `E5`;
- SBC absolute `ED`.

The dispatch subset grows from 159 to 163 opcodes. These forms reuse the shared
M-selected binary arithmetic helper and the existing Direct Page/absolute address
helpers. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
instruction operand/address. Decimal calculation, indexed/indirect/long/
stack-relative ADC/SBC and all unrelated emulator subsystems remain outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4-2.8 for A, DBR, D and P;
- sections 3.5.1 and 3.5.17 for absolute and Direct addressing;
- Table 5-5 for opcodes, arithmetic operations and C/V/N/Z effects;
- Table 5-7 entries 1a and 10a plus notes 1 and 2 for M-width and DL cycles;
- caveats 7.2 and 7.12 for Direct Page and binary-mode behavior.

Direct Page uses bank zero and the existing emulator rule for the second byte of a
selected-width operand. Absolute uses DBR for the initial address and the existing
linear 24-bit data access for a second byte across a bank boundary. No emulator
implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- all four opcodes in native 8-bit and 16-bit accumulator modes;
- ADC and SBC signed-overflow cases with C/no-borrow results;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- Direct Page bank-zero 16-bit wrap at `00:FFFF`;
- aligned-D and `DL != 0` Direct Page cycle bases;
- absolute DBR selection and linear word read from `7F:FFFF` into `80:0000`;
- two-/three-byte PC advancement and exact width-adjusted cycles;
- pre-operand D=1 diagnostic stops for all six recognized ADC/SBC opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 163 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -C platform/ps2 clean all` with pinned `PS2DEV`/`PS2SDK` | Passed with PS2 toolchain v2.0.0 / GCC 15.2.0; produced a nonempty 1,404,384-byte `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with pinned `PSPDEV` | Passed with PSP toolchain v20261001 / GCC 15.2.0; produced a nonempty 184,722-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

The old temporary SDK extraction was no longer present, so both official archives
were downloaded again through `tools/setup_toolchain.py`, matched their pinned
SHA-256 digests and were extracted into a fresh directory. Cross-build success is not
console execution. GitHub GCC/Clang, cross-build and downloaded artifact/checksum
evidence remains pending until publication.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC and every
indexed, indirect, long and stack-relative ADC/SBC form remain unimplemented. No
PCSX2, real PS2, PPSSPP or real PSP runtime test is claimed. Cross-compilation is not
execution. No performance or commercial-game compatibility result exists. The broad
arithmetic, addressing, timing, regression and complete-interpreter tasks remain in
progress.
