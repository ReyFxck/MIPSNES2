# 65C816 Direct Page load/store audit

Date: 2026-10-10

Branch: `feature/65c816-direct-page-load-store`

Reviewed base: `e2ca1fb774daf29b81f50670ec9bff7fb010c272`, the head of the
unmerged addressing/load-store PR when this focused stacked branch began.

## Scope

This milestone adds exactly twelve load/store opcodes: `84`, `85`, `86`, `94`,
`95`, `96`, `A4`, `A5`, `A6`, `B4`, `B5` and `B6`. It verifies Direct and
Direct-indexed bank-zero address formation, the E=1/DL=0 page-wrap exception,
full bank-zero addition otherwise, M/X-selected data width, N/Z results and the
documented DL/width cycle additions.

It does not implement Direct-indirect forms, Direct read-modify-write or ALU forms,
a complete interpreter, a SNES memory map or runtime console emulation. No external
test ROM or implementation from another emulator is included.

## Behavioral source

The implementation was independently derived from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially sections 2.6, 3.5.11, 3.5.12 and 3.5.17, tables 5-5 and 5-7, and caveat
7.2. These establish Direct Page bank zero, indexed address formation, the
emulation-mode aligned-page wrap rule and the relevant base/penalty cycles.

## Local host evidence

Environment:

- GCC 13.3.0 (`cc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`)
- GNU Make 4.3
- Python 3.12.14

| Command | Result |
|---|---|
| `make clean && make test` | Passed: repository checker, contract tests, logging-disabled contracts, CPU/bus tests and host smoke test |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG -std=c99 -Wall -Wextra -Wpedantic -Werror' test` | Passed all host tests with `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=c99 -Wall -Wextra -Wpedantic -Werror' LDFLAGS='-fsanitize=address,undefined' test` | Passed ASan+UBSan; LeakSanitizer alone was disabled for the pre-existing local `/proc` inspection restriction |
| `command -v clang` | Unavailable; GitHub's Clang job remains required |

All executed CPU/bus tests reported `PASS: logical bus and 65C816 direct-page
load/store subset`.

## Console cross-build evidence

Pinned archives were installed by `tools/setup_toolchain.py`; both archive hashes
were checked before extraction. PS2SDK v2.0.0 and PSPSDK v20261001 each contain GCC
15.2.0 in this environment.

| Command | Result |
|---|---|
| `make clean all` without SDK environment | Failed before compilation because `PS2SDK` was unset; retained here as an honest operational failure |
| `make clean all` without SDK environment | Failed before compilation because `psp-config` was unavailable on `PATH`; retained here as an honest operational failure |
| `env PS2DEV=<pinned-ps2dev> PS2SDK=<pinned-ps2sdk> PATH=<pinned-ps2-paths> make clean all` in `platform/ps2` | Passed; produced a 32-bit MIPS `mipsnes2-ps2.elf` and compiled the changed CPU core |
| `env PSPDEV=<pinned-pspdev> PATH=<pinned-psp-path> make clean all` in `platform/psp` | Passed; produced `mipsnes2-psp.elf`, PRX and `EBOOT.PBP` and compiled the changed CPU core |

These are cross-build results only. Neither output was executed in an emulator or
on real console hardware.

## GitHub Actions evidence

Pending publication and workflow execution. A workflow file or prior-branch run is
not evidence for this branch.

## Validation limits

- Host runtime: tested only through the commands recorded here.
- PCSX2: not tested.
- Real PS2: not tested.
- PPSSPP: not tested.
- Real PSP: not tested.
- Performance: not measured; no performance claim is made.
