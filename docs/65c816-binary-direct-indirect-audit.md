# 65C816 binary regular Direct-indirect ADC/SBC audit

Date: 2026-10-15
Branch: `feature/65c816-binary-adc-sbc-direct-indirect`
Stacked base: `feature/65c816-binary-adc-sbc-stack-relative` commit
`41f9bd32be403e645935ae2fa5fe9c7231be9995`

## Scope

This increment adds exactly six binary-mode opcodes to the platform-independent
reference C interpreter:

- ADC Direct-indexed-indirect `61`;
- ADC Direct-indirect-indexed `71`;
- ADC Direct-indirect `72`;
- SBC Direct-indexed-indirect `E1`;
- SBC Direct-indirect-indexed `F1`;
- SBC Direct-indirect `F2`.

The dispatch subset grows from 173 to 179 opcodes. These forms reuse the shared
M-selected binary arithmetic helper and existing regular Direct-indirect address
helpers. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
instruction operand/address. Decimal calculation, Direct-indirect-long and absolute-
long ADC/SBC and all unrelated emulator subsystems remain outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4-2.8 for A, X, Y, D, DBR and P;
- sections 3.5.3, 3.5.4 and 3.5.6 for regular Direct-indirect addressing;
- Table 5-5 for opcodes, arithmetic operations and C/V/N/Z effects;
- Table 5-7 entries 15, 19b and 20 plus notes 1, 2 and 4 for base, M-width, DL and indexed-read cycles;
- caveats 7.2 and 7.12 for Direct Page and binary-mode behavior.

All pointer reads wrap in bank zero and select DBR for the data address.
`(direct,X)` pre-indexes the pointer address; `(direct),Y` post-indexes the data
address with 24-bit carry. Its indexed-read cycle is added once when X selects
16-bit indexes or Y crosses a page. No emulator implementation or external ROM was
copied or committed.

## Verification coverage

Deterministic host tests cover:

- all six opcodes in native 8-bit and 16-bit accumulator modes;
- ADC and SBC signed-overflow cases with C/no-borrow results;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- bank-zero pointer wrap at `00:FFFF` and DBR data selection;
- X pre-index and Y post-index with 24-bit carry;
- aligned and nonzero-DL Direct Page timing;
- independent `(direct),Y` page-cross and 16-bit-index timing paths;
- two-byte PC advancement and exact base/width-adjusted cycles;
- pre-operand D=1 diagnostic stops for all twenty-two recognized ADC/SBC opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required. The PS2 and PSP builds
reuse repository-pinned toolchains whose downloaded archives were previously matched
against the SHA-256 values recorded by the repository setup script.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 179 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -C platform/ps2 clean all` with pinned `PS2DEV`/`PS2SDK` | Passed with PS2 toolchain v2.0.0 / GCC 15.2.0; produced a nonempty 1,414,420-byte `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with pinned `PSPDEV` | Passed with PSP toolchain v20261001 / GCC 15.2.0; produced a nonempty 187,018-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

Cross-build success is compilation evidence only and never console execution.
GitHub Actions and artifact evidence remain pending until the implementation commit
is published.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC and every
Direct-indirect-long and absolute-long ADC/SBC form remain unimplemented. No PCSX2,
real PS2, PPSSPP or real PSP runtime test is claimed. Cross-compilation is not
execution. No performance or commercial-game compatibility result exists. The broad
arithmetic, timing, regression and complete-interpreter tasks remain in progress.
