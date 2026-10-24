# OAM CPU ports — 2026-10-24

`core/ppu/oam_ports.c` models the CPU-facing `$2102/$2103/$2104/$2138`
subset over 544 caller-owned bytes: 512 in the low table and 32 in the high
table. Either address-register write reloads a ten-bit internal byte pointer
from the saved nine-bit word address. The low table stores a two-byte word
only after its odd-byte write; an even-byte write alone changes just the
write latch. High-table writes commit each byte directly. Reads advance the
same internal byte pointer. The high table mirrors across the selected half
of the 1024-byte logical range; the pointer wraps after byte 1023.
`$2103` bit 7 is retained as priority-rotation state, but no sprite priority
evaluation exists yet.

Behavior references: Nintendo's [SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
PPU register pages 2-27-2 (`$2102-$2104`) and 2-27-21 (`$2138`). Independent
[sprite hardware notes](https://wiki.superfamicom.org/sprites) describe the
544-byte physical layout, byte-address wrapping, high-table mirror and
low-table write latch, including mixed reads/writes. No source code from
other emulators was used or indexed.

`tests/test_ppu_oam.c` covers pair commitment, both table boundaries,
high-table mirror, pointer wrap, register reload, mixed read/write latch,
priority-rotation bit storage and power-on byte retention.
`tests/test_machine.c` checks buffer-capacity rejection and routing through
the SNES bus. Host C99 `make test` and repository checks passed. Implementation
revision `28ed5c1f9d106c84e9f421f8cefc16f88e6d8f37` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076823004),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076823013) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076822995).
No runtime result on either target is claimed.

An OAM-only machine scheduler callback now reloads the internal pointer at
horizontal dot 10 (40 master clocks) of the VBlank start line, provided
`$2100` bit 7 indicates display is enabled. The callback schedules the next
frame, accounting for NTSC's alternating four-clock short line and PAL's
normal frame length. The machine's power-on state starts forced blank as a
deterministic policy. [Independent timing research](https://wiki.superfamicom.org/timing)
places this reload at dot 10; Nintendo's manual confirms automatic reload
near VBlank but does not specify this exact clock. The timing remains a
hardware-validation target.

`tests/test_machine.c` also checks refusal when the event budget is zero,
reload precisely at the event boundary, forced-blank suppression and the PAL
overscan event position. Direct host `make test` passes for this extension;
implementation revision `af3ff1e9f947f586638a5cb11e1ddc3df47b8be8`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224129),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224195) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224329).

The machine now additionally reloads on a `$2100` bit-7 1-to-0 transition
during the first VBlank scanline. `$2100` writes still reach any external PPU
callback, which may later own brightness. [Fullsnes's independent hardware
description](https://problemkaputt.de/fullsnes.htm#snesmemoryoamaccessspriteattributes)
describes this first-line condition. [Anomie's timing notes](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt)
relay a broader report of any 1-to-0 transition. The documentation does not
resolve that difference. The implementation chooses the narrower behavior;
outside-first-line behavior needs a targeted hardware fixture. The exact
effect of writes around horizontal dot 10 also needs verification.

`tests/test_machine.c` exercises release on the first VBlank line through a
mirrored bus bank, repeated enabled-display writes, the reverse transition,
release on the next line and the next scheduled VBlank reload. A clean
`make test` run passes on host C99 `cc` (2026-10-24); PS2/PSP CI is pending.

Active-display address invalidation and access restrictions are not
implemented. Sprite decoding, priority rotation
effects, DMA/HDMA,
rendering and real-console behavior are also unverified. The OAM and PPU
parent tasks remain incomplete.
