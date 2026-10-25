# DMA / HDMA

## Scope and current state

SNES DMA and HDMA are emulated bus masters. PS2 host DMA transfers data around the target machine. These are completely different layers; speeding up a host copy does not emulate SNES transfer timing. The shared core models eight channel register banks and both general DMA and HDMA operation cursors. The resumable CPU bridge services `$420B` and `$420C`, pauses CPU operations for general DMA and visible-line HDMA, advances scheduler/H/V time and shares WRAM refresh stalls. Same-channel cancellation and scheduled VBlank deactivation are integrated; electrical phases and hardware-revision edges remain pending.

## Design direction

Model channel registers, transfer modes and address stepping/wrapping, then central scheduler arbitration with CPU, scanline events and indirect tables. Establish source/destination side effects and behavior at event boundaries with evidence.

## Verification criteria

Test every transfer mode, channel ordering, zero/maximum counts, fixed/increment/decrement addressing, bank edges, HDMA line counters, indirect tables and CPU/PPU interactions. Include interactions of regular DMA and HDMA, not just byte-copy tests.

## Work breakdown

Planning checklist; [STATUS.md](../STATUS.md) is canonical. Register and
transfer-mode foundations are partial; document implementation evidence before
promotion.

- [ ] [IN PROGRESS] DMA registers — channel bytes, `$420B` start and `$420C` enable mask; same-channel arbitration pending
- [ ] [IN PROGRESS] DMA transfer modes — timed general DMA and direct/indirect HDMA; electrical phases pending
- [ ] [IN PROGRESS] DMA timing — slots/setup/resynchronization, HDMA priority and shared refresh stalls; electrical phases pending
- [ ] DMA source/destination wrapping
- [ ] [IN PROGRESS] HDMA initialization — V=0 request, table engine and VBlank deactivation; revision edges pending
- [ ] [IN PROGRESS] HDMA scanline behavior — timed HBlank requests and line transfers; electrical phase pending
- [ ] [IN PROGRESS] Indirect HDMA — timed table/pointer path; zero-descriptor edge pending
- [ ] Transfer edge cases
- [ ] CPU interaction
- [ ] Channel priority
- [ ] [IN PROGRESS] DMA and HDMA overlap — same-channel cancellation at slot boundaries; electrical phase pending
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
Frame/line scheduling and priority are in the
[timed HDMA audit](hdma-timed-audit.md).
Same-channel cancellation is scoped in the
[DMA/HDMA overlap audit](dma-hdma-overlap-audit.md).
VBlank deactivation is scoped in the [HDMA VBlank audit](hdma-vblank-audit.md).
