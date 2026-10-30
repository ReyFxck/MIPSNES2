# PS2 persistent GS presenter audit

Date: 2026-10-30

## Scope

The PS2 backend now owns one packet and one 256×256 RGBA32 GS framebuffer across
the fixed `--rom` diagnostic horizons. Initialization allocates both resources and
configures GIF DMA/GS once. Each upload resets and rebuilds the retained packet,
flushes the separately referenced 64-byte-aligned EE texture, submits the chain,
waits for DMA completion and then waits for vertical sync. Shutdown waits for the
channel and releases the VRAM allocation, packet, graph state and DMA channel.

The ROM path resets its capture bookkeeping before each fixed absolute emulated
frame horizon and presents only after `mipsnes2_machine_run_until` returns. GS/DMA
waits therefore affect wall time only; they cannot request work from the machine or
change its emulated clock.

## Source basis

The lifecycle follows the official PS2SDK source at pinned revision
`4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5`:

- [graph sample](https://github.com/ps2dev/ps2sdk/blob/4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5/ee/graph/samples/graph.c)
  for retained packets, DMA waits and graph/VRAM/DMA shutdown;
- [packet API](https://github.com/ps2dev/ps2sdk/blob/4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5/ee/packet/include/packet.h)
  for packet reset and lifetime;
- [texture sample](https://github.com/ps2dev/ps2sdk/blob/4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5/ee/draw/samples/texture/texture.c)
  for transfer-chain construction;
- [DMA implementation](https://github.com/ps2dev/ps2sdk/blob/4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5/ee/dma/src/dma.c)
  for packet-versus-separately-referenced cache behavior.

This is original platform code using SDK APIs; no source from another emulator was
used.

## Verification and limits

`test-ps2-gs-presenter` supplies narrow SDK stubs and covers packet-allocation,
VRAM-allocation, DMA-init and graph-init failures; invalid/unaligned/short images;
chain-send failure; two successful uploads without another allocation; exact
cache-flush/DMA-wait/vsync counts; and one-time cleanup. The existing frame-capture
and machine tests cover the RGBA32 producer. The PS2 CI cross-build checks the real
pinned SDK declarations and link set.

Local validation includes the complete `make -j2 test` suite, a focused ASan+UBSan
build with leak detection disabled for the managed host, and GCC `-fanalyzer` over
the presenter plus its stubs. The architecture and performance records now describe
the retained ownership and unmeasured upload cost. No compatibility result follows.

No ARMSX2, NetherSX2 or physical-PS2 execution of this revision is claimed. The
presenter is still one framebuffer with no scaling, aspect policy, double buffering,
field handling, overlay or continuous gameplay loop. Its real cache behavior,
transfer latency, tearing and frame pacing remain unmeasured.

## Primary next milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
emulator version/settings and on-device ELF checksum recorded.
