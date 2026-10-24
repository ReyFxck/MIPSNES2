# Mode 0 BG register subset — 2026-10-24

Nintendo's *SNES Development Manual, Book I*, BG/register appendices specify
the Mode 0 BG map and character bases. The independent
[Super Famicom register notes](https://wiki.superfamicom.org/registers)
describe `$2105`, `$2107-$210C` and `$210D-$2114`. The notes explicitly
present the shared BG scroll-write latch formula as a current theory.
No emulator source code was consulted or copied.

`core/ppu/bg_registers.c` retains Mode 0 mode/size, four BG screen bases,
four character bases and shared horizontal/vertical scroll latches. Its
Mode 0 decoder supplies explicit configurations to the pure 2bpp line helper.
The machine routes those writes through the shared PPU callback and prepares
BG1 at the provisional HBlank event when caller-owned VRAM exists. This also
works when OAM storage is absent. Power-on resets the register subset and
line while retaining VRAM bytes.

Synthetic host tests cover mode/base/size conversion, palette assignment,
high VRAM-word wrap, shared scroll latches, non-Mode-0 rejection and BG1
selection at the exact scheduler event boundary. C99 `cc` `make test` passes
(2026-10-24). Cross-build CI for this revision is pending.

BG2-4 configurations are decoded but only BG1 pixels are scheduled. `$2106`
mosaic, `$212C` layer enable, window masks, BG priority mixing, Mode 1+,
mid-line writes, interlace and target runtime remain open. The event phase
and shared scroll-latch behavior need real-console validation. The reference
per-pixel path needs target profiling before performance claims.
