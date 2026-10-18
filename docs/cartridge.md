# Cartridge

## Scope and current state

Platform code loads files and persists saves; the shared core sees caller-owned byte
buffers. The first cartridge foundation now provides:

- an explicit slot contract whose reads/writes can decline unmapped addresses;
- read-only ROM and writable SRAM views with no heap allocation;
- explicit LoROM, HiROM or documented Mode 25/35 ExHiROM selection by the caller;
- standard mapper windows with deterministic power-of-two and bounded
  non-power-of-two mirroring within the verified size limits;
- configuration/size rejection and synthetic host tests;
- integration through the SNES bus into CPU reset/fetch/timing;
- an immutable image view with explicit 512-byte copier-prefix policy;
- raw registration/internal-header parsing at LoROM, HiROM and ExHiROM candidates;
- mirrored whole-image checksum calculation and conservative mapper selection.
- an allocation-free plan/construction contract over caller-owned ROM/SRAM storage.
- revisioned dirty-state handoff for platform-owned SRAM persistence.

There is no file loader, board resolver or automatic executable-cartridge
allocation. A preparation call can connect successful analysis to executable-cartridge
configuration, but the platform still owns file acquisition and storage lifetime.
Supported base ROM sizes begin at 32 KiB for LoROM or 64 KiB for
HiROM and end at 4 MiB; ExHiROM accepts 64 KiB-aligned payloads larger than 4 MiB
through 8 MiB. A ROM may be a power of two or one larger power-of-two region followed
by one smaller power-of-two tail, provided the entire payload is aligned to the
selected mapper's bank size. The mapper precomputes the first-region size and next
power-of-two address span, then repeats only the smaller tail in the upper half of
that span. This covers the bounded `ABCC` and `ABCDEEEE` style layouts without heap
allocation or per-read shape analysis. Padding-dependent and board-specific layouts
remain rejected.
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
games. Handle LoROM, HiROM and ExHiROM separately. Unofficial ExLoROM remains
research-only until a commercial-library requirement and hardware evidence justify
it. Checksums may aid diagnostics but must not invent game-specific mapping. Platform
filesystem code must remain outside `core/`.

The slot returns `false` when the selected cartridge hardware does not decode an
address. The SNES bus then preserves its open latch. ROM writes report a decoded
address but do not modify bytes; SRAM writes update the caller's buffer.

### SRAM persistence handoff

The mapper exposes a live, const view of its caller-owned SRAM buffer, byte count,
dirty state and revision token. Initialization starts clean at revision zero. A
decoded SRAM write advances to a nonzero revision and becomes dirty only when its
stored byte actually changes. Mirrored addresses share storage and therefore share
one revision history; ROM and unmapped writes cannot dirty SRAM.

Platform code may record the revision it is saving and acknowledge that exact token
only after durable output succeeds. A newer emulated write makes the old token stale,
so acknowledgement fails and preserves dirty state. Revision wrap skips zero. The
view is not a snapshot: asynchronous backends must copy it or coordinate emulation.
The core does not select paths, perform I/O, define an on-disk format or schedule
saves.

### Internal-header candidate analysis

The parser reads the 64-byte block beginning at payload offset `$7FC0` for an explicit
LoROM candidate, `$FFC0` for HiROM or `$40FFC0` for ExHiROM. It also reads the raw
16-byte registration block immediately preceding the title: maker code, game code,
fixed/reserved bytes, expansion RAM size, special version and cartridge sub-number.
These bytes are diagnostic metadata, not ASCII or game-identity confidence gates.
The common header returns the raw 21-byte title, map mode, chipset, ROM/SRAM size
exponents, destination code, developer ID, version, checksum/complement and
emulation-mode reset vector. Declared sizes are decoded only when representable by
`size_t`; exponent zero means no declared SRAM.

Map-mode classification recognizes base `20h/30h` LoROM, `21h/31h` HiROM and
`25h/35h` ExHiROM. Everything else remains `OTHER`, including enhancement-map
values. The parser reports whether checksum and complement XOR to `FFFFh`; parsing
alone does not endorse a candidate.

