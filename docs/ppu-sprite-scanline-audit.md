# OBJ scanline selection — 2026-10-24

Nintendo's *SNES Development Manual, Book I*, [chapter 20, page
2-20-1](https://floating.muncher.se/bot/manual/book1_text.pdf), states the
33rd-sprite range and 35th-8-dot-tile time-over limits: at most 32 sprites
and 34 tiles contribute on one line. [Independent sprite hardware
notes](https://wiki.superfamicom.org/sprites) describe first-sprite order,
reverse tile-fetch order, left-to-right tile traversal, X clipping and the
X=`$100` exception. No other emulator's source code was used or indexed.

`core/ppu/sprite_scanline.c` makes a bounded, read-only selection from
caller-owned OAM. Its input line is already in sprite Y coordinates; the
future timed PPU integration must translate the machine H/V line. An explicit
first-sprite index avoids guessing priority-rotation state. The result
contains up to 32 selected sprite indices and 34 tile slivers with source
tile coordinates after flip and true screen X. It reports overflow when
further eligible sprites or slivers exist. Size modes 6-7 are rejected
because the current OBJ size lookup does not document them.

`tests/test_ppu_sprite_scanline.c` uses synthetic OAM bytes and checks
32/33 sprites, 34/35 slivers, circular first-index order, reverse fetch
order, X=`$100`, negative X boundaries, horizontal/vertical flips, Y wrap
and invalid inputs. C99 `cc` `make test` passes on host (2026-10-24);
PS2/PSP cross-build CI is pending.

This selection is not a timed PPU evaluation. `$213E` sticky flags,
line-zero blanking, active-display OAM invalidation, priority rotation's
line-dependent edge cases, fetch timing, interlace and real-console
behavior remain unverified. No renderer or frame output exists.
