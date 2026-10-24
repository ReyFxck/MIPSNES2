# Mode 0 color-window regions — 2026-10-24

Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
PPU pages 2-27-12 through 2-27-16, defines color-window selection, logic
and `$2130` region modes. The [SNESdev PPU register guide](https://snes.nesdev.org/wiki/PPU_registers)
lists the `never/outside/inside/always` encodings. The independent
[Super Famicom rendering notes](https://wiki.superfamicom.org/rendering-the-screen)
describe clipping the main pixel before arithmetic and omitting half math
when main color is clipped. No emulator source or external test asset was
used.

The shared window module evaluates `$2125`'s high nibble with the same two
inclusive edge ranges, inversion and `$212B` OR/AND/XOR/XNOR logic used for
layer windows. It emits a 256-pixel inside predicate. `$2130` bits 6-7
replace main color with black in a chosen region; bits 4-5 independently
suppress color math. An unconfigured color window has an empty inside
region. Main clipping occurs before add/subtract, and suppresses half math
while still allowing the selected addend. The machine supplies this
predicate to the shared color-math pass at its provisional line event.

Synthetic C99 host fixtures cover all four region encodings, inside and
outside pixels, disabled windows, XOR overlap, main clipping, math
suppression and both effects together. A machine fixture covers register
routing and the resulting palette/fixed-color pixel. `make test` passes
(2026-10-24); PS2/PSP/host CI for this revision is pending.

The full-line register snapshot does not reproduce mid-line writes or
color-window propagation in pseudo-hires. Exact fetch phase, analog DAC,
video output and target runtime remain unverified or absent.
