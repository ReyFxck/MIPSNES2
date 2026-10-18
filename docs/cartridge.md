# Cartridge

## Scope and current state

Platform code loads files and persists saves; the shared core sees caller-owned byte
buffers. The first cartridge foundation now provides:

- an explicit slot contract whose reads/writes can decline unmapped addresses;
- read-only ROM and writable SRAM views with no heap allocation;
- explicit base LoROM or HiROM selection by the caller;
- standard base mapping windows with deterministic power-of-two and bounded
  non-power-of-two mirroring;
- configuration/size rejection and synthetic host tests;
- integration through the SNES bus into CPU reset/fetch/timing;
- an immutable image view with explicit 512-byte copier-prefix policy;
- raw internal-header parsing at LoROM and HiROM candidates;
- mirrored whole-image checksum calculation and conservative base-map selection.

There is no file loader, extended-header resolver or automatic executable-cartridge
configuration. Supported base ROM sizes begin at 32 KiB for LoROM or 64 KiB for
HiROM and end at 4 MiB. A ROM may be a power of two or one larger power-of-two region
followed by one smaller power-of-two tail, provided the entire payload is aligned to
the selected mapper's bank size. The mapper precomputes the first-region size and
next power-of-two address span, then repeats only the smaller tail in the upper half
of that span. This covers the bounded `ABCC` and `ABCDEEEE` style layouts without
heap allocation or per-read shape analysis. Padding-dependent and board-specific
layouts remain rejected.
Supported SRAM is zero or a power of two up to 512 KiB for the LoROM address window
and 256 KiB for HiROM. These limits avoid pretending that one rule describes every
irregular multi-chip board.

Image analysis accepts a payload made from one or more 32 KiB units, independently
of whether the current base mapper can execute that layout. A source may be clean or
may contain exactly one 512-byte prefix. The caller selects `detect`, `reject` or
`require`; the view retains both source size and payload offset without copying.
Other remainders are rejected as malformed. This is a deliberately narrow policy,
not a claim that every historical copier format used identical metadata.

## Design direction

Validate size, candidate headers and documented mapper fields. Detect hardware, not
games. Handle LoROM, HiROM,
ExLoROM and ExHiROM separately. Checksums may aid diagnostics but must not invent
game-specific mapping. Platform filesystem code must remain outside `core/`.

The slot returns `false` when the selected cartridge hardware does not decode an
address. The SNES bus then preserves its open latch. ROM writes report a decoded
address but do not modify bytes; SRAM writes update the caller's buffer.

### Internal-header candidate analysis

The parser reads the 64-byte block beginning at payload offset `$7FC0` for an explicit
LoROM candidate or `$FFC0` for an explicit HiROM candidate. It returns the raw 21-byte
title, map mode, chipset, ROM/SRAM size exponents, destination code, developer ID,
version, checksum/complement and emulation-mode reset vector. Declared sizes are
decoded only when representable by `size_t`; exponent zero means no declared SRAM.

Map-mode classification recognizes only base `20h/30h` LoROM and `21h/31h` HiROM.
Everything else remains `OTHER`, including enhancement and Ex mapping values. The
parser reports whether checksum and complement XOR to `FFFFh`; parsing alone does not
endorse a candidate.

### Checksum and base-map selection

The checksum calculator sums the immutable ROM payload, never the optional copier
prefix. The selected candidate's four checksum bytes are normalized to `FF FF 00 00`,
which has the same byte sum as any valid checksum/complement pair. Power-of-two images
are summed once. If an image is the documented larger power-of-two region followed by
a smaller power-of-two region, the smaller tail is repeated until the combined span
reaches the next power of two. The reported mirrored span must equal the header's
declared ROM size.

For diagnostic completeness the calculator can zero-pad a tail whose size is not a
power of two before mirroring it. Hardware/file tools disagree on such malformed or
unusual shapes, so that result is never sufficient for confident automatic selection.
It remains useful for reporting and does not make the current mapper execute that
padding-dependent image.

The selector evaluates both header locations and accepts exactly one candidate only
when all of these independent gates pass:

- header location is present and its map mode matches that location;
- chipset is base ROM/RAM/battery class `00h` through `02h`;
- RESET vector is in the ROM-visible `$8000-$FFFF` half of bank zero;
- declared ROM size equals the calculated mirrored span;
- declared SRAM size is representable by the supported base map;
- checksum/complement form a valid pair and the full checksum matches;
- the payload is a power of two or an unambiguous sum of two power-of-two regions.

Zero passing candidates return `NO_CONFIDENT_HEADER`; two return `AMBIGUOUS_HEADER`.
There are no weighted scores, title/ASCII tests, first-opcode guesses, game names or
CRC exceptions. A successful result identifies base LoROM versus HiROM evidence only;
the caller still decides whether the current executable mapper supports the image.

### Base LoROM subset

LoROM uses 32 KiB ROM banks: cartridge ROM address bits preserve CPU A0-A14 and
shift the higher bank bits around CPU A15. The supported foundation maps ROM at
`00-3F/80-BF:8000-FFFF`, all of `40-6F/C0-EF`, and the upper halves of
`70-7D/F0-FF`. SRAM uses the lower halves of `70-7D/F0-FF`. Power-of-two ROMs mask
to their size. Supported irregular ROMs retain the larger region once, repeat the
smaller tail across the remaining half of the next power-of-two span and then mirror
that complete span through the base LoROM window.

