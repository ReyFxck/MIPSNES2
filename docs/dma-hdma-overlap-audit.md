# DMA and HDMA same-channel overlap audit

Date: 2026-10-25

## Behavior and boundary

The [Super Famicom Development Wiki DMA/HDMA description](https://wiki.superfamicom.org/dma-and-hdma)
states that HDMA takes priority, pauses general DMA, and terminates a
conflicting general-DMA channel. It also notes that a terminated channel can
retain a nonzero `$43x5-$43x6` count. This core implementation cancels each
general-DMA channel selected by an admitted HDMA frame or scanline operation.
It waits for an in-progress eight-clock general-DMA slot to finish, then
prevents further bytes from that channel. An unrelated general-DMA channel
continues after HDMA completes and receives its own channel setup phase.

The transfer cursor owns channel selection and count updates. The shared
scheduler owns arbitration and changes the general-DMA phase when HDMA removes
the current channel. No platform-specific code or game-specific condition is
involved. This is a slot-level model: the physical arbitration instant inside
a byte and register behavior under sub-byte overlap are not established.

## Deterministic verification

- [x] Pure cursor fixture cancels a future channel, then an active channel
  after one committed byte. The active DAS/A1T retain their partial values;
  the next eligible channel completes once.
- [x] Timed machine fixture begins a long general DMA on channel 0 and a
  one-byte DMA on channel 1 before a visible-line HDMA request on channel 0.
  At HDMA admission, channel 0 is removed with nonzero DAS, channel 1 remains,
  HDMA completes one data byte, and channel 1 later reaches zero DAS.
- [x] Revision `0d1d64b` passed
  [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937420),
  [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937429)
  and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937440).
- [ ] Real PS2 and PSP execution; no device result is claimed.

The timed fixture uses a generated in-memory cartridge, WRAM and VRAM; it
contains no commercial ROM. The exact hardware behavior if a descriptor
terminates an HDMA channel immediately before a general-DMA overlap remains
open. VBlank deactivation and electrical transfer phases are separate work.
No source code from another emulator was used or indexed.
