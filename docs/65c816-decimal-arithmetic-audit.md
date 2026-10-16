# 65C816 Decimal ADC/SBC Audit

Date: 2026-10-16

Branch: `feature/65c816-decimal-adc-sbc`

Pull request: [#23](https://github.com/ReyFxck/MIPSNES2/pull/23)

Stacked base: PR #22 final revision
`134c37911b2f945c15de72ee1c1dc8d25b10320f`

## Scope

This milestone implements packed-BCD decimal calculation for all thirty already
recognized ADC/SBC opcodes. It does not add opcodes, addressing modes, SNES bus
timing or scheduler behavior. D=0 retains the existing binary path; D=1 corrects
two nibbles when M=1 and four nibbles when M=0.

The accumulator width still follows M, B remains unchanged in 8-bit mode, and every
addressing form retains its established address and cycle rules. C reports decimal
carry or no-borrow, N/Z reflect the corrected result, and V reflects the binary
overflow stage. ADC derives V from the pre-correction highest-nibble sum; SBC derives
V from the binary subtraction result. Inputs containing A-F digits are deterministic
and covered rather than silently rejected.

## Primary reference

The implementation was independently derived from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf).
Table 5-5 defines ADC/SBC status effects. Table 8-1 identifies N/V/Z as valid in
decimal mode for the W65C816 and does not add a decimal-mode cycle. The opcode table
provides the addressing-mode base and width cycles retained by this change.

## Independent vector audit

A read-only audit used the MIT-licensed
[SingleStepTests/65816](https://github.com/SingleStepTests/65816) corpus. No corpus
source or test JSON is copied into MIPSNES2.

- Audited repository revision: `307c63f5673c8c6b861c52f71ddb89212a227480`
- `LICENCE` blob: `1ec4f0ecaa6b575b3fbbadf17f526c6d424cbf68`
- `v1/69.e.json`: `02c5dce7178c8d6537f3cbfe7c9d5075e1843acb`
- `v1/69.n.json`: `358fe81edf36ed99c19983c811ade6425c9207d5`
- `v1/e9.e.json`: `5bae443747976218878f97f5ebc2f3ef5d75b18f`
- `v1/e9.n.json`: `5f08c77d286130dc3cb8a4d542c3a8a4f867acf7`
- ADC emulation decimal cases: 4,962
- ADC native decimal cases: 4,981
- SBC emulation decimal cases: 5,015
- SBC native decimal cases: 5,012
- Total decimal cases compared: 19,970
- Result and relevant C/V/N/Z mismatches: 0

This audit includes invalid packed-BCD digits. It found that a naive overflow check
against the final corrected ADC result was wrong; the implementation was corrected
to use the pre-correction highest-nibble addition stage and the full audit then
reported zero mismatches.

## Repository verification

`tests/test_cpu_bus.c` adds:

- targeted 8- and 16-bit ADC/SBC decimal carry, borrow, zero, negative and overflow cases;
- two independently sourced invalid-digit ADC regressions;
- exhaustive valid 8-bit packed-BCD ADC/SBC testing: 100 × 100 × 2 carry states × 2 operations, or 40,000 instruction executions;
- D=1 execution of all thirty ADC/SBC addressing forms at both M widths, including PC, cycle and status checks;
- continued binary-mode and unimplemented-opcode regressions.

## Local verification

Environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14.

- `make clean && make test` — passed.
- `make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` — passed.
- `make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` — passed; leak detection was disabled because this managed runtime cannot expose the required process inspection.
- `python3 tools/check_repository.py && git diff --check` — passed.
- Local Clang — unavailable (`clang: No such file or directory`); GitHub Clang remains required.
- `make -C platform/ps2 clean && make -C platform/ps2` — unavailable because `PS2SDK` is unset.
- `make -C platform/psp clean && make -C platform/psp` — unavailable because `psp-config` is absent.

The pinned GitHub workflows remain the cross-build and Clang verification gates.

## GitHub Actions

The implementation commit is
`2711cc2625c7cd847c5f4e53debba0b42e6c729c`. GitHub tested the pull request's
synthetic merge `a941bef03fd4acb594c38b9fea51555bab1fb48c`.

- Host CI run `38936209822` — passed GCC and Clang release, sanitizer, test and artifact jobs.
- PS2 CI run `38936209553` — passed the pinned PS2SDK scaffold cross-build and artifact job.
- PSP CI run `38936209445` — passed the pinned PSPSDK scaffold cross-build and artifact job.

Downloaded artifact archives were inspected and their GitHub-reported SHA-256
digests matched locally:

- Host GCC artifact `12061596491`: `a0633d9c75ee3da485dd28ddac8c6a09aed062bc180c0a5b0b6f9aeede3925d1`; host binary 21,472 bytes.
- Host Clang artifact `12060373804`: `a0a9a253eae57bc19938c8d3d468e7a8eb713bbbeb389e81c5d414784375a9e2`; host binary 20,552 bytes.
- PS2 artifact `12060937922`: `9ceceade8594de543ed6b6f1a9ad053285f3a754966d065ef2dca236d6c760ae`; ELF 1,424,124 bytes.
- PSP artifact `12060568151`: `8b0fe1815e31e4f49dae7d3346f7a6313f0730538425c7af4a55dea8c5879f4e`; EBOOT 187,314 bytes.

Every archive's `SHA256SUMS` validated its executable/package, build metadata,
license and pinned toolchain metadata where present. These are build results only;
they are not console runtime validation.

## Limits

The independent corpus comparison audits immediate arithmetic state transitions; it
does not establish memory-address behavior, bus-cycle accuracy or complete CPU
correctness. Repository tests cover the existing addressing/timing contract, but no
PCSX2, PPSSPP, real PS2 or real PSP execution occurred. No performance measurement
or compatibility claim is made.
