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
BG/OBJ handoff. C99 `cc` `make test` passes (2026-10-24). Revision `91170f3`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722188),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722237) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722289).

The color pass uses the 256 15-bit entries described by the
[SNESdev palette reference](https://snes.nesdev.org/wiki/Palettes), including
entry zero for backdrop. [SNESdev's `$2100` register description](https://snes.nesdev.org/wiki/PPU_registers)
defines brightness steps 0-15 and forced blank. The pass converts each
BGR555 component to `floor(component * brightness / 15)`. This is an
explicitly nominal digital representation; no analog DAC transfer curve or
brightness-transition latency is claimed. The machine retains `$2100` low
bits and computes the color line at the provisional HBlank event when both
VRAM and CGRAM are present. Forced blank clears the stored line at that
event. Tests cover red/green/white, high-bit masking, CGRAM index 255,
backdrop, brightness 0/8/15, forced blank, invalid input and machine BG/OBJ
palette routing. C99 `cc` `make test` passes (2026-10-24). Revision `ac3124c`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159833),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159758) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159561).
No emulator source code was consulted.

This line is not a screen image. Main-screen layer windows now run before
selection; see the [window audit](ppu-window-mask-audit.md). Sub screen, color math, hires,
interlace, mid-line changes and target hardware runtime remain unimplemented
or unverified. The line-event phase and reference performance require target
measurement.
