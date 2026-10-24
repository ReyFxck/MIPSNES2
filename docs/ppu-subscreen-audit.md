# Mode 0 sub-screen foundation — 2026-10-24

Nintendo's *SNES Development Manual, Book I*, PPU register sections for
`$212D/$212F/$2132`, and the independent
[SNESdev PPU register guide](https://snes.nesdev.org/wiki/PPU_registers)
describe sub-screen layer/window enables and COLDATA's separate red, green
and blue writes. The [Super Famicom transparency notes](https://wiki.superfamicom.org/transparency)
describe the fixed color as the sub-screen backdrop. No emulator source
code or external fixture was used.

The machine stores `$212D` bits 0-4 independently from `$212C`, and `$212F`
gates the existing window predicates for the sub-screen independently from
`$212E`. `$2132` updates only the selected 5-bit channels of a BGR555 fixed
color. At the provisional line event the shared mixer chooses BG1-4/OBJ or
backdrop for each sub-screen pixel. A pure resolver reads unattenuated CGRAM
for selected layers and substitutes the fixed color for backdrop pixels.
This is an intermediate color-math input. The previously added main line is
already brightness-scaled, so a later color-math implementation must obtain
unattenuated main colors before combining them.

Deterministic host fixtures cover independent main/sub layer selection,
masked sub-screen fallback, fixed-color channel retention, CGRAM source
colors and invalid resolver arguments. Host C99 `cc` with
`make clean && make test` passes (2026-10-24). Revision `b258135` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037923),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037911)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037891).

Color math, color-window clipping, pseudo-hires output, mid-line register
changes and exact hardware fetch phases remain open. No PS2/PSP runtime or
real SNES hardware validation is claimed.
