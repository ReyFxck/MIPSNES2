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
the SNES bus. Host C99 `make test` and repository checks passed; target CI
results must be recorded only after the runs finish.

The PPU's automatic OAM address reload at the beginning of VBlank, forced
blank transition, active-display address invalidation and access restrictions
are not implemented. Sprite decoding, priority rotation effects, DMA/HDMA,
rendering and real-console behavior are also unverified. The OAM and PPU
parent tasks remain incomplete.
