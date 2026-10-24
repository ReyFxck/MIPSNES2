# CGRAM CPU ports — 2026-10-24

The shared core models 256 two-byte colors in 512 caller-owned bytes. `$2121`
selects an eight-bit color address and resets the byte selector. `$2122` latches
the low byte, then commits the low byte and seven high bits on the next write;
the address increments after high and wraps from 255 to zero. `$213B` reads
low then high, also incrementing after high; the high read's bit 7 comes from
the existing PPU2 open-bus latch. Machine power-on resets port selectors and
address while retaining palette bytes. The PS2 ROM path supplies the buffer;
the PSP target compiles the shared module but does not yet run a ROM.

Sources for behavior, not implementation code: Nintendo's [SNES Development
Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), PPU
register pages 2-27-11 (`$2121/$2122`) and 2-27-21 (`$213B`); independently
published [Super Famicom hardware register notes](https://wiki.superfamicom.org/registers),
`$2121/$2122/$213B` descriptions and PPU2 open-bus note. No emulator source
was used or indexed.

`tests/test_ppu_cgram.c` covers paired writes, high-bit masking, read byte
order, open-bus high bit, address wrap, selector reset and memory retention.
`tests/test_machine.c` covers capacity rejection and CPU bus routing, including
power-on retention. Host `make test` passes with C99 `cc` on 2026-10-24.
Implementation revision `37b9ab68c6102a5accdf6693e0a2c4be13b45a8e`
passed [PS2 cross-build CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572567),
[PSP cross-build CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572654)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572626).
No runtime check on either real console is claimed.

The read/write byte selector is modeled as shared. Exact behavior when reads
and writes are interleaved has not been verified on hardware. Writes during
active display, read access restrictions, PPU fetch contention, initial
electrical contents, DMA/HDMA and rendering are unresolved. This port subset
does not complete CGRAM or the PPU; keep their parent tasks in progress.
