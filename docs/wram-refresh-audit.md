# Shared WRAM refresh stall audit

Date: 2026-10-24

## Scope

The shared scheduler bridge now pauses an in-flight CPU operation or general DMA
phase for the SNES WRAM refresh window. The implementation is platform-neutral;
PS2 and PSP compile the same core and contain no refresh policy.

The reference timing model uses a 40-master-clock pause near H=536. The first
window begins at H=538. Later starts retain that absolute modulo-eight phase and
select the point closest to H=536, producing H=538/H=534 alternation on normal
1364-clock lines. A 1360-clock NTSC short line retains the current phase.

These rules follow the measured timing description in the
[Super Famicom Development Wiki timing notes](https://wiki.superfamicom.org/timing)
and the 40-clock DRAM-refresh summary in the
[SNESdev timing documentation](https://snes.nesdev.org/wiki/Timing). Nintendo's
[SNES Development Manual, Book I](https://archive.org/details/SNESDevManual)
identifies the 128 KiB work memory used by the S-CPU; it does not provide the
measured H-clock sequence used here. No emulator implementation or game-specific
source was consulted or copied.

## Implementation properties

- The existing H/V master-clock domain derives the current or next refresh window
  at any valid timestamp, including entry in the middle of a window.
- CPU and DMA share one stall path. The pending operation/phase endpoint moves by
  the unelapsed refresh duration and its data effect remains uncommitted.
- A horizon or exhausted event budget can pause before or within refresh and resume
  without charging or applying the stall twice.
- Scheduler callbacks at or before the refresh boundary still run through the
  existing H/V-before-callback order.
- `refresh_master_clocks` reports completed refresh contribution separately.
  `master_clocks` includes committed CPU, DMA and refresh clocks, while
  `dma_master_clocks` continues to report DMA phases alone.
- Calls without an H/V timing domain retain their previous behavior because they
  do not have enough scanline information to place refresh.

## Deterministic verification

- [x] CPU read ending exactly at H=538 commits before the following internal
  operation stalls.
- [x] Horizons at H=537, H=538, H=577 and H=578 preserve exact-once CPU effects
  and charge refresh only after completion.
- [x] The next normal line places the pause at H=534.
- [x] NTSC line 240 and the line following its 1360-clock short length retain the
  H=538 absolute phase.
- [x] A general DMA setup crossing H=538 performs no early byte effect, then
  completes with distinct DMA and refresh clock totals.
- [x] Default host, optimized and sanitizer suites pass.

## Honest boundary

The 40-clock pause and measured alignment are a digital timing model. Exact S-CPU
pin phases, CPU-cycle subphases, early hardware revisions, refresh interaction with
HDMA and electrical bus ownership remain unverified. The model pauses complete
software operation/phase endpoints; it does not claim transistor-level timing.

## Completion criterion

This block is complete when CPU and general DMA use the same resumable refresh
path; normal and NTSC-short-line alignment fixtures pass; default and ASan/UBSan
host suites pass; and host, PS2 and PSP CI pass for the published implementation.
The next primary milestone is HDMA initialization and per-scanline arbitration.
