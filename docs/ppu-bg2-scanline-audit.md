# Pure 2bpp BG scanline — 2026-10-24

Nintendo's *SNES Development Manual, Book I*, chapter 3 and appendix A,
describes tilemap entries, palettes and 2bpp character data. The independent
[Super Famicom background notes](https://wiki.superfamicom.org/backgrounds)
describe BGnSC's four 32x32-screen layouts and 8x8/16x16 tile addressing.
No emulator source code or ROM data was used.

`core/ppu/bg2_scanline.c` is a pure 256-pixel Mode 0-style 2bpp reference
path. The caller supplies VRAM, map/character word bases, scroll values,
screen size, tile size and one of the four Mode 0 palette ranges. It wraps
map and tile word addresses in 32K-word VRAM, resolves 32x32 screen quadrants,
applies 16x16 subtiles and H/V flips, and emits palette index, priority and
opacity for every pixel. Color zero is transparent to later mixing.

`tests/test_ppu_bg2_scanline.c` uses synthetic map and planar bytes to cover
palette/priority, zero transparency, horizontal/vertical map boundaries,
the 64x64 bottom-right quadrant, 16x16 double flip and invalid inputs.
Host C99 `cc` `make test` passes (2026-10-24). Cross-build CI for this
revision is pending.

The machine now connects BG1 Mode 0 control, tilemap, tile-base and scroll
registers to this helper at a provisional HBlank event; see the
[register audit](ppu-bg-register-audit.md). It does not model mid-scanline changes, windows, BG/OBJ ordering,
color math, hires/interlace or screen presentation. No target runtime or
real-hardware verification is claimed. The per-pixel lookup is a reference
path and needs profiling before it becomes a PS2/PSP hot path.