### Checksum and mapper selection

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

The generic selector evaluates all three header locations and accepts exactly one
candidate only
when all of these independent gates pass:

- header location is present and its map mode matches that location;
- chipset is base ROM/RAM/battery class `00h` through `02h`;
- RESET vector is in the ROM-visible `$8000-$FFFF` half of bank zero;
- declared ROM size equals the calculated mirrored span;
- declared SRAM size is representable by the supported base map;
- checksum/complement form a valid pair and the full checksum matches;
- the payload is a power of two or an unambiguous sum of two power-of-two regions;
- the mapper can execute that payload size and bank alignment.

Zero passing candidates return `NO_CONFIDENT_HEADER`; multiple matches return
`AMBIGUOUS_HEADER`.
There are no weighted scores, title/ASCII tests, first-opcode guesses, game names or
CRC exceptions. The older base-only selector remains available for callers that
intentionally exclude ExHiROM. Selection and executable cartridge initialization are
still separate operations.

### Preparation and construction

`mipsnes2_cartridge_plan_create` accepts an entire caller-owned source buffer and the
explicit copier-prefix policy. It stores the immutable image view, candidate evidence,
selected mapper and header-declared SRAM requirement. It neither allocates nor copies
the ROM. Analysis failures leave the destination plan unchanged.

`mipsnes2_cartridge_construct` binds that plan to caller-owned SRAM. Capacity may
exceed the declared size, but only the declared bytes become visible through the
mapper. Missing/undersized SRAM and inconsistent plans are rejected. Construction
failures leave the destination cartridge unchanged. The caller must keep source and
SRAM buffers alive for the cartridge lifetime.

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

### Documented Mode 25/35 ExHiROM subset

ExHiROM is a separate address transform, not oversized base HiROM. The supported
subset accepts payloads larger than 4 MiB through 8 MiB. Banks `C0-FF` map the first
4 MiB in full, and `80-BF:8000-FFFF` provide their system-bank upper-half mirrors.
Banks `40-7D` map the extended region starting at payload offset 4 MiB, while
`00-3F:8000-FFFF` provide the corresponding upper-half mirrors. Banks `7E-7F` remain
WRAM and are not claimed. Standard HiROM-style SRAM windows at
`20-3F/A0-BF:6000-7FFF` are retained for this verified subset.

A 6 MiB image consists of a 4 MiB first region plus a 2 MiB power-of-two tail. The
tail repeats through the upper half of the 8 MiB logical span; no zero padding or
game-specific layout exception is inferred. Map modes `25h` and `35h` identify the
slow and fast variants at `$40FFC0`. FastROM affects SNES-bus access timing through
`$420D`; it does not change the cartridge address transform.

## Verification criteria

`tests/test_cartridge.c` uses only generated byte arrays. It covers invalid pointers,
mapper values and unsupported sizes; LoROM/HiROM/ExHiROM bank bytes and mirrors;
96 KiB and 160 KiB LoROM tail repetition; 192 KiB HiROM tail repetition; mapper-bank
alignment; padding-dependent layout rejection; mapped/unmapped boundaries; SRAM
aliases/read/write; ROM write protection; 24-bit normalization; open-latch
preservation; and CPU reset/NOP through both base LoROM and ExHiROM integrated
cartridge buses. It proves explicit mapping, not file loading or physical
cartridge-board validation.

`tests/test_cartridge_image.c` covers clean, prefixed, required and rejected prefix
policies; malformed granularities; LoROM/HiROM/ExHiROM offsets, registration fields
and common fields; exponent overflow;
normalized power-of-two checksums; copier-prefix exclusion; unambiguous 96 KiB tail
mirroring; diagnostic padded-tail calculation; valid LoROM/HiROM selection; 32 KiB
LoROM-only bounds; arbitrary non-ASCII titles; checksum, pair, map, chipset, RESET and
size rejection; multiple-candidate ambiguity; explicit no-match; invalid arguments;
a selected 96 KiB LoROM candidate initializing the executable irregular mapper; and
a selected 6 MiB Mode 25/35 ExHiROM candidate initializing the executable mapper.
All fixtures are generated byte arrays with no commercial content.

