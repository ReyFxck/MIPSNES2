# PS2 reference-frame capture audit

The PS2 ROM path now registers the shared machine's video-line callback.
`platform/ps2/video_frame.c` copies each 256-pixel nominal BGR555 line into
a caller-owned 256×256 RGBA32 texture. The current NTSC path accepts 224
visible lines, leaving the remaining texture rows black. It expands each
5-bit channel to 8 bits with bit replication and sets GS alpha to `0x80`.
The frame buffer is static EE memory, not stack storage.

The conversion is backend-only: the SNES core continues to retain BGR555 and
knows nothing about GS texture format. A duplicate line updates the texture
but does not double-count `delivered_rows`; out-of-range lines are ignored.
Host fixtures check red/green/blue channel order, intermediate levels,
black, last visible row, padding, duplicate delivery, 239-line capacity and
invalid initialization. `make clean && make -j2 test` runs the fixture.
Cross-build evidence is in [STATUS.md](../STATUS.md).

No texture upload or GS drawing is implemented in this block. The PS2 ROM
launcher still shows its debug text and reports the number of captured lines.
The bounded CPU run may finish before a full frame, so a complete captured
image is not guaranteed. Neither emulator runtime nor physical PS2 output
was tested for this revision.

The channel packing and transfer plan were checked against the
[PS2SDK draw/texture sample](https://github.com/ps2dev/ps2sdk/blob/master/ee/draw/samples/texture/texture.c)
and [PS2SDK GS graph sample](https://github.com/ps2dev/ps2sdk/blob/master/ee/graph/samples/graph.c)
on 2026-10-24. These are SDK examples, not emulator code; no sample
implementation was copied.
