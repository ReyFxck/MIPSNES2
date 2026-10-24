# DMA register and address-plan foundation

The shared core now owns the eight `$43x0-$43xB` DMA/HDMA register banks.
`$43xF` aliases `$43xB`; `$43xC-$43xE` remain unhandled. Machine power-on
initializes the modeled channel bytes to `$FF`. CPU reads and writes in
system banks route through the normal SNES bus to this state; standalone
bus users can leave the state pointer null and continue using their
external DMA callback. No platform address or SDK type enters the core.

A pure transfer-plan helper returns the B-bus port offset for all eight
general-purpose DMA modes, advances or fixes the 16-bit A-bus offset under
`$43x0` bits 3/4, and interprets a zero 16-bit size as 65536 bytes. It
does not move bytes, trigger `$420B`, stall the CPU, update channel registers
after transfer, or implement HDMA. Those behaviors remain separate work.

Deterministic host tests cover eight isolated channels, all implemented
register offsets, `$43xB/$43xF` aliasing, invalid/unknown offsets,
power-on reset, mirrored system-bank routing, the eight mode sequences,
16-bit address wrap, fixed/decrement precedence and zero length. The first
two parallel host attempts encountered unrelated zero-byte generated test
executables; `make clean && make test` in serial passed (2026-10-24, C99
`cc`). Cross-build/CI outcomes are tracked in [STATUS.md](../STATUS.md).
No real-console DMA timing or runtime result is claimed.

Hardware references consulted on 2026-10-24:

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), DMA chapter and CPU register tables.
- [Super Famicom Development Wiki register descriptions](https://wiki.superfamicom.org/registers), `$43x0-$43xF`, including the unknown-register alias and power-on values.
- [Super Famicom Development Wiki DMA/HDMA overview](https://wiki.superfamicom.org/dma-and-hdma), B-bus modes, address direction and CPU suspension.

These sources describe hardware behavior; no emulator source was used.
