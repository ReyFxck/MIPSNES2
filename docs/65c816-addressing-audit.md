# 65C816 addressing/load-store audit

Audit date: 2026-10-10. Branch: `feature/65c816-addressing-load-store`, stacked on
`feature/65c816-foundation` while pull request #2 remains open.

## Audited scope

This milestone adds absolute loads, absolute X/Y indexed loads and accumulator
stores, absolute-long loads/stores and absolute-long X-indexed loads/stores. The
implemented set grows from 14 to 27 opcodes. It also corrects data-word and indexed
address calculation to advance linearly through the 24-bit data space while keeping
PBR:PC instruction fetch bank-local.

Direct Page, indirect and stack-relative modes remain outside this milestone. No
machine, cartridge loader, scheduler, interrupt path, PPU, APU, DMA, assembly,
RFAuds2 runtime integration or ROM execution was added.

## Source evidence

The implementation was independently derived from the Western Design Center
[W65C816S Datasheet dated March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- section 3.5 defines the continuous 24-bit data space and cross-bank data;
- sections 3.5.1 and 3.5.3-3.5.7 define absolute, indexed and long addresses;
- Tables 5-4, 5-5 and 5-7 define opcodes and base cycles;
- Table 5-7 notes 1 and 4 define width and indexed page/write/X penalties;
- caveats 7.3 and 7.5 clarify indexed bank carry and the optional indexed cycle.

No emulator implementation or external test ROM was used.

## Verification record

The first full `make clean && make test` after adding the documentation link stopped
at repository consistency because this audit file did not yet exist. CPU compilation
and the earlier CPU/bus test execution had passed. This was a documentation ordering
failure, not an emulation assertion; the final rerun and all other results are recorded
below.

Environment:

- Ubuntu host, GCC 13.3.0, GNU Make 4.3, Python 3.12.14;
- pinned PS2 toolchain `v2.0.0`, cross GCC 15.2.0;
- pinned PSP toolchain `v20261001`, cross GCC 15.2.0.

| Command | Result |
|---|---|
| `make clean && make test` | Pass after the documented link-order rerun |
| `make BUILD_DIR=build/release CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test` | Pass |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-gcc CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' clean test` | Pass with ASan+UBSan; local LeakSanitizer remains disabled for the previously documented `/proc` restriction |
| `make -C platform/ps2 clean all` with the pinned PS2 environment | Pass; CPU compiles and links into `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with the pinned PSP environment | Pass; CPU compiles and links into ELF/PRX/EBOOT.PBP |

The first GitHub Actions execution for implementation commit
`487fe6b064d2112e3f88f5c0f8b4ad48aa48ae98` also passed:

| Workflow | Verified result |
|---|---|
| [Host CI run 38078935577](https://github.com/ReyFxck/MIPSNES2/actions/runs/38078935577) | GCC and Clang release builds/tests, sanitizer builds/tests and host artifact upload passed |
| [PS2 CI run 38078935650](https://github.com/ReyFxck/MIPSNES2/actions/runs/38078935650) | Pinned-toolchain cross-build, artifact staging, SHA-256 generation and upload passed |
| [PSP CI run 38078935663](https://github.com/ReyFxck/MIPSNES2/actions/runs/38078935663) | Pinned-toolchain cross-build, artifact staging, SHA-256 generation and upload passed |

Clang remains unavailable locally; GitHub CI is the Clang/full-sanitizer gate. No
PCSX2, PPSSPP, real PS2 or real PSP runtime test was performed. Cross-compilation
does not establish runtime correctness. No performance measurement was made.

## Audit boundaries

- unimplemented opcodes still enter the sticky diagnostic stop;
- no platform SDK or RFAuds2 dependency enters `core/`;
- cycle totals are processor base cycles, not SNES master time;
- no performance, compatibility or target-runtime claim is made;
- `STATUS.md` keeps the interpreter, addressing families and timing model incomplete.
