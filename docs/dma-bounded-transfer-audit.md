# Bounded general-purpose DMA byte cursor

The shared core can now iterate a requested mask of DMA channels, lowest
channel first, with a caller-chosen byte budget. Each step calls a
platform-neutral A/B-bus data-phase callback once. The callback receives the
24-bit A address, `$21xx` B port and transfer direction. The cursor updates
the 16-bit A offset without crossing the A bank, decrements DAS (including
the zero-as-65536 case), and preserves its mode position across pauses. The
eight modes use the separately tested B-port sequences.

This is an **untimed** core primitive. Its callback contract assumes the
caller performs one byte's effects exactly once and does not mutate DMA
registers during the callback. The current machine does not invoke it:
`$420B` has no start effect, CPU suspension and scheduler arbitration are
absent, and DMA-specific A/B restrictions (including I/O and WRAM-port
conflicts) are not yet applied. An ordinary pair of CPU bus read/write calls
would charge two CPU accesses and is not a valid substitute for one DMA data
phase. HDMA is out of scope. No target runtime behavior is claimed.

The synthetic host fixture verifies masked channel order, byte-budget
pause/resume without repeated callbacks, forward/reverse direction, all eight
B-port modes, `$21FF` wrapping, 16-bit A offset wrapping with fixed bank,
fixed/decrement/increment, per-byte A1T/DAS updates, transfer completion and
the full 65536-byte zero-count path. It uses a callback with test-owned byte
arrays; it does not verify machine bus side effects or hardware timing.

Hardware references consulted on 2026-10-24:

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), Direct Memory Access and CPU register sections.
- [Super Famicom Development Wiki DMA/HDMA overview](https://wiki.superfamicom.org/dma-and-hdma), channel priority, byte direction, address/count update and A/B-bus constraints.

The implementation and tests are original; no emulator source was used.
