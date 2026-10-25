# Timed HDMA scheduler integration audit

Date: 2026-10-25

## Scope

The platform-neutral machine now requests HDMA initialization near V=0/H=6
(24 master clocks) and one transfer near dot 278 (1112 clocks) on each visible
line. The bus enables these recurring events only after a nonzero `$420C` write,
so machines that never enable HDMA have no HDMA scheduler callbacks. A write
during VBlank schedules the next frame's initialization and first visible-line
request. A mid-frame enable uses caller-programmed A2A/NLTR until the next
frame. Power-on seeds empty logical HDMA state for that case.

The CPU timing bridge waits for a current CPU or general-DMA operation to end,
then advances the HDMA cursor's overhead, table and data phases through the
shared scheduler/H/V and WRAM-refresh arbiter. HDMA preempts general DMA between
its slots. It uses the same A-bus exclusions and open-bus latch as general DMA,
and data bytes reach the existing PPU port route. CPU access and cycle counters
stay CPU-only. Reports separate HDMA clocks, table bytes and data bytes from
general DMA and refresh. A partly elapsed phase remains pending across a caller
horizon or event-budget failure; bus effects occur once at admitted endpoints.

## Timing basis and limits

[Anomie's Timing Doc](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt),
HDMA section, describes priority over general DMA, frame initialization near
V=0/H=6, visible-line transfer near dot 278, approximate 18-clock global
overhead, 8 direct/24 indirect frame initialization per channel, and visible
line 8-clock channel overhead, 8 clocks per data byte and 16 for an indirect
pointer reload. The document explicitly leaves parts of the pause and bus
subphase timing uncertain. The current model completes an admitted CPU/DMA slot
before HDMA, and commits a whole HDMA byte at its slot endpoint. It does not
model electrical read/write subphases or post-HDMA CPU alignment.

Same-channel general DMA/HDMA register collisions, exact HDMA behavior across
VBlank start, revision-specific zero-descriptor indirect shortening, dynamic
`$2133` overscan changes and active-display PPU port restrictions remain open.
The core has no game-specific path. No PS2, PSP or real-console runtime is
claimed for this block. No source code from another emulator was used or
indexed.

## Deterministic verification

- [x] The machine fetches a direct WRAM table and writes the shared VRAM ports.
  The first data byte stays absent through repeated pre-endpoint horizons and
  appears exactly once at the byte endpoint.
- [x] An indirect table loads a separate WRAM data pointer, writes VRAM through
  that pointer and terminates on the next descriptor in the timed machine path.
- [x] An HDMA line request during general DMA pauses the general cursor
  between slots until the HDMA line completes on a different channel.
- [x] An early synthetic line request crosses WRAM refresh; the 40-clock stall
  completes separately and does not release the HDMA data effect early.
- [x] A zero event budget stops before frame initialization, and retry with
  sufficient budget completes normally.
- [x] A VBlank enable schedules next-frame V=0/H=6 and line dot-278 requests
  for NTSC and PAL; the NTSC short current frame is included.
- [x] HDMA table reads honor blocked A-bus registers and open-bus behavior
  without incrementing CPU access or clock counters.

The block is complete when the host suite, PS2 and PSP cross-build CI pass.
Physical PS2/PSP execution and finer electrical timing remain separate checks.
On 2026-10-25, the C99 `cc` host suite, optimized `-O3` suite, ASan/UBSan suite
with leak detection disabled, repository checker and GCC `-fanalyzer` build
passed. One local generated `test-ppu-bg4-scanline` file had mode `0644` and
stopped a repeat suite run; rebuilding that executable restored the full pass.
The cause of that artifact mode is unconfirmed.
The first published integration revision `e18fe55` passed host CI but failed
the PS2 and PSP link steps because their Makefiles lacked `hdma.o`. The target
object lists are corrected, and the repository checker now guards host/target
core-object parity. Revision `8a0b915` passed all three CI workflows.
Same-channel general-DMA cancellation is covered separately in the
[DMA/HDMA overlap audit](dma-hdma-overlap-audit.md).
