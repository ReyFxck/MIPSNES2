# 65C816 binary stack-relative ADC/SBC audit

Date: 2026-10-15
Branch: `feature/65c816-binary-adc-sbc-stack-relative`
Stacked base: `feature/65c816-binary-adc-sbc-indexed` commit
`6180f5d9ce77f615a5acaa5b39c489515b053f21`

## Scope

This increment adds exactly four binary-mode opcodes to the platform-independent
reference C interpreter:

- ADC stack-relative `63`;
- ADC stack-relative-indirect-indexed `73`;
- SBC stack-relative `E3`;
- SBC stack-relative-indirect-indexed `F3`.

The dispatch subset grows from 169 to 173 opcodes. These forms reuse the shared
M-selected binary arithmetic helper and existing stack-relative address helpers.
Together with earlier LDA/STA/ORA/AND/EOR/CMP work, they complete the two
stack-relative accumulator addressing forms. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
instruction operand/address. Decimal calculation, Direct-indirect and long ADC/SBC
and all unrelated emulator subsystems remain outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4-2.8 for A, Y, S, DBR and P;
- sections 3.5.11 and 3.5.12 for stack-relative addressing;
- Table 5-5 for opcodes, arithmetic operations and C/V/N/Z effects;
- Table 5-7 entries 18 and 19a plus note 1 for base and M-width cycles;
- caveat 7.12 for binary-mode behavior.

The direct form uses the full 16-bit `S + offset` address in bank zero. The indirect
form reads a bank-zero wrapping pointer at that address, selects DBR and then adds Y
with 24-bit carry. Neither form receives a Direct Page low-byte, page-cross or X-width
cycle. No emulator implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- all four opcodes in native 8-bit and 16-bit accumulator modes;
- ADC and SBC signed-overflow cases with C/no-borrow results;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- bank-zero word and pointer wrap at `00:FFFF`;
- DBR plus Y data carry from bank `7F` into bank `80`;
- full S-based address formation in native and emulation modes;
- two-byte PC advancement and exact 4/7-cycle bases plus M-width cycle;
- pre-operand D=1 diagnostic stops for all sixteen recognized ADC/SBC opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required. The PS2 and PSP builds
reuse repository-pinned toolchains whose downloaded archives were previously matched
against the SHA-256 values recorded by the repository setup script.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 173 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -C platform/ps2 clean all` with pinned `PS2DEV`/`PS2SDK` | Passed with PS2 toolchain v2.0.0 / GCC 15.2.0; produced a nonempty 1,409,540-byte `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with pinned `PSPDEV` | Passed with PSP toolchain v20261001 / GCC 15.2.0; produced a nonempty 185,930-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

Cross-build success is compilation evidence only and never console execution.

## GitHub Actions evidence

Implementation commit: `807de91845eabe98558a9f9a770025e7c244802a`.

| Workflow | Run | Result | Inspected artifact |
|---|---|---|---|
| Host CI | [38823583608](https://github.com/ReyFxck/MIPSNES2/actions/runs/38823583608) | Passed GCC 13.3 and Clang 18.1 release, sanitizer and test jobs | GCC host 21,472 bytes; Clang host 20,552 bytes |
| PS2 CI | [38823583641](https://github.com/ReyFxck/MIPSNES2/actions/runs/38823583641) | Passed pinned cross-build and checksum job | 1,409,600-byte MIPS ELF |
| PSP CI | [38823583602](https://github.com/ReyFxck/MIPSNES2/actions/runs/38823583602) | Passed pinned cross-build and checksum job | 185,930-byte `EBOOT.PBP` |

The downloaded archive digests match GitHub artifact metadata:

- host GCC: `fb5826bc6ae4ee852587e2c439a7b4a8d26a78c14d63420ebc7fa88126e8e4f4`;
- host Clang: `7a5a7cc717d07b412cc08f2916172440e58d786e2dc3fbfbdb9d7741ebf549cb`;
- PS2: `33881f273809d72941728d3fc75bbe1bd3adf31c6fd98beb014d43e9fc3a76de`;
- PSP: `bb60e712a159a1b6d37324a95107bc6feb2c2f28c29e1ebe29348f77107f40c8`.

Every embedded `SHA256SUMS` entry validates. Artifact metadata identifies branch head
`807de91845eabe98558a9f9a770025e7c244802a`; artifact names and `BUILD.txt` identify
the pull request's synthetic merge commit
`83a5eff7260c6f1e30ef925450b30b003b7ce326`. The final documentation-only commit is
verified separately and recorded in the pull request conversation.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC and every
Direct-indirect and long ADC/SBC form remain unimplemented. No PCSX2, real PS2,
PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution. No
performance or commercial-game compatibility result exists. The broad arithmetic,
timing, regression and complete-interpreter tasks remain in progress.
