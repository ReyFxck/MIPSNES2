# Mode 0 basic color math — 2026-10-24

Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
PPU register page 2-27-16 (`$2130/$2131`), specifies main-layer math
enables, OBJ palette 4-7 eligibility, addition/subtraction, halving and the
exception for sub-screen backdrop. The independent
[SNESdev color-math guide](https://snes.nesdev.org/wiki/Color_math)
describes the same channel arithmetic and `$2132` fixed-color source. The
MIPSNES2 implementation is original; no emulator source was consulted.

`core/ppu/color_math.c` takes bounded 256-pixel unattenuated BGR555 main
and sub lines. `$2130` bit 1 selects sub-screen or fixed color. `$2131`
selects main layers, add/subtract and half math. It applies each operation
per 5-bit channel, halves before saturation, and clamps to 0-31. A
sub-screen backdrop uses the fixed color and suppresses half math. OBJ
color math requires CGRAM index 192-255. `$2100` brightness scales the
result only after math, using the existing nominal digital model.

The machine retains `$2130/$2131` values and computes a separate raw main
line before color math at its provisional HBlank event. Synthetic C99 host
fixtures cover add/subtract, half rounding, saturation, per-channel
packing, layer and OBJ palette gating, fixed and sub sources, the backdrop
exception, brightness order, forced blank and invalid arguments. A machine
fixture checks register routing and combined BG/fixed-color output. Host
`make clean && make test` passes (2026-10-24); PS2/PSP/host CI is pending.

`$2130` color-window region bits and direct-color bit are retained but not
applied here. Color-window clipping can alter the half-math decision and
remains the next block. Exact mid-line behavior, analog DAC response,
target runtime and video output remain unverified or unimplemented.
