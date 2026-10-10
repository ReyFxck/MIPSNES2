# Cartridge

## Scope and current state

Own ROM metadata, mapper selection, ROM/SRAM views and chip declaration. Platform loads bytes and persists SRAM through its filesystem; core sees buffers. No loader or mapper implementation exists.

## Design direction

Validate size, candidate headers and documented mapper fields; define copier-header and ambiguous-header policy. Detect hardware, not games. Handle LoROM/HiROM/ExLoROM/ExHiROM separately. Checksums may aid diagnostics but must not invent game-specific mapping.

## Verification criteria

Use original synthetic cartridges covering mirrors, bank boundaries, SRAM sizes and malformed/truncated data. Prove detection independent of ROM titles. SRAM persistence integration needs explicit save/error behavior and platform tests.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] ROM loading
- [ ] ROM metadata
- [ ] Header analysis
- [ ] LoROM
- [ ] HiROM
- [ ] ExLoROM
- [ ] ExHiROM
- [ ] SRAM
- [ ] SRAM persistence
- [ ] Mapper tests
- [ ] Checksum handling where useful
- [ ] Truncated and malformed input rejection
- [ ] Copier-header policy
- [ ] Region detection

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