### Base HiROM subset

HiROM uses 64 KiB ROM banks with CPU address lines connected in order. The supported
foundation maps ROM at `00-3F/80-BF:8000-FFFF` and all of `40-7D/C0-FF`. SRAM uses
8 KiB windows at `20-3F/A0-BF:6000-7FFF`. Banks `7E-7F` remain WRAM and are not
claimed by the cartridge.

Power-of-two ROMs mask to their size. Supported irregular ROMs use the same bounded
larger-region/smaller-tail policy as LoROM, with 64 KiB mapper-bank alignment.

## Verification criteria

`tests/test_cartridge.c` uses only generated byte arrays. It covers invalid pointers,
mapper values and unsupported sizes; LoROM/HiROM first/last bank bytes and mirrors;
96 KiB and 160 KiB LoROM tail repetition; 192 KiB HiROM tail repetition; mapper-bank
alignment; padding-dependent layout rejection; mapped/unmapped boundaries; SRAM
aliases/read/write; ROM write protection; 24-bit
normalization; open-latch preservation; and CPU reset/NOP through the integrated
cartridge bus. It proves explicit mapping, not header detection or file loading.

`tests/test_cartridge_image.c` covers clean, prefixed, required and rejected prefix
policies; malformed granularities; LoROM/HiROM offsets and fields; exponent overflow;
normalized power-of-two checksums; copier-prefix exclusion; unambiguous 96 KiB tail
mirroring; diagnostic padded-tail calculation; valid LoROM/HiROM selection; 32 KiB
LoROM-only bounds; arbitrary non-ASCII titles; checksum, pair, map, chipset, RESET and
size rejection; ambiguous dual headers; explicit no-match; invalid arguments; and a
selected 96 KiB LoROM candidate initializing the executable irregular mapper. All
fixtures are generated byte arrays with no commercial content.

Future detection tests must be independent of ROM titles. SRAM persistence needs
explicit save/error behavior and platform tests.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Partial items remain
unchecked until their complete verification gates are met.

- [ ] ROM loading
- [ ] [IN PROGRESS] ROM metadata — standard raw fields and base-map evidence only
- [ ] [IN PROGRESS] Header analysis — conservative base selection; extended variants absent
- [ ] [IN PROGRESS] LoROM — power-of-two/unambiguous two-region base map and conservative selection verified
- [ ] [IN PROGRESS] HiROM — power-of-two/unambiguous two-region base map and conservative selection verified
- [ ] ExLoROM
- [ ] ExHiROM
- [ ] [IN PROGRESS] SRAM — base mapper read/write windows verified
- [ ] SRAM persistence
- [ ] [IN PROGRESS] Mapper tests — synthetic power-of-two/irregular map/checksum/selection suite exists
- [ ] [IN PROGRESS] Checksum handling where useful — base mirrored checksum exists; special variants absent
- [ ] [IN PROGRESS] Truncated and malformed input rejection — mapper config and image/header bounds only
- [x] Copier-header policy — explicit detect/reject/require for a single 512-byte prefix
- [ ] Region detection

## Research and decisions

Sources:

- [Anomie's SNES memory mapping document, revision 1126](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt)
  establishes the console bus windows and that LoROM/HiROM are cartridge responses,
  not CPU address modes;
- [SNESdev Wiki memory map](https://snes.nesdev.org/wiki/Memory_map), inspected
  2026-10-17, describes the 32 KiB LoROM and 64 KiB HiROM address-line wiring;
- [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/),
  inspected 2026-10-17, supplies an independent hardware-board/decoder cross-check.
- [fullsnes cartridge ROM header specification](https://problemkaputt.de/fullsnes.htm#snescartridgeromheader),
  inspected 2026-10-18, documents candidate offsets, the 512-byte image-prefix rule,
  standard fields, size encoding, checksum pair, map modes and vectors;
- [SNESdev Wiki ROM header](https://snes.nesdev.org/wiki/ROM_header) and
  [ROM file formats](https://snes.nesdev.org/wiki/ROM_file_formats), inspected
  2026-10-18, provide the checksum mirroring process, header verification evidence and
  an independent file/header boundary cross-check. The checksum page cites Nintendo's
  development manual Book 1 pages 1-2-10 and 1-2-21; no manual copy is committed.
- [Super Famicom Development Wiki non-power-of-two ROM size](https://wiki.superfamicom.org/non-power-of-two-rom-size),
  inspected 2026-10-18, describes the larger-chip-first and smaller-chip repetition
  patterns conventionally written `ABCC` and `ABCDEEEE`;
- [SnesLab SNES ROM Header](https://sneslab.net/wiki/SNES_ROM_Header), inspected
  2026-10-18, independently records checksum mirroring for common 2.5 MiB and 6 MiB
  images and points to Nintendo development-manual material. The current executable
  mapper remains capped at 4 MiB, so the 6 MiB case is not claimed.

No emulator implementation was consulted or copied.

Intentional limitation: an unambiguous, mapper-bank-aligned two-region layout can be
checksummed, identified and executed by the supported base mapper policy. Padded or
multi-stage irregular shapes are diagnostic-only, and board-specific decoding is not
inferred from this generic rule. Extended/variant headers, Ex mappings, special
checksum conventions and special boards must add evidence and fixtures before those
tasks can be promoted.
