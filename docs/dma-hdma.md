# DMA / HDMA

## Scope and current state

SNES DMA and HDMA are emulated bus masters. PS2 host DMA transfers data around the target machine. These are completely different layers; speeding up a host copy does not emulate SNES transfer timing. The shared core models eight channel register banks and all general-purpose mode/address sequences. A bounded untimed cursor issues byte effects through a caller-supplied A/B-bus callback; the resumable CPU bridge now services `$420B`, pauses the CPU and advances scheduler/H/V time. Refresh and HDMA arbitration remain pending.

## Design direction

Model channel registers, transfer modes and address stepping/wrapping, then central scheduler arbitration with CPU, scanline events and indirect tables. Establish source/destination side effects and behavior at event boundaries with evidence.

## Verification criteria

Test every transfer mode, channel ordering, zero/maximum counts, fixed/increment/decrement addressing, bank edges, HDMA line counters, indirect tables and CPU/PPU interactions. Include interactions of regular DMA and HDMA, not just byte-copy tests.

## Work breakdown

Planning checklist; [STATUS.md](../STATUS.md) is canonical. Register and
transfer-mode foundations are partial; document implementation evidence before
promotion.

- [ ] [IN PROGRESS] DMA registers — channel bytes and general DMA start; HDMA enable effects absent
- [ ] [IN PROGRESS] DMA transfer modes — timed general DMA machine path; HDMA and electrical phases pending
- [ ] [IN PROGRESS] DMA timing — slots/setup/resynchronization; refresh and HDMA pending
- [ ] DMA source/destination wrapping
- [ ] HDMA initialization
- [ ] HDMA scanline behavior
- [ ] Indirect HDMA
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
in the [machine DMA audit](dma-machine-audit.md).