`tests/test_cartridge_prepare.c` covers analysis error propagation, unchanged failure
outputs, clean/prefixed source ownership, zero/required SRAM construction, capacity
checks, tampered-plan rejection and selected LoROM/ExHiROM mapper access.

The cartridge regression also covers initial/no-SRAM persistence views,
value-changing and same-value writes, mirrored aliases, stale/matching
acknowledgement, unrelated writes and revision wrap. Platform file/save-error
behavior and real-device tests remain absent. Future detection tests must remain
independent of ROM titles.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Partial items remain
unchecked until their complete verification gates are met.

- [ ] [IN PROGRESS] ROM loading — common preparation/construction exists; platform file adapters absent
- [ ] [IN PROGRESS] ROM metadata — standard and registration fields parsed; special variants absent
- [ ] [IN PROGRESS] Header analysis — conservative LoROM/HiROM/ExHiROM selection; special variants absent
- [ ] [IN PROGRESS] LoROM — power-of-two/unambiguous two-region base map and conservative selection verified
- [ ] [IN PROGRESS] HiROM — power-of-two/unambiguous two-region base map and conservative selection verified
- [ ] ExLoROM — unofficial mapping remains research-only
- [ ] [IN PROGRESS] ExHiROM — Mode 25/35 subset verified; board/runtime validation absent
- [ ] [IN PROGRESS] SRAM — mapper read/write windows and revisioned dirty tracking verified
- [ ] [IN PROGRESS] SRAM persistence — shared handoff verified; platform file backends absent
- [ ] [IN PROGRESS] Mapper tests — synthetic base/irregular/ExHiROM map/checksum/selection suite exists
- [ ] [IN PROGRESS] Checksum handling where useful — base mirrored checksum exists; special variants absent
- [ ] [IN PROGRESS] Truncated and malformed input rejection — mapper config and image/header bounds only
- [x] Copier-header policy — explicit detect/reject/require for a single 512-byte prefix
- [ ] Region detection
- [x] Platform-neutral image-to-cartridge construction contract
- [ ] [IN PROGRESS] SRAM dirty/revision persistence handoff — GCC default/release/sanitizer host suites pass; cross-build CI pending

## Research and decisions

Sources:

- [Anomie's SNES memory mapping document, revision 1126](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt)
  establishes the console bus windows and that LoROM/HiROM are cartridge responses,
  not CPU address modes;
- [SNESdev Wiki memory map](https://snes.nesdev.org/wiki/Memory_map), inspected
  2026-10-17, describes the 32 KiB LoROM and 64 KiB HiROM address-line wiring;
- [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/),
  inspected 2026-10-18, supplies an independent hardware-board/decoder cross-check,
  including the two ExHiROM ROM regions and their address-line selection;
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
  images and points to Nintendo development-manual material;
- [SNES Development Manual index](https://snes.nesdev.org/wiki/SNES_Development_Manual),
  inspected 2026-10-18, identifies the manufacturer manual used for the registration
  block and Map Mode 25/35 layout. Book I pages 1-2-14 through 1-2-17 place Mode 25
  registration data at `$40FFB0-$40FFDF` and define `25h/35h`. No manual copy is
  committed because redistribution permission has not been established;
- [SnesLab ExLoROM](https://sneslab.net/wiki/ExLoROM), inspected 2026-10-18, records
  ExLoROM as a rare unofficial community mapping. It is not implemented or inferred
  from ExHiROM.

No emulator implementation was consulted or copied.

Intentional limitation: an unambiguous, mapper-bank-aligned two-region layout can be
checksummed, identified and executed by the supported LoROM/HiROM/ExHiROM policy.
Padded or multi-stage irregular shapes are diagnostic-only, and board-specific
decoding is not inferred from this generic rule. Unofficial ExLoROM, special checksum
conventions and special boards must add evidence and fixtures before those tasks can
be promoted.
