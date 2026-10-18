# Mode 25/35 ExHiROM audit

Date: 2026-10-18

## Scope

This record covers the first documented ExHiROM subset in MIPSNES2:

- raw 16-byte registration metadata immediately preceding an internal header;
- Mode `25h`/`35h` classification at payload offset `$40FFC0`;
- conservative three-candidate LoROM/HiROM/ExHiROM selection;
- caller-owned ExHiROM payloads larger than 4 MiB through 8 MiB;
- the first 4 MiB, extended region and system-bank upper-half mirrors;
- standard HiROM-style SRAM windows;
- deterministic 6 MiB tail repetition through an 8 MiB logical span;
- SNES-bus CPU reset and instruction fetch through the ExHiROM transform.

This is not a claim of complete cartridge compatibility or physical-board validation.
There is no file loader, automatic cartridge construction, SRAM persistence, special
chipset support or real-SNES cartridge test in this milestone.

## Evidence used

- Nintendo's *Super Nintendo Entertainment System Development Manual*, Book I,
  pages 1-2-14 through 1-2-17, places Mode 25 registration data at
  `$40FFB0-$40FFDF`, names the maker/game and special-purpose registration fields,
  and lists slow/fast map values `25h`/`35h`. The repository links the public manual
  index but does not redistribute the manual.
- [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/),
  inspected 2026-10-18, independently describes the two ExHiROM ROM regions, their
  bank-select behavior, upper-half mirrors and HiROM-style SRAM decoder.
- [SNESdev Wiki memory map](https://snes.nesdev.org/wiki/Memory_map), inspected
  2026-10-18, supplies an independent community cross-check for logical SNES windows.
- [SnesLab SNES ROM Header](https://sneslab.net/wiki/SNES_ROM_Header), inspected
  2026-10-18, records the common 6 MiB checksum-mirroring case.

No emulator implementation source was consulted or copied. All tests use generated
byte arrays and original expectations derived from the cited hardware documentation.

## Executable address contract

| Logical SNES range | Payload region |
|---|---|
| `C0-FF:0000-FFFF` | First region, offsets `$000000-$3FFFFF` |
| `80-BF:8000-FFFF` | Upper-half mirrors of the first region |
| `40-7D:0000-FFFF` | Extended region beginning at offset `$400000` |
| `00-3F:8000-FFFF` | Upper-half mirrors of the extended region |
| `20-3F/A0-BF:6000-7FFF` | Caller-owned SRAM, using the HiROM-style window |
| `7E-7F` | Declined by the cartridge because these banks are WRAM |

The mapper requires 64 KiB alignment. Images of 4 MiB or smaller belong to the base
mappers and are rejected as ExHiROM; images larger than 8 MiB are rejected. A payload
may be a power of two or one larger power-of-two first region plus one smaller
power-of-two tail. For 6 MiB, the 4 MiB first region is retained and the 2 MiB tail
repeats once across the remainder of the 8 MiB logical span. Layouts requiring
arbitrary padding remain unsupported.

## Header and selection contract

The parser exposes registration bytes as raw metadata. It does not require printable
maker/game strings and does not use them as a confidence score. A generic selection
succeeds only when exactly one of the LoROM, HiROM and ExHiROM candidates passes all
existing independent gates: location/map agreement, supported base chipset class,
ROM-visible reset vector, declared mirrored size, supported SRAM size, valid and
matching checksum, unambiguous checksum layout, and executable mapper size/alignment.
Zero matches fail explicitly and multiple matches remain ambiguous.

The earlier base-only selector remains for callers that intentionally exclude
ExHiROM. Successful analysis does not itself configure or allocate a cartridge.

## Generated verification fixtures

`tests/test_cartridge.c` verifies first/extended regions, upper-half mirrors, 6 MiB
tail repetition, ROM write protection, declined holes/WRAM, SRAM aliases, invalid
size bounds and CPU reset/NOP timing through the integrated SNES bus.

`tests/test_cartridge_image.c` verifies raw registration fields, `$40FFC0` bounds,
both `25h` and `35h`, 6 MiB mirrored checksum/declared 8 MiB span, all selection
gates, executable mapper initialization and rejection of an oversized base-map
candidate. No commercial ROM or external binary fixture is present.

## Verification record

The following local checks passed from the repository root with GCC on 2026-10-18:

- `make clean && make test`;
- `make BUILD_DIR=build/release-local CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-local CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test`;
- `python3 tools/check_repository.py`;
- `git diff --check`.

Leak detection was disabled only for the local sanitizer runtime environment; ASan
and UBSan remained enabled. Local Clang, PS2SDK and PSPSDK executables were not
available. GitHub Host/PS2/PSP workflow identifiers are added only after those exact
jobs succeed. Real PS2, real PSP, PCSX2, PPSSPP and physical SNES cartridge
validation were not performed.

## Deliberate exclusions

- Unofficial ExLoROM is not implemented. Community documentation describes it as a
  rare unofficial mapping, so it remains research-only until a concrete compatibility
  need and stronger hardware evidence justify a separate design.
- Special cartridge boards and enhancement chips remain separate hardware.
- FastROM map bit `35h` does not change the cartridge address transform; the partial
  SNES bus continues to own `$420D` timing selection.
- This milestone makes no PS2/PSP speed, compatibility or real-hardware claim.
