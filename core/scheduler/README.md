# core/scheduler

Contains the caller-owned deterministic scheduler foundation, exact integer clock-
ratio accumulator and Ricoh 5A22 logical-address timing classifier. Events use SNES
master-clock timestamps and deterministic timestamp/priority/insertion ordering. A
shared coordinator advances the scheduler and non-interlaced H/V component through
one master-clock domain; the CPU bridge can feed it one atomic instruction delta.

This is not complete SNES scheduling. Exact within-instruction bus timing, automatic
CPU interrupt-line delivery, refresh stalls, DMA/HDMA ownership, PPU/APU clocks and
hardware subsystem tie priorities remain open.
See [scheduler documentation](../../docs/scheduler.md) and the
[canonical status](../../STATUS.md).
