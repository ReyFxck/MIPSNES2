# Hardware and Toolchain References

This is MIPSNES2's curated starting point for hardware behavior, independent tests
and target SDK APIs. It intentionally contains **no emulator source code or emulator
implementation documentation**. A link here permits research; it does not permit
copying code, redistributing copyrighted manuals or treating an unsourced statement
as hardware truth.

## Evidence order

When sources disagree, investigate and record the conflict instead of silently
choosing the most convenient answer. Prefer, in order:

1. repeatable observation on identified real hardware;
2. manufacturer datasheets/manuals and current SDK headers;
3. independent hardware test programs with source, provenance and expected output;
4. independent hardware research documents;
5. community development documentation.

Pin repository revision, file/blob hash and license before importing any test asset.
Do not commit commercial ROMs, console firmware, proprietary SDKs or manuals whose
redistribution is not clearly permitted. A test passing in PCSX2 or PPSSPP is not a
substitute for real PS2 or PSP evidence.

## 65C816 and SNES CPU

- [WDC W65C816S datasheet (March 13, 2024)](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf) — primary register, instruction, addressing, flag, signal and processor-cycle reference. Record the revision date because WDC updates this PDF in place.
- [WDC documentation index](https://www.westerndesigncenter.com/wdc/documentation.php) — manufacturer index for the W65C02/W65C816 assembly programming manual and related datasheets.
- [SNESdev Wiki: CPU and hardware contents](https://snes.nesdev.org/wiki/SNESdev_Wiki) — navigation hub for Ricoh 5A22 differences, vectors, timing and development practice. Community-maintained; confirm edge cases with tests or stronger evidence.
- [SNESdev Wiki: timing](https://snes.nesdev.org/wiki/Timing) — 5A22 master-clock conversion and 6/8/12-master-clock access classes. This complements rather than replaces WDC processor-cycle tables.
- [SNESdev Wiki: version differences](https://snes.nesdev.org/wiki/Version_differences) — revision-specific S-CPU/PPU behavior that must be kept distinct from universal behavior.

## SNES bus, cartridge and registers

- [SNESdev Wiki: memory map](https://snes.nesdev.org/wiki/Memory_map) — WRAM, MMIO, cartridge windows, mirrors and access-speed overview.
- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt) — pinned independent hardware-research map used for WRAM, system-bank route and 6/8/12-clock boundaries. Revision `eeb98f1e85b7db6574ca0804455819ec81117198`; review provenance again before importing any asset (no asset is committed).
- [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/) — hardware-board and address-decoder explanation used to cross-check LoROM/HiROM/ExHiROM wiring. It is not an emulator implementation; special boards still require board-specific evidence.
- [SNESdev Wiki: ROM header](https://snes.nesdev.org/wiki/ROM_header) — header locations and mapper-related metadata. Header heuristics are not a license for per-game hardcoding.
- [SNESdev Wiki: ROM file formats](https://snes.nesdev.org/wiki/ROM_file_formats) — copier-header and file-layout context for the future loader.
- [Super Famicom Development Wiki: non-power-of-two ROM size](https://wiki.superfamicom.org/non-power-of-two-rom-size) — community description of larger-chip-first, smaller-chip mirroring (`ABCC`, `ABCDEEEE`) used only for the explicitly bounded irregular base-map policy; board-specific exceptions still require stronger evidence.
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm) — independently written, broad SNES register/timing/mapping reference. It is a hardware document, not an emulator implementation; verify disputed or revision-sensitive details on hardware.
- [Super Famicom Development Wiki: registers](https://wiki.superfamicom.org/registers) — searchable community register reference derived from hardware research documents. Check attribution and primary evidence for edge cases.
- [SNES Development Manual index](https://snes.nesdev.org/wiki/SNES_Development_Manual) — description/index for Nintendo's two-book licensed-developer manual. Book I pages 1-2-14 through 1-2-17 define the 16-byte registration prefix, Mode 25's `$40FFB0-$40FFDF` registration/header location and `25h/35h` map values. Do not add a manual copy to this repository without verified redistribution rights.
- [SnesLab: ExLoROM](https://sneslab.net/wiki/ExLoROM) — community evidence that ExLoROM is a rare unofficial mapping rather than a Nintendo-defined peer of ExHiROM. MIPSNES2 keeps it research-only pending a demonstrated compatibility need and stronger hardware evidence.

## PPU, DMA and timing

- [SNESdev Wiki: SNES PPU for NES developers](https://snes.nesdev.org/wiki/SNES_PPU_for_NES_developers) — useful conceptual PPU orientation, not a complete behavior contract.
- [SNESdev Wiki: timing](https://snes.nesdev.org/wiki/Timing) — scanline/frame clocks and CPU access timing.
- [SNESdev Wiki: version differences](https://snes.nesdev.org/wiki/Version_differences) — PPU and DMA/HDMA revision differences.
- [Anomie's document index](https://sneslab.net/wiki/List_of_Anomie%27s_Docs) — independent hardware research covering registers, timing, memory mapping, open bus/wrapping, SPC700 and S-DSP. Preserve document title/revision when citing a specific file.

## SPC700, S-DSP and audio

- [SNESdev Wiki: S-SMP](https://snes.nesdev.org/wiki/S-SMP) — audio CPU, RAM, IPL and timer overview.
- [SNESdev Wiki: S-SMP register table](https://snes.nesdev.org/wiki/APU_register_table/SMP) — SPC700-side MMIO register map, including undocumented fields that require independent confirmation.
- [SNESdev Wiki: S-DSP registers](https://snes.nesdev.org/wiki/S-DSP_registers) — register-oriented S-DSP starting point.
- [Super Famicom Development Wiki: SPC700 reference](https://wiki.superfamicom.org/spc700-reference) — instruction/register reference cross-check.
- [Anomie's document index](https://sneslab.net/wiki/List_of_Anomie%27s_Docs) — includes dedicated SPC700 cycle, boot ROM and S-DSP research documents.
- [RFAuds2](https://github.com/ReyFxck/RFAuds2) — preferred experimental future PS2 PCM transport only. Pin a known revision before integration; never use it as an S-DSP timing or behavior source and never copy it into the shared core.

## Independent SNES tests

These are tests and vectors, not emulator implementations. Before committing any
asset, re-check its license at the exact pinned revision and add a provenance record.

- [SingleStepTests/65816](https://github.com/SingleStepTests/65816) — language-neutral JSON instruction transitions with bus activity. The decimal milestone audited revision `307c63f5673c8c6b861c52f71ddb89212a227480`; a corpus vector is evidence to investigate, not automatic proof over real hardware.
- [gilyon/snes-tests](https://github.com/gilyon/snes-tests) — source and ROM tests for 65816 and SPC700 behavior; currently MIT-licensed upstream. Prefer building from a pinned source revision.
- [undisbeliever/snes-test-roms](https://github.com/undisbeliever/snes-test-roms) — real-hardware-oriented SNES tests including PPU/DMA edge cases; currently zlib-licensed upstream. Preserve reference images/results and hardware revision metadata.
- [SNESdev Wiki test-ROM index](https://snes.nesdev.org/wiki/Emulator_tests) — discovery index only. Each linked test still needs its own provenance, license and expected real-hardware result before use.

## PlayStation 2 target

- [PS2SDK documentation](https://ps2dev.github.io/ps2sdk/) and [PS2SDK source/API headers](https://github.com/ps2dev/ps2sdk) — authoritative for the open-source SDK interfaces MIPSNES2 actually compiles against. Pin releases/commits; do not invent APIs from memory.
- [ps2toolchain](https://github.com/ps2dev/ps2toolchain) — toolchain build source and version basis used by CI pinning.
- [ps2dev portal](https://ps2dev.github.io/) — maintained installation and development entry point.
- [ps2tek](https://israpps.github.io/ps2tek/) — independent EE, DMAC, GIF, GS, VIF, VU, IOP and SPU2 hardware documentation. Confirm hardware-sensitive optimization claims with real-console measurements.
- [unknownbrackets/ps2autotests](https://github.com/unknownbrackets/ps2autotests) — test programs with captured PS2 results. Treat license and per-test provenance as mandatory before reuse.

Do not add proprietary Sony SDK files, BIOS images or questionably redistributed
manuals to MIPSNES2. Public hardware research can guide an original implementation;
SDK calls must still be verified against the pinned PS2SDK revision.

## PlayStation Portable target

- [PSPSDK documentation](https://pspdev.github.io/pspsdk/) and [PSPSDK source/API headers](https://github.com/pspdev/pspsdk) — open-source SDK API source of truth for PSP scaffolding and future backends.
- [psptoolchain](https://github.com/pspdev/psptoolchain) — toolchain build source and pinning basis.
- [PSP VFPU documentation](https://pspdev.github.io/vfpu-docs/) and [source repository](https://github.com/pspdev/vfpu-docs) — independent Allegrex/VFPU instruction documentation. Profile first; it does not imply VFPU is suitable for scalar 65C816 work.
- [PSP Developer Wiki: Allegrex](https://www.psdevwiki.com/psp/Allegrex) — community hardware cross-reference for the CPU. Validate uncertain behavior on real PSP hardware.

Do not use PPSSPP behavior as final timing/performance evidence and do not source the
MIPSNES2 implementation from emulator code.

## Citation checklist

For every behavior-sensitive change, record:

- exact source title, URL and revision/date;
- table, section, register or test case used;
- whether the source is manufacturer, hardware measurement, independent test or community documentation;
- any conflicting sources and how the result was resolved;
- hardware model/revision when real-console observation is involved;
- license, commit and hashes for any imported redistributable asset.

If evidence is insufficient, leave the corresponding STATUS.md task unchecked.
