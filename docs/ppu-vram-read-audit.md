# Buffered VRAM read ports — 2026-10-24

`$2139/$213A` now return low/high bytes of a word buffer. Only the port chosen
by VMAIN bit 7 refreshes that buffer from the current remapped VRAM word and
then increments VMADD by the selected 1/32/128-word step. Writing VMADD low
or high does not refresh the buffer: the first incrementing read returns old
data and fetches the newly selected word for later reads. This is shared core
state routed through `core/machine` to the PS2 path; no platform addresses or
emulator source are imported into the implementation.

Nintendo's *SNES Development Manual*, Book I, PPU register pages 2-27-6 and
2-27-21 plus programming caution 24.5, documents increment selection and the
first dummy read: [manual scan](https://floating.muncher.se/bot/manual/book1_text.pdf).
The original hardware-research register notes reproduced at
[Super Famicom Development Wiki](https://wiki.superfamicom.org/registers)
describe the word-sized buffer and fetch immediately before increment. These
are behavior references, not emulator implementation code.

`tests/test_ppu_vram.c` checks both increment timings, low/high lanes, stale
buffer after VMADD changes, remapped read, physical/logical wrap and ignored
registers. `tests/test_machine.c` checks routing of buffered reads through the
machine bus after power-on. `make test` passes on the host; console builds and
CI for implementation revision `de252cc632e63b4a28d9b9a63c35f91d2f73e3c3`
completed successfully: [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052089722),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052089632) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052089693).
No PS2/PSP runtime result is claimed.

The power-on buffer starts at zero as an explicit deterministic test policy;
real initial electrical contents are unverified. CPU reads outside the allowed
display windows and interactions between `$2118/$2119` writes and pending
`$2139/$213A` reads remain unresolved. PPU fetch contention, DMA and rendering
are not implemented. These omissions remain unchecked in STATUS.md.
