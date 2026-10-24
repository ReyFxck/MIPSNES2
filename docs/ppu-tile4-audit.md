# 4bpp tile decode — 2026-10-24

`core/ppu/tile4.c` reads one 8×8, 4bpp tile from caller-owned 64 KiB VRAM
and produces 64 row-major color indices. Nintendo's *SNES Development
Manual, Book I*, [appendix A-2](https://floating.muncher.se/bot/manual/book1_text.pdf),
identifies an OBJ character as 16 VRAM words, with two groups of eight row
words. The independent [SnesLab graphics-format table](https://sneslab.net/wiki/Graphics_Format)
specifies bitplanes 0/1 as row-interleaved bytes in the first 16 bytes and
bitplanes 2/3 similarly in the next 16 bytes. The implementation is original;
the sample source code on that page was not used or indexed.

Bit 7 of each row byte is the leftmost pixel. The decoder yields indices
0-15 without applying a palette or deciding transparency. It wraps tile
fetch addresses within 64 KiB VRAM. Its 32K-word address unit agrees with
the OAM tile-address helper. `tests/test_ppu_tile4.c` checks all-zero and
all-15 tiles, individual bitplanes at distinct columns, a mixed last pixel,
end-of-VRAM wrap, invalid inputs and an OBJ address-to-VRAM fixture.

A clean host C99 `cc` `make test` passes (2026-10-24) after rebuilding an
unrelated generated `test-interrupt-lines` executable that had become
zero-byte on the first attempt; its cause is unconfirmed. Implementation
revision `6b23966a5b32856f7f4e8575c0d06eec24fab2cb` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023302),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023320) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023326).
No target runtime, graphics
output, palette behavior, flipping or scanline sprite evaluation is claimed.
