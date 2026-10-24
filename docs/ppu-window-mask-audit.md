# Mode 0 main-screen layer windows — 2026-10-24

Nintendo's *SNES Development Manual, Book I*, register pages 2-27-12 through
2-27-13 describe window selection, positions and boolean combination. The
[SNESdev PPU register guide](https://snes.nesdev.org/wiki/PPU_registers)
documents `$2123-$212B/$212E` bit allocation, while its
[window overview](https://snes.nesdev.org/wiki/Windows) describes hiding
layers and the four combinations. Independent
[Super Famicom window notes](https://wiki.superfamicom.org/windows) confirm
per-layer masking. No emulator source code was consulted.

`core/ppu/window_mask.c` owns only the main-screen BG1-4/OBJ subset. It
calculates a 256-entry bitmask with inclusive left/right edges, per-layer
enable and inversion, and OR/AND/XOR/XNOR when both windows are enabled. A
reversed edge pair is treated as an empty region before inversion. `$212E`
gates each layer mask; an unconfigured layer is never masked. The machine
routes register writes to this state and computes the mask at its provisional
HBlank line event. The Mode 0 mixer ignores masked BG/OBJ candidates before
priority selection, revealing lower layers or CGRAM backdrop index zero.

Synthetic host vectors cover all four boolean operations, edge inclusion,
single-window inversion, disabled layers, reversed edges, BG4/OBJ separate
settings, masked fall-through and a machine bus/event fixture. Host C99
`cc` `make test` passes (2026-10-24); PS2/PSP/host CI for this revision is
pending. The core module is included in both target build graphs.

This does not implement the color window, `$212F` sub-screen masking,
`$2130` clip modes, mid-scanline writes, hardware fetch phase or a video
backend. Window registers are sampled as a full-line snapshot at the
current provisional event. Neither target runtime nor real SNES hardware
behavior has been measured.
