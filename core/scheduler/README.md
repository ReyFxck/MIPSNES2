# core/scheduler

Contains the caller-owned deterministic scheduler foundation, exact integer clock-
ratio accumulator and Ricoh 5A22 logical-address timing classifier. Events use SNES
master-clock timestamps and deterministic timestamp/priority/insertion ordering. A
shared coordinator advances the scheduler and non-interlaced H/V component through
one master-clock domain. The opt-in, caller-owned CPU-operation timing state can hold
one cursor operation in flight through event horizons and budget retries, then commit
its effect exactly once at the endpoint. Existing atomic CPU-step APIs remain
compatibility entry points. A caller-owned interrupt-line aggregator preserves
independent IRQ sources and hands CPU-I/O IRQ/NMI state across instruction boundaries.

This is not complete SNES scheduling. Exact electrical bus/interrupt timing,
refresh stalls, DMA/HDMA ownership, PPU/APU clocks and
hardware subsystem tie priorities remain open.
See [scheduler documentation](../../docs/scheduler.md) and the
[canonical status](../../STATUS.md).
