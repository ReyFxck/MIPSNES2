# PS2 GS one-frame presentation audit

After a bounded `--rom PATH` run captures at least one reference line, the
PS2 backend initializes a 256×256 RGBA32 GS framebuffer and submits the
caller-owned, 64-byte-aligned EE buffer with a GIF DMA chain. The 224 visible
SNES rows occupy the top of the buffer; remaining rows are black. The path
uses no SNES behavior in `platform/ps2/gs_present.c`, and the core knows
nothing about GS addresses, DMA or pixel formats.
The backend flushes the EE data cache before sending the chain because
PS2SDK's chain helper flushes its packet but not separately referenced
texture data.

The design follows the public PS2SDK
[graph example](https://github.com/ps2dev/ps2sdk/blob/master/ee/graph/samples/graph.c),
[texture transfer example](https://github.com/ps2dev/ps2sdk/blob/master/ee/draw/samples/texture/texture.c),
and [draw sample link list](https://github.com/ps2dev/ps2sdk/blob/master/ee/draw/samples/teapot/Makefile.sample)
plus [DMA source-chain cache notes](https://github.com/ps2dev/ps2sdk/blob/master/ee/dma/src/dma.c)
consulted on 2026-10-24. This is original backend code using SDK APIs, not
code from another emulator.

The host tests validate the BGR555-to-RGBA32 input buffer and the machine
handoff, not the GS. The PS2 cross-build tests API/link compatibility only.
Revision `4ff9020` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898305),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898337)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898350).
No emulator or physical-console execution of this GS path is claimed. Its
256×256 single buffer has no scaling, double buffering or continuous frame
loop. It replaces the debug text only after at least one line has been
captured; before that, the existing text remains. A blank image can result
from a ROM that does not configure the currently supported PPU subset or
from the bounded CPU run ending early. PAL and overscan display paths remain
unimplemented.
