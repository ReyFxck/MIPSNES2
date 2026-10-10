# 65C816 Direct-indirect load/store audit

Date: 2026-10-10

Branch: `feature/65c816-direct-indirect-load-store`

Reviewed base: `72c1948943a86bbed5c6ae13bab97db49ec0cc2c`, the head of the
unmerged Direct Page load/store PR when this focused stacked branch began.

## Scope

This milestone adds exactly ten LDA/STA opcodes: `81`, `87`, `91`, `92`, `97`,
`A1`, `A7`, `B1`, `B2` and `B7`. It verifies `(direct,X)`, `(direct)`,
`(direct),Y`, `[direct]` and `[direct],Y`; regular pointer wrapping and DBR use;
the Direct-indirect-long pointer-byte exception and pointer-supplied bank; pre/post
indexing; M-selected data width; LDA N/Z results; and documented cycle additions.

It does not implement other indirect instructions, a complete interpreter, a SNES
memory map or runtime console emulation. No external test ROM or implementation from
another emulator is included.

## Behavioral source

The implementation was independently derived from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially addressing sections 3.5.10 and 3.5.13-3.5.16, opcode table 3-1, timing
tables 5-5 and 5-7, and caveat 7.2. These establish bank-zero pointer fetches,
regular-pointer Direct Page wrapping, the long-pointer linear-byte exception, DBR
selection, full-address post-indexing and the relevant base/penalty cycles.

## Local host evidence

Environment:

- GCC 13.3.0 (`cc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`)
- GNU Make 4.3
- Python 3.12.14

| Command | Result |
|---|---|
| `make clean && make test` | Passed after implementation: repository checker, contract tests, logging-disabled contracts, CPU/bus tests and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG -std=c99 -Wall -Wextra -Wpedantic -Werror' test` | Passed all host tests with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=c99 -Wall -Wextra -Wpedantic -Werror' LDFLAGS='-fsanitize=address,undefined' test` | Passed ASan+UBSan; LeakSanitizer alone was disabled for the pre-existing local `/proc` inspection restriction |
| `command -v clang` | Unavailable; GitHub's Clang job remains required |

The implemented CPU/bus suite reports `PASS: logical bus and 65C816 direct-indirect
load/store subset`.

## Console cross-build evidence

Pinned PS2SDK v2.0.0 and PSPSDK v20261001 archives were already installed; both use
GCC 15.2.0 in this environment.

| Command | Result |
|---|---|
| `env PS2DEV=<pinned-ps2dev> PS2SDK=<pinned-ps2sdk> PATH=<pinned-ps2-paths> make clean all` in `platform/ps2` | Passed; produced a 32-bit MIPS `mipsnes2-ps2.elf` and compiled the changed CPU core |
| `env PSPDEV=<pinned-pspdev> PATH=<pinned-psp-path> make clean all` while still in `platform/ps2` | Failed before compilation because `PS2SDK` was unset; this was an operator working-directory error, not a code failure |
| `env PSPDEV=<pinned-pspdev> PATH=<pinned-psp-path> make clean all` in `platform/psp` | Passed; produced `mipsnes2-psp.elf`, PRX and `EBOOT.PBP` and compiled the changed CPU core |
| `psp-gcc --version` without the PSP path in that shell | Failed with command not found after the successful build; the absolute pinned path then reported GCC 15.2.0 |

These are cross-build results only. Neither output was executed in an emulator or
on real console hardware.

## GitHub Actions evidence

The first published implementation commit was
`6754868adfee6406a11c372bd1b74c5863c48668`. All triggered workflows completed
successfully on the first attempt:

| Workflow | Verified jobs | Result |
|---|---|---|
| [Host CI run 38087337175](https://github.com/ReyFxck/MIPSNES2/actions/runs/38087337175) | GCC and Clang release builds/tests; GCC and Clang sanitizer builds/tests; two host artifact uploads | Passed |
| [PS2 CI run 38087337189](https://github.com/ReyFxck/MIPSNES2/actions/runs/38087337189) | Pinned toolchain install, cross-build, artifact/checksum staging and upload | Passed |
| [PSP CI run 38087337183](https://github.com/ReyFxck/MIPSNES2/actions/runs/38087337183) | Pinned toolchain install, cross-build, artifact/checksum staging and upload | Passed |

The final documentation commit is validated separately. Its resulting workflow URLs
are recorded in the PR conversation rather than creating an unbounded chain of
commits that each records the preceding run. The PR is stacked on
`feature/65c816-direct-page-load-store` and is not merged automatically.

## Validation limits

- Host runtime: tested only through the commands recorded here.
- PCSX2: not tested.
- Real PS2: not tested.
- PPSSPP: not tested.
- Real PSP: not tested.
- Performance: not measured; no performance claim is made.
