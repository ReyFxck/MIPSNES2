# PPU Mode 1 reference scanline audit

Mode 1 uses 4bpp BG1/BG2 and 2bpp BG3. The pure 4bpp helper reads 32-byte
characters from caller-owned VRAM, four bitplanes, 8×8 or 16×16 map entries,
flips, scrolling, map quadrants and the CGRAM palette number. Zero color is
transparent. BG3 uses the existing 2bpp line with CGRAM base zero. `$2105`
bit 3 raises BG3 priority-1 pixels above OBJ priority 3; otherwise the
documented Mode 1 BG/OBJ order applies. The existing window, main/sub,
color-math and brightness passes consume the chosen CGRAM/source line.

The test data is synthetic and hand-composed. `make clean && make -j2 test`
checks distinct bitplanes, palette, transparency, a horizontal and combined
map-boundary transition, a flipped 16×16 tile, invalid input, register
configuration, BG/OBJ priority, masks and the scheduled machine bus path.
Target build results belong in [STATUS.md](../STATUS.md); no target execution
or image presentation is claimed.

This is a scanline reference subset. It does not model mid-line register
writes, mosaic, interlace, pseudo-hires, exact PPU pipeline phases, VRAM
access restrictions or a PS2/PSP display path. The machine's shared line
fields retain their older `mode0_*` names and now carry Mode 1 too; this is
a naming limitation, not a separate hardware mode-specific backend.

Hardware references:

- [SNES Development Manual](https://floating.muncher.se/bot/manual/book1_text.pdf), PPU background modes and tile formats.
- [Super Famicom Development Wiki priority chart](https://wiki.superfamicom.org/background-priority-chart), Mode 1 column and BG3 priority bit.
- [Fullsnes PPU control documentation](https://problemkaputt.de/fullsnes.htm#snesppucontrol), independently described register and priority behavior.

No source code from another emulator was used.
