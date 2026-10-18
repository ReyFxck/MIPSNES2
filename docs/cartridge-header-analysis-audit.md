# Cartridge Image and Internal-Header Analysis Audit

Date: 2026-10-17

## Scope

This milestone adds original, heap-free C99 image analysis without file I/O or
automatic hardware selection:

- immutable clean/prefixed ROM payload views;
- explicit `detect`, `reject` and `require` policy for a 512-byte copier prefix;
- 32 KiB payload-granularity and candidate-bound validation;
- caller-selected LoROM `$7FC0` or HiROM `$FFC0` internal-header parsing;
- raw standard metadata, reset vector and overflow-safe declared-size decoding;
- base map-mode and checksum/complement relationship classification;
- deterministic synthetic host tests and PS2/PSP cross-build inclusion.

It does not open files, score candidates, automatically select a mapper, interpret
extended/variant headers, calculate the full ROM checksum, identify enhancement
hardware or make irregular ROM boards executable.

## Sources and implementation boundary

The field layout, image offsets and size encodings are based on the
[fullsnes cartridge ROM header specification](https://problemkaputt.de/fullsnes.htm#snescartridgeromheader),
inspected 2026-10-17. The [SNESdev Wiki ROM header](https://snes.nesdev.org/wiki/ROM_header)
and [ROM file formats](https://snes.nesdev.org/wiki/ROM_file_formats), also inspected
2026-10-17, provide an independent hardware-research/file-format cross-check.

No emulator implementation, commercial ROM or external binary fixture was consulted,
copied or committed.

The prefix decision is intentionally explicit. A clean source must contain one or
more complete 32 KiB units. A prefixed source must contain exactly 512 leading bytes
plus that payload shape. The view records the offset and never modifies/copies the
source. This is narrower than accepting every historical copier variant and therefore
does not silently normalize unknown layouts.

## Verification

`tests/test_cartridge_image.c` covers:

- null arguments, invalid policy, empty/truncated and invalid-remainder rejection;
- detect/reject/require outcomes for clean and 512-byte-prefixed sources;
- retained source size, payload size, data pointer and prefix offset;
- a 96 KiB payload accepted for analysis while mapper support remains separate;
- LoROM and HiROM candidate offsets and out-of-range rejection;
- raw 21-byte title, map/chip/size/destination/developer/version fields;
- little-endian checksum, complement and reset-vector extraction;
- base LoROM/HiROM and FastROM classification;
- valid and invalid checksum/complement pairs;
- zero SRAM declaration and overflow-safe hostile exponents;
- neutral parsing of a caller-selected wrong candidate.

The following commands passed locally:

```text
make clean && make test
make BUILD_DIR=build/release clean
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize clean
ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
git diff --check
python3 tools/check_repository.py
```

The new suite reported
`PASS: cartridge image and internal-header analysis foundation` in default, release
and ASan+UBSan configurations. Leak detection alone was disabled for the existing
local environment restriction. Clang was unavailable. Local PS2 and PSP commands
stopped before compilation because PS2SDK and `psp-config` were unavailable.
Repository, modified-source marker, platform-API leakage and RFAuds2-leakage checks
passed.

## GitHub Actions

All three pull-request workflows passed for implementation commit
`add0e83ae5c5aee1680794ea420314cac3aa2b57`:

- [Host CI run 39137517768](https://github.com/ReyFxck/MIPSNES2/actions/runs/39137517768)
  — GCC and Clang release/sanitizer jobs succeeded and staged artifacts;
- [PS2 CI run 39137517776](https://github.com/ReyFxck/MIPSNES2/actions/runs/39137517776)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded;
- [PSP CI run 39137517750](https://github.com/ReyFxck/MIPSNES2/actions/runs/39137517750)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded.

The documentation-evidence commit must also pass all applicable workflows before
merge. Cross-compilation is not console execution.

## Runtime and performance

No PCSX2, real PS2, PPSSPP or real PSP runtime test was performed. No commercial ROM
was loaded. No benchmark, compatibility or full-speed claim is made. RFAuds2 remains
unrelated future PS2-only PCM output infrastructure.
