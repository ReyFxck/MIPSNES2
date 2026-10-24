# Mode 0 CGRAM-index mixer — 2026-10-24

The independent [Super Famicom background priority chart](https://wiki.superfamicom.org/background-priority-chart)
lists Mode 0 layers from OBJ priority 3 at the top through BG1-4 priority
bands and the backdrop at the bottom. Nintendo's *SNES Development Manual,
Book I*, BG and OBJ sections describe layer priorities and transparent
character color zero. No source code from another emulator was consulted.

`core/ppu/mode0_mix.c` is a pure 256-pixel selector. It receives four 2bpp
BG lines and one OBJ line and applies `$212C` main-screen bits 0-4. Every
transparent pixel falls through to a lower ranked layer; an uncovered pixel
selects CGRAM index zero. It reports the chosen index and source layer,
without reading CGRAM or producing RGB.

The machine stores `$212C` bits 0-4, computes enabled Mode 0 BG1-4 lines
and combines them with its existing OBJ line at the provisional HBlank event.
OBJ overflow evaluation remains independent of the OBJ display-enable bit.
Synthetic host vectors cover BG priority ties across layers, OBJ bands,
transparency, disabled layers, backdrop, invalid pointers and the machine
BG/OBJ handoff. C99 `cc` `make test` passes (2026-10-24). Cross-build CI for
this revision is pending.

This line is not a screen image. CGRAM lookup, brightness, windows, sub
screen, color math, forced-blank color, hires, interlace, mid-line changes
and target hardware runtime remain unimplemented or unverified. The line
event phase and reference performance require target measurement.
