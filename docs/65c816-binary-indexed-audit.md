# 65C816 binary indexed ADC/SBC audit

Date: 2026-10-15
Branch: `feature/65c816-binary-adc-sbc-indexed`
Stacked base: `feature/65c816-binary-adc-sbc-direct-absolute` commit
`1a6166a01d2c63a0242a5e6195206cdf835c4587`

## Scope

This increment adds exactly six binary-mode opcodes to the platform-independent
reference C interpreter:

- ADC Direct-X `75`;
- ADC absolute-Y `79`;
- ADC absolute-X `7D`;
- SBC Direct-X `F5`;
- SBC absolute-Y `F9`;
- SBC absolute-X `FD`.

The dispatch subset grows from 163 to 169 opcodes. These forms reuse the shared
M-selected binary arithmetic helper and existing Direct Page/absolute-indexed address
helpers. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
instruction operand/address. Decimal calculation, indirect, long and stack-relative
ADC/SBC and all unrelated emulator subsystems remain outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4-2.8 for A, X, Y, DBR, D and P;
- sections 3.5.1, 3.5.2 and 3.5.17 for absolute-indexed and Direct addressing;
- Table 5-5 for opcodes, arithmetic operations and C/V/N/Z effects;
- Table 5-7 entries 1a, 1b and 10b plus notes 1, 2 and 4 for M-width, indexed-read and DL cycles;
- caveats 7.2 and 7.12 for Direct Page and binary-mode behavior.

Direct-X uses bank zero and the existing Direct Page wrapping rule. Absolute-X/Y
uses DBR for the base and linear 24-bit addition/data access. The indexed-read cycle
is added once when X selects 16-bit indexes or indexing crosses a page; the two
conditions do not stack. No emulator implementation or external ROM was copied or
committed.

## Verification coverage

Deterministic host tests cover:

- all six opcodes in native 8-bit and 16-bit accumulator modes;
- ADC and SBC signed-overflow cases with C/no-borrow results;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- Direct-X bank-zero wrap and aligned/nonzero-DL cycle bases;
- absolute-X/Y no-cross, page-cross and 16-bit-index timing paths;
- linear absolute indexing from bank `7F` into bank `80`;
- two-/three-byte PC advancement and exact width-adjusted cycles;
- pre-operand D=1 diagnostic stops for all twelve recognized ADC/SBC opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required. The PS2 and PSP builds
reuse repository-pinned toolchains whose downloaded archives were previously matched
against the SHA-256 values recorded by the repository setup script.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 169 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -C platform/ps2 clean all` with pinned `PS2DEV`/`PS2SDK` | Passed with PS2 toolchain v2.0.0 / GCC 15.2.0; produced a nonempty 1,407,132-byte `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with pinned `PSPDEV` | Passed with PSP toolchain v20261001 / GCC 15.2.0; produced a nonempty 185,434-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

Cross-build success is compilation evidence only and never console execution.

## GitHub Actions evidence

Implementation commit: `8ad1dea1e79e22cecdc61cfcb453100b769a25f9`.

| Workflow | Run | Result | Inspected artifact |
|---|---|---|---|
| Host CI | [38822863810](https://github.com/ReyFxck/MIPSNES2/actions/runs/38822863810) | Passed GCC 13.3 and Clang 18.1 release, sanitizer and test jobs | GCC host 21,472 bytes; Clang host 20,552 bytes |
| PS2 CI | [38822863565](https://github.com/ReyFxck/MIPSNES2/actions/runs/38822863565) | Passed pinned cross-build and checksum job | 1,407,192-byte MIPS ELF |
| PSP CI | [38822863667](https://github.com/ReyFxck/MIPSNES2/actions/runs/38822863667) | Passed pinned cross-build and checksum job | 185,434-byte `EBOOT.PBP` |

The downloaded archive digests match GitHub artifact metadata:

- host GCC: `c994af1dd491c3bf6df459569d6df7a4014d1398e52bfa0dceb63097e76d29ec`;
- host Clang: `2697476bea4c8c20f8888e37fd1364abd0597eaa3724db614c1b9d0e31e4d282`;
- PS2: `14be0e1295ad58d36ac676bca38e0dedb339e12aeb33200e3eb5b212b3141941`;
- PSP: `561592afafce7154d33d21eb1c46b6568a33672fbb9e430594a10635ecd752a3`.

Every embedded `SHA256SUMS` entry validates. Artifact metadata identifies branch head
`8ad1dea1e79e22cecdc61cfcb453100b769a25f9`; artifact names and `BUILD.txt` identify
the pull request's synthetic merge commit
`ffba1be6a5d08a6ada0744f02d40b8611711d164`. The final documentation-only commit is
verified separately and recorded in the pull request conversation.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC and every
indirect, long and stack-relative ADC/SBC form remain unimplemented. No PCSX2, real
PS2, PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution.
No performance or commercial-game compatibility result exists. The broad arithmetic,
addressing, timing, regression and complete-interpreter tasks remain in progress.
