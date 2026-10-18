# Cartridge Checksum and Base-Mapper Selection Audit

Date: 2026-10-18

## Scope

This milestone adds original, heap-free C99 analysis for:

- normalized whole-payload SNES checksum calculation;
- exclusion of the optional 512-byte copier prefix;
- power-of-two images and documented smaller-tail mirroring;
- explicit diagnostic zero-padding for otherwise ambiguous tail shapes;
- independent LoROM and HiROM candidate reports;
- conservative selection of exactly one supported base mapper;
- explicit no-confident-header and ambiguous-header results.

It does not load files, configure the executable cartridge mapper, implement physical
irregular board wiring, parse extended/Ex headers, support enhancement chips or use
game identity. It does not make MIPSNES2 a working SNES emulator.

## Sources and independent implementation

The checksum field normalization and all-byte 16-bit sum follow the
[fullsnes cartridge ROM header specification](https://problemkaputt.de/fullsnes.htm#snescartridgeromheader),
inspected 2026-10-18. The [SNESdev Wiki ROM header](https://snes.nesdev.org/wiki/ROM_header)
and [ROM file formats](https://snes.nesdev.org/wiki/ROM_file_formats), inspected the
same day, document larger-first power-of-two regions, smaller-tail repetition, copier
prefix exclusion and header-verification evidence. The ROM-header page attributes the
checksum organization to Nintendo's development manual Book 1 pages 1-2-10 and
1-2-21; no manual, emulator source, external binary or commercial ROM is committed.

The implementation was written from those behavior descriptions. It does not use the
emulator implementations cited by the community wiki.

## Selection contract

There is no weighted heuristic score. A candidate is confident only when its map mode,
base chipset class, ROM-visible RESET vector, declared ROM span, supported base SRAM
size, checksum/complement pair, calculated checksum and unambiguous image layout all
agree. Title bytes and the opcode at RESET are deliberately not inputs. If both
candidates pass, the API returns ambiguity; if neither passes, it returns no match.

The checksum calculator exposes the calculated value and mirrored span separately
from selection. A tail which is not itself a power of two is zero-padded for a
diagnostic result, but that uncertain shape fails the confidence gate. An unambiguous
two-region checksum result still does not imply that the current power-of-two-only
executable mapper can run the image.

## Deterministic coverage

`tests/test_cartridge_image.c` uses generated arrays only and covers:

- checksum fields normalized to `FF FF 00 00`;
- known all-zero checksum value and immunity to stored checksum-field contents;
- copier-prefix exclusion;
- power-of-two, 96 KiB mirrored-tail and 224 KiB diagnostic-padding cases;
- valid LoROM, HiROM, prefixed, irregular and 32 KiB LoROM-only candidates;
- arbitrary non-ASCII title bytes without title scoring;
- checksum-data mismatch and invalid complement pair as distinct failures;
- wrong map mode, RESET below `$8000`, declared-size mismatch, enhancement chipset,
  unsupported SRAM declaration and uncertain padded layout;
- dual-valid ambiguity, no-match and invalid-argument/bounds results.

## Verification

The implementation revision passed locally with GCC through:

```text
make clean && make test
make BUILD_DIR=build/release clean
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize clean
ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
git diff --check
python3 tools/check_repository.py
```

The default, `-DNDEBUG` release and ASan+UBSan suites each reported
`PASS: cartridge image checksum and base-mapper selection` and all existing host
regressions passed. Leak detection alone was disabled for the existing local
environment restriction. Clang was unavailable.

Local `make -C platform/ps2 clean all` stopped before compilation because `PS2SDK`
was unset. Local `make -C platform/psp clean all` stopped before compilation because
`psp-config` was unavailable. No runtime or performance claim is implied by host tests
or cross-compilation.

## GitHub Actions

All three pull-request workflows passed for implementation commit
`b394209825d6c78ed2e08f86d7c142158e52ba6c`:

- [Host CI run 39139163385](https://github.com/ReyFxck/MIPSNES2/actions/runs/39139163385)
  — GCC and Clang release/sanitizer jobs succeeded and staged artifacts;
- [PS2 CI run 39139163219](https://github.com/ReyFxck/MIPSNES2/actions/runs/39139163219)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded;
- [PSP CI run 39139163241](https://github.com/ReyFxck/MIPSNES2/actions/runs/39139163241)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded.

The documentation-evidence commit must also pass all applicable workflows before
merge. Cross-compilation is not console execution.

## Runtime and performance

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No benchmark,
compatibility or full-speed claim is made. RFAuds2 remains unrelated future PS2-only
PCM output infrastructure.
