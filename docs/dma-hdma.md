# DMA / HDMA

## Scope and current state

SNES DMA and HDMA are emulated bus masters. PS2 host DMA transfers data around the target machine. These are completely different layers; speeding up a host copy does not emulate SNES transfer timing. The shared core models eight channel register banks, all general-purpose mode/address sequences and an operation-granular but untimed HDMA table/scanline engine. The resumable CPU bridge services `$420B`, pauses the CPU, advances scheduler/H/V time and shares WRAM refresh stalls with CPU operations. `$420C` is routed, but timed HDMA arbitration remains pending.

## Design direction

Model channel registers, transfer modes and address stepping/wrapping, then central scheduler arbitration with CPU, scanline events and indirect tables. Establish source/destination side effects and behavior at event boundaries with evidence.

## Verification criteria

Test every transfer mode, channel ordering, zero/maximum counts, fixed/increment/decrement addressing, bank edges, HDMA line counters, indirect tables and CPU/PPU interactions. Include interactions of regular DMA and HDMA, not just byte-copy tests.

## Work breakdown

Planning checklist; [STATUS.md](../STATUS.md) is canonical. Register and
transfer-mode foundations are partial; document implementation evidence before
promotion.

- [ ] [IN PROGRESS] DMA registers — channel bytes, `$420B` start and `$420C` enable mask; timed HDMA requests pending
- [ ] [IN PROGRESS] DMA transfer modes — timed general DMA plus pure direct/indirect HDMA paths; timed HDMA/electrical phases pending
- [ ] [IN PROGRESS] DMA timing — slots/setup/resynchronization and shared refresh stalls; HDMA/electrical phases pending
- [ ] DMA source/destination wrapping
- [ ] [IN PROGRESS] HDMA initialization — pure table engine; V=0 scheduler trigger pending
- [ ] [IN PROGRESS] HDMA scanline behavior — pure line counters/transfers; HBlank arbitration pending
- [ ] [IN PROGRESS] Indirect HDMA — table/pointer path implemented; timed edge cases pending
- [ ] Transfer edge cases
- [ ] CPU interaction
- [ ] Channel priority
- [ ] DMA and HDMA overlap
- [ ] Tests

## Research and decisions

The register/address-plan evidence and hardware references are recorded in the
[DMA foundation audit](dma-register-foundation-audit.md) and
[bounded transfer audit](dma-bounded-transfer-audit.md). Before extending
transfer behavior, record the exact hardware reference and independently
authored regression evidence. SDK documentation is not a source for SNES
behavior. Intentional timing approximations: none implemented yet.

The current machine integration and explicit timing approximations are detailed
in the [machine DMA audit](dma-machine-audit.md), with refresh follow-up in the
[WRAM refresh audit](wram-refresh-audit.md) and logical HDMA state in the
[HDMA table audit](hdma-table-audit.md). The resumable operation contract and
approximate per-slot costs are in the [HDMA cursor audit](hdma-cursor-audit.md).
