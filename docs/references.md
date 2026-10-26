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

Source code from other emulators is not an accepted implementation source and must
not be added to a XERJ corpus. A separately published prose explanation from an
emulator project's developers may be recorded as secondary evidence when it clearly
states the behavior and implementation method. Cite its author, URL and revision;
write original code and resolve claims against manufacturer documentation,
independent tests or real-hardware observations before marking behavior verified.

## 65C816 and SNES CPU

- [WDC W65C816S datasheet (March 13, 2024)](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf) — primary register, instruction, addressing, flag, signal and processor-cycle reference. Sections 2.13, 7.4, 7.13 and 7.19 plus Tables 5-2, 5-3 and 5-7 entry 22a define the current ABORT foundation and its explicit mid-instruction limits. Record the revision date because WDC updates this PDF in place.
- [WDC documentation index](https://www.westerndesigncenter.com/wdc/documentation.php) — manufacturer index for the W65C02/W65C816 assembly programming manual and related datasheets.
- [SNESdev Wiki: CPU and hardware contents](https://snes.nesdev.org/wiki/SNESdev_Wiki) — navigation hub for Ricoh 5A22 differences, vectors, timing and development practice. Community-maintained; confirm edge cases with tests or stronger evidence.
- [SNESdev Wiki: timing](https://snes.nesdev.org/wiki/Timing) — 5A22 master-clock conversion and 6/8/12-master-clock access classes. This complements rather than replaces WDC processor-cycle tables.
- [SNESdev Wiki: version differences](https://snes.nesdev.org/wiki/Version_differences) — revision-specific S-CPU/PPU behavior that must be kept distinct from universal behavior.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf) — manufacturer reference. Chapter 15 and internal pages 2-28-1 through 2-28-2 specify unsigned CPU multiplication/division, operands, results, divide-by-zero and approximate/exact 8/16-machine-cycle completion. Link only; do not commit a manual copy without verified redistribution rights.
- [SNESdev Wiki: multiplication](https://snes.nesdev.org/wiki/Multiplication) and [division](https://snes.nesdev.org/wiki/Division) — community register/timing summaries and links to independent hardware tests. Busy-operation warnings remain research leads rather than verified MIPSNES2 behavior.

## SNES bus, cartridge and registers

- [SNESdev Wiki: memory map](https://snes.nesdev.org/wiki/Memory_map) — WRAM, MMIO, cartridge windows, mirrors and access-speed overview.
- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt) — pinned independent hardware-research map used for WRAM, system-bank route and 6/8/12-clock boundaries. Revision `eeb98f1e85b7db6574ca0804455819ec81117198`; review provenance again before importing any asset (no asset is committed).
- [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/) — hardware-board and address-decoder explanation used to cross-check LoROM/HiROM/ExHiROM wiring. It is not an emulator implementation; special boards still require board-specific evidence.
- [SNESdev Wiki: ROM header](https://snes.nesdev.org/wiki/ROM_header) — header locations and mapper-related metadata. Header heuristics are not a license for per-game hardcoding.
- [SNESdev Wiki: ROM file formats](https://snes.nesdev.org/wiki/ROM_file_formats) — copier-header and file-layout context for the future loader.
- [Super Famicom Development Wiki: non-power-of-two ROM size](https://wiki.superfamicom.org/non-power-of-two-rom-size) — community description of larger-chip-first, smaller-chip mirroring (`ABCC`, `ABCDEEEE`) used only for the explicitly bounded irregular base-map policy; board-specific exceptions still require stronger evidence.
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm) — independently written, broad SNES register/timing/mapping reference. It is a hardware document, not an emulator implementation; verify disputed or revision-sensitive details on hardware.
- [Super Famicom Development Wiki: registers](https://wiki.superfamicom.org/registers) — searchable community register reference derived from hardware research documents. Check attribution and primary evidence for edge cases.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf) — chapter 13 and internal pages 2-28-7 through 2-28-9 define automatic standard-controller reading, the busy flag and `$4218-$421F` result layout; the controller section also directs manual `$4016/$4017` operation through the NES-compatible serial protocol. The manual gives an approximate 18 microsecond delay and roughly 215 microsecond automatic-read duration; link only, with no manual asset committed.
- [fullsnes: controller ports](https://problemkaputt.de/fullsnes.htm#snescontrollersioports) — independent hardware research for manual `$4016/$4017` strobe, paired serial lines, standard-pad bit order and post-bit-16 identification, plus the 4224-master-clock automatic-read duration and observed variable start-phase window. The fixed MIPSNES2 automatic start phase and independent manual/automatic state remain explicit approximations pending targeted hardware tests.
- [SNESdev Wiki: MMIO registers](https://snes.nesdev.org/wiki/MMIO_registers) — independent register-level description of `$4201` WRIO, `$4213` RDIO, open-collector pin combination and the controller-port-2 external-latch connection. PPU counter capture remains a separate future subsystem.
- [SNES Development Manual index](https://snes.nesdev.org/wiki/SNES_Development_Manual) — description/index for Nintendo's two-book licensed-developer manual. Book I pages 1-2-14 through 1-2-17 define the 16-byte registration prefix, Mode 25's `$40FFB0-$40FFDF` registration/header location and `25h/35h` map values. Do not add a manual copy to this repository without verified redistribution rights.
- [SnesLab: ExLoROM](https://sneslab.net/wiki/ExLoROM) — community evidence that ExLoROM is a rare unofficial mapping rather than a Nintendo-defined peer of ExHiROM. MIPSNES2 keeps it research-only pending a demonstrated compatibility need and stronger hardware evidence.

## PPU, DMA and timing

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf) — chapter 11 (2-11-1), programming caution 24.1 and PPU register pages 2-27-20/22/23 define software H/V capture, nine-bit readout, selector reset and field/latch/region/version status. Inspected on 2026-10-20; no manual asset is committed.
- The same manual's PPU register pages 2-27-11 and 2-27-21 document CGRAM address, paired read/write byte order and increment. [Super Famicom hardware register notes](https://wiki.superfamicom.org/registers) independently describe the PPU2 open-bus bit on the high read. Consulted on 2026-10-24; no emulator source used.
- The same manual's PPU pages 2-27-2 and 2-27-21 document OAM address/write/read and VBlank constraints. [Independent sprite hardware notes](https://wiki.superfamicom.org/sprites) describe the low-table latch and high-table mirror; consulted on 2026-10-24 without emulator source.
- The same manual's PPU page 2-27-1 and appendix A-3 specify `$2101` OBJSEL's six size pairs and the 128 sprite records across the low and high OAM tables. [Independent sprite hardware notes](https://wiki.superfamicom.org/sprites) corroborate the bit layout. Consulted on 2026-10-24; no emulator source was used.
- The same manual's PPU page 2-27-1 and appendix A-1 specify OBJ name-base and second-name region selection in VRAM. [Independent sprite hardware notes](https://wiki.superfamicom.org/sprites) describe 16×16 character-grid row/column wrap and first-tile word addresses. Consulted on 2026-10-24; no emulator source was used.
- The same manual's appendix A-2 describes OBJ 4-bit, 8×8 characters occupying 16 VRAM words, with row data in two eight-word halves. [SnesLab's declarative graphics-format table](https://sneslab.net/wiki/Graphics_Format) specifies the four bitplane byte positions; its sample source code was not used. Consulted on 2026-10-24.
- The same manual's chapter 20, page 2-20-1 specifies 33rd-OBJ range over and 35th-8-dot-tile time over, with 32/34 retained limits. [Independent sprite hardware notes](https://wiki.superfamicom.org/sprites) describe reverse tile-fetch order, X=`$100` counting and horizontal clipping. Consulted on 2026-10-24; no emulator source was used.
- The same [sprite hardware notes](https://wiki.superfamicom.org/sprites), priority-rotation section, describe `$2103` bit 7 and the current internal OAM word address as the first-sprite source, including high-table wrap and reload effects. Consulted on 2026-10-24; no emulator source was used.
- [Super Famicom Development Wiki, Backgrounds](https://wiki.superfamicom.org/backgrounds), BGnSC and tilemap sections, documents 32x32 screen arrangements, 8x8/16x16 character layout, map entry palette/priority/flip bits and tilemap base. Cross-checked against Nintendo's *SNES Development Manual, Book I*, chapter 3 and appendix A. Consulted on 2026-10-24 as hardware descriptions; no emulator source was used.
- [Super Famicom Development Wiki, Registers](https://wiki.superfamicom.org/registers), `$2105/$2107-$2114` sections, describes BG mode, map/character bases and a shared previous-byte scroll-write formula. The scroll formula is explicitly presented as a theory and remains hardware-unverified here. Consulted on 2026-10-24; no emulator source was used.
- [Super Famicom Development Wiki, Background Priority Chart](https://wiki.superfamicom.org/background-priority-chart), Mode 0 column, gives the strict BG1-4/OBJ priority order used by the reference CGRAM-index mixer. Consulted on 2026-10-24 as independent hardware documentation; no emulator source was used.
- [Super Famicom Development Wiki, DMA/HDMA](https://wiki.superfamicom.org/dma-and-hdma) and [register descriptions](https://wiki.superfamicom.org/registers) specify eight channels, `$43x0-$43xB/$43xF`, transfer-mode port patterns, A-address control and zero-size length. Cross-checked with Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), DMA chapter and register tables. Consulted on 2026-10-24 as hardware descriptions; no emulator source was used. Electrical transfer phases remain open.
- [Anomie's SNES Timing Doc, HDMA section](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt) gives V=0/H≈6 initialization, visible-line dot-278 start, VBlank-start channel deactivation, DMA priority and approximate 18/8/24-clock costs. Consulted on 2026-10-25 as independent hardware research; exact pauses and bus subphases remain unverified. No emulator source was used.
- [Anomie's SNES register documentation, SETINI `$2133`](https://github.com/gilligan/snesdev/blob/master/docs/snes_registers.txt) describes bit 2 overscan, its end-of-frame sensitivity and complex effects for changes near normal VBlank. Cross-checked with Nintendo's *SNES Development Manual, Book I* register table on 2026-10-25. The current core implements a bounded frame-level cutoff and leaves the reported late-line effects unclaimed; no emulator source was used.
- The [Super Famicom Development Wiki HDMA algorithm](https://wiki.superfamicom.org/dma-and-hdma) describes a shortened indirect-pointer load after a zero line descriptor on the last active channel of a scanline: the descriptor supplies the zero low byte and only the high byte is fetched. Consulted on 2026-10-25 as independent hardware research. Frame-initialization and chip-revision variants are not established by this source; no emulator source was used.
- The same [priority chart](https://wiki.superfamicom.org/background-priority-chart), Mode 1 column, gives BG1/BG2/BG3/OBJ ordering and the `$2105` BG3-high override. Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), BG mode and character-format sections, defines Mode 1's two 4bpp backgrounds and one 2bpp background. Consulted on 2026-10-24; no emulator source was used.
- [SNESdev Wiki, Palettes](https://snes.nesdev.org/wiki/Palettes), CGRAM access and assignment sections, documents 256 BGR555 words, low-byte-first layout and entry zero as backdrop. [SNESdev Wiki, PPU registers](https://snes.nesdev.org/wiki/PPU_registers), INIDISP `$2100` section, describes 0-15 brightness steps and forced blank. Consulted on 2026-10-24 as community hardware documentation. The core's integer brightness conversion is a nominal digital approximation, not a measured DAC transfer curve; no emulator source was used.
- Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), PPU register pages 2-27-12 through 2-27-13, covers window selection, edges and mask logic. [SNESdev Wiki, PPU registers](https://snes.nesdev.org/wiki/PPU_registers), Windows section, gives `$2123-$212B/$212E` bit positions, and its [window overview](https://snes.nesdev.org/wiki/Windows) describes per-layer masking. Cross-checked with [independent Super Famicom window notes](https://wiki.superfamicom.org/windows). Consulted on 2026-10-24; no emulator source or test asset was imported. The current full-line snapshot needs hardware timing checks.
- The same Nintendo manual's PPU register sections for `$212D/$212F/$2132` and [SNESdev's PPU register guide](https://snes.nesdev.org/wiki/PPU_registers) describe sub-screen layer/window enables and fixed-color channel writes. [Independent Super Famicom transparency notes](https://wiki.superfamicom.org/transparency) identify fixed color as the sub-screen backdrop. Consulted on 2026-10-24; no emulator source or test asset was imported. Color math and exact sub-screen timing remain open.
- The same [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), PPU register page 2-27-16, specifies `$2130/$2131` color math source/operation bits, OBJ palette 4-7 eligibility and the sub-backdrop half-math exception. [SNESdev Wiki: Color math](https://snes.nesdev.org/wiki/Color_math) independently describes BGR channel addition/subtraction and fixed/sub-screen sources. Consulted on 2026-10-24; no emulator source or test asset was imported. Color windows, mid-line timing and analog output remain unverified.
- The same Nintendo manual's PPU pages 2-27-12 through 2-27-16 specify color-window registers and region selection. [SNESdev Wiki: PPU registers](https://snes.nesdev.org/wiki/PPU_registers) lists `$2130` never/outside/inside/always modes; [Super Famicom rendering notes](https://wiki.superfamicom.org/rendering-the-screen) explain pre-math main clipping and the half-math exception. Consulted on 2026-10-24; no emulator source or test asset was imported. Full-line snapshots still need hardware phase validation.
- [Anomie's SNES register documentation, STAT77 `$213E`](https://github.com/gilligan/snesdev/blob/master/docs/snes_registers.txt) records bit 7 time over, bit 6 range over, end-of-VBlank clearing and setting even when OBJ output is disabled. Consulted on 2026-10-24 as an independent hardware description, not emulator source. It does not establish the intra-line bit assertion phase or forced-blank selection behavior.
- [Independent SNES timing notes](https://wiki.superfamicom.org/timing) place the OAM VBlank reload near horizontal dot 10 and describe forced-blank gating. The precise edge still needs real-hardware validation; consulted on 2026-10-24 without emulator source.
- [Fullsnes OAM hardware description](https://problemkaputt.de/fullsnes.htm#snesmemoryoamaccessspriteattributes) limits the forced-blank release reload to the first VBlank scanline; [Anomie's timing notes](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt) relay a broader report. Consulted on 2026-10-24 as hardware descriptions, not implementation source; the disagreement remains open.
- [fullsnes: PPU timers and status](https://problemkaputt.de/fullsnes.htm#snespputimersandstatus) — firsthand hardware-research register and latch observations; the timing sections distinguish normal long dots 323/327 from the NTSC short line. Its unconditional status-clear summary differs from Anomie's WRIO-gated description; MIPSNES2 records the conflict in the counter audit rather than claiming hardware resolution.
- [SNESdev Wiki: SNES PPU for NES developers](https://snes.nesdev.org/wiki/SNES_PPU_for_NES_developers) — useful conceptual PPU orientation, not a complete behavior contract.
- [SNESdev Wiki: timing](https://snes.nesdev.org/wiki/Timing) — scanline/frame clocks and CPU access timing.
- [fullsnes hardware specification: SNES Timings](https://problemkaputt.de/fullsnes.htm#snestimings) — independent hardware-research sections for H/V counters, regional frame geometry, long/short scanlines and event positions. The non-interlaced H/V foundation was audited against these sections on 2026-10-19; disputed and revision-specific behavior still requires targeted hardware tests.
- [Super Famicom Development Wiki: Timing](https://wiki.superfamicom.org/timing) — community hardware-research summary for H/V timer modes, master-clock match formulas and documented short/final-line exceptions. The H/V IRQ foundation was cross-checked against it on 2026-10-19; exact CPU sampling still requires stronger bus-cycle and hardware evidence.
- [SNESdev Wiki: version differences](https://snes.nesdev.org/wiki/Version_differences) — PPU and DMA/HDMA revision differences.
- [Anomie's document index](https://sneslab.net/wiki/List_of_Anomie%27s_Docs) — independent hardware research covering registers, timing, memory mapping, open bus/wrapping, SPC700 and S-DSP. Preserve document title/revision when citing a specific file.

## SPC700, S-DSP and audio

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), PPU register page 2-27-24, documents `$2140-$2143` as four communication addresses backed by different input and output registers, eight total. Consulted on 2026-10-25; no manual asset is committed.
- [SNESdev Wiki: S-SMP](https://snes.nesdev.org/wiki/S-SMP) — audio CPU, RAM, IPL and timer overview.
- [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual), memory map, direct-page, peripheral-register and SPC700 instruction-summary sections — manufacturer sound-hardware description of `$F0-$FF`, port directions, `$F1` port clear, timers, PSW bit order, P-selected pages and the opcode/length/cycle/flag behavior of the implemented `NOP`, PSW-control and register/immediate/direct-page MOV instructions, including indexed direct-page forms. Consulted on 2026-10-26; linked only, with no manual or firmware bytes committed.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), appendix C SPC700 instruction tables — first-party source for the `$F4/$F9/$FB` indexed direct-page loads, `$D4/$D9/$DB` stores, `$E5/$E9/$EC` absolute loads, `$C5/$C9/$CC` absolute stores and `$F5/$F6/$D5/$D6` absolute-indexed A forms, including operand lengths, 4/5/6-cycle costs and load-only N/Z effects. Consulted on 2026-10-26; no manual asset is committed.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), appendix C tables C-5/C-6 — first-party source for `$C6/$E6/$AF/$BF` register-indirect, `$C7/$D7/$E7/$F7` direct-indirect, `$8F` immediate-to-direct and `$FA` direct-to-direct MOV forms, including stream lengths, 3/4/5/6/7-cycle costs and load-only N/Z effects. Consulted on 2026-10-26; no manual asset is committed.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), appendix C tables C-7 through C-10 — first-party source for the 100-opcode byte ALU block: accumulator and direct-memory OR/AND/EOR/CMP/ADC/SBC, ASL/ROL/LSR/ROR, and byte INC/DEC forms with opcode, length, 2-6-cycle cost and N/Z/C/H/V effects. Consulted on 2026-10-26; no manual asset is committed.
- [Anomie's SPC700 Doc](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/spc700.txt), timer and CONTROL-register sections — pinned independent hardware research for the free-running 128/16-cycle first stage, 0-to-1 stage-2/stage-3 clear, zero target meaning 256, running-target wrap behavior and four-bit read-clear output. Revision `eeb98f1e85b7db6574ca0804455819ec81117198`; consulted 2026-10-25. No source code from another emulator was used.
- [SNESdev Wiki: S-SMP register table](https://snes.nesdev.org/wiki/APU_register_table/SMP) — SPC700-side MMIO register map, including undocumented fields that require independent confirmation.
- [SNESdev Wiki: S-DSP registers](https://snes.nesdev.org/wiki/S-DSP_registers) — register-oriented S-DSP starting point.
- [Super Famicom Development Wiki: SPC700 reference](https://wiki.superfamicom.org/spc700-reference) — instruction/register reference cross-check.
- [Anomie's document index](https://sneslab.net/wiki/List_of_Anomie%27s_Docs) — includes dedicated SPC700 cycle, boot ROM and S-DSP research documents.
- [RFAuds2](https://github.com/ReyFxck/RFAuds2) — preferred experimental future PS2 PCM transport only. Pin a known revision before integration; never use it as an S-DSP timing or behavior source and never copy it into the shared core.

## Independent SNES tests

These are tests and vectors, not emulator implementations. Before committing any
asset, re-check its license at the exact pinned revision and add a provenance record.

- [SingleStepTests/65816](https://github.com/SingleStepTests/65816) — language-neutral JSON instruction transitions with bus activity. The decimal milestone audited revision `307c63f5673c8c6b861c52f71ddb89212a227480`; a corpus vector is evidence to investigate, not automatic proof over real hardware.
- [gilyon/snes-tests](https://github.com/gilyon/snes-tests/tree/5ecdf555da920f0bd7b157542141965a8120186d) — MIT-licensed source and ROM tests for 65816 and SPC700 behavior, reviewed at revision `5ecdf555da920f0bd7b157542141965a8120186d` on 2026-10-23. The CPU suite excludes WAI/STP, interrupt, MMIO and cycle tests; the SPC suite excludes S-SMP MMIO, dummy reads, DSP and timing. No upstream ROM or source was added to MIPSNES2. Prefer building and running the ROMs once a complete SNES machine/ROM runner exists.
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
