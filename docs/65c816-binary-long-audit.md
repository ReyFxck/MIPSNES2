# 65C816 binary long ADC/SBC audit

Date: 2026-10-16
Branch: `feature/65c816-binary-adc-sbc-long`
Stacked base: `feature/65c816-binary-adc-sbc-direct-indirect` commit
`5ca1093628a2656f90a9afec3eec5c3791f53289`

## Scope

This increment adds exactly eight binary-mode opcodes to the platform-independent
reference C interpreter:

- ADC Direct-indirect-long `67`;
- ADC absolute long `6F`;
- ADC Direct-indirect-long-indexed `77`;
- ADC absolute long indexed with X `7F`;
- SBC Direct-indirect-long `E7`;
- SBC absolute long `EF`;
- SBC Direct-indirect-long-indexed `F7`;
- SBC absolute long indexed with X `FF`.

The dispatch subset grows from 179 to 187 opcodes and now contains every ADC/SBC
addressing form for binary operation. These forms reuse the shared M-selected binary
arithmetic helper and existing long-address helpers. When D=1 they enter the sticky
`MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC` diagnostic stop before fetching the
instruction operand/address. Decimal calculation and unrelated emulator subsystems
remain outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4-2.8 for A, X, Y, D, DBR and P;
- sections 3.5.5 and 3.5.7 for Direct-indirect-long addressing;
- sections 3.5.13 and 3.5.14 for absolute-long addressing;
- Table 5-5 for opcodes, arithmetic operations and C/V/N/Z effects;
- Table 5-7 entries 2 and 18 plus notes 1 and 2 for base, M-width and DL cycles;
- caveats 7.2 and 7.12 for Direct Page and binary-mode behavior.

Long Direct pointers provide the complete 24-bit data address and therefore ignore
DBR. Pointer bytes advance linearly inside bank zero and wrap at `00:FFFF` under the
existing project address contract. Long indexing adds the architectural X or Y to
the full 24-bit address. No emulator implementation or external ROM was copied or
committed.

## Verification coverage

Deterministic host tests cover:

- all eight opcodes in native 8-bit and 16-bit accumulator modes;
- ADC and SBC signed-overflow cases with C/no-borrow results;
- selected-width N/Z and preservation of unrelated status bits;
- 8-bit B preservation;
- long-pointer byte wrap at `00:FFFF` and pointer-bank selection independent of DBR;
- absolute-long operand fetch and word read crossing a bank boundary;
- full 24-bit X/Y indexed carry into the next bank;
- aligned and nonzero-DL Direct Page timing;
- exact PC movement and 5/6-cycle bases plus the M/DL additions;
- absence of page-cross and X-width penalties for long indexed forms;
- pre-operand D=1 diagnostic stops for all thirty ADC/SBC opcodes.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required. The previous temporary
PS2SDK/PSPSDK directories had expired before this milestone; the attempted local
cross-builds therefore stopped at missing `Makefile.eeglobal` and `psp-config` rather
than compiling source. The pinned GitHub cross-build jobs remain required evidence.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 187 dispatch opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean test` | Passed the release host matrix with GCC 13.3.0 |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test` | Passed AddressSanitizer and UndefinedBehaviorSanitizer host tests |
| `make -C platform/ps2 clean all` with the expired temporary `PS2DEV`/`PS2SDK` path | Not executed: failed before compilation because the temporary PS2SDK directory no longer existed |
| `make -C platform/psp clean all` with the expired temporary `PSPDEV` path | Not executed: failed before compilation because `psp-config` was unavailable |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

Cross-build success is compilation evidence only and never console execution.

## GitHub Actions evidence

Pending publication and successful workflow inspection. A workflow file existing is
not accepted as CI evidence.

## Validation limits

Host unit tests do not establish complete CPU correctness. Decimal ADC/SBC remains
unimplemented. No PCSX2, real PS2, PPSSPP or real PSP runtime test is claimed.
Cross-compilation is not execution. No performance or commercial-game compatibility
result exists. The broad arithmetic, timing, regression and complete-interpreter
tasks remain in progress.
