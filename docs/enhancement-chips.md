# Enhancement hardware

Base SNES comes first. No enhancement implementation or per-game fallback exists.
A cartridge describes actual hardware. Chip state connects to the bus, scheduler and
interrupt system through explicit ownership and timing contracts. Do not implement
chips during bootstrap or paste cores from existing emulators.

Research instruction/command behavior, register state, mapping, bus contention,
timing and signaling individually. Some chips are command/decompression devices
rather than general CPUs: research applicability instead of inventing interrupts.
Firmware/table provenance must be settled before distribution. A "not applicable"
decision requires hardware evidence and documentation.

Each chip has a separate detailed checklist in [STATUS.md](../STATUS.md) and Phase 12
of [ROADMAP.md](../ROADMAP.md): Super FX, SA-1, DSP-1/2/3/4, CX4, S-DD1, SPC7110,
OBC-1 and other commercially relevant hardware. For each require original test
vectors, bus/timing integration, representative game validation and separate PS2/PSP
profiling. Super FX must include Star Fox and Yoshi's Island; SA-1 includes Super Mario
RPG. Working games are evidence of chip behavior, never a reason for CRC hacks.

- [ ] Base SNES exit criteria satisfied before chip implementation
- [ ] Chip-interface design after bus/scheduler behavior stabilizes
- [ ] Firmware/table licensing and provenance inventory
- [ ] Independent per-chip regressions
- [ ] Real PS2 and PSP validation for claimed supported chips
