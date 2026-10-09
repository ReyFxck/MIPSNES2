# PPU

## Scope and current state

Separate state, register behavior, rendering, timing and platform presentation. Core owns VRAM/CGRAM/OAM and SNES priorities, windows, color math and latches. GS/GU must not be called here. No PPU code or render API exists.

## Design direction

Begin with sourced register semantics and independent state tests, then reference scanline output. Decide synchronization granularity from software-visible behavior, including mid-frame writes. Candidate accelerated pixel kernels need exact formats, alignment and reference equivalence; final frame upload belongs to platform.

## Verification criteria

Fixtures must exercise port increment/remap, VRAM/CGRAM/OAM boundaries, tile decode, all modes, priority ties, sprite limits, clipping/windows, math saturation, hires/interlace/overscan and latching/open bus. Keep pixel golden data independently derived. A visible title screen is insufficient.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] PPU register model
- [ ] VRAM
- [ ] CGRAM
- [ ] OAM
- [ ] BG Mode 0
- [ ] BG Mode 1
- [ ] BG Mode 2
- [ ] BG Mode 3
- [ ] BG Mode 4
- [ ] BG Mode 5
- [ ] BG Mode 6
- [ ] Mode 7
- [ ] Sprites
- [ ] Sprite priorities
- [ ] BG priorities
- [ ] Window system
- [ ] Color math
- [ ] Brightness
- [ ] Mosaic
- [ ] Scrolling
- [ ] Offset-per-tile
- [ ] Hires
- [ ] Interlace
- [ ] Overscan
- [ ] Relevant latching behavior
- [ ] Relevant open-bus behavior
- [ ] Scanline integration
- [ ] VRAM port increment/remap behavior
- [ ] Sprite limits and overflow flags
- [ ] Mid-frame register effects
- [ ] PPU tests
- [ ] Reference renderer integration

## Research and decisions

No emulation semantics are claimed verified by bootstrap. Before implementation,
record the exact hardware reference, section/revision and independently authored
regression evidence. SDK documentation is not a source for SNES behavior.
Intentional approximations: none implemented. Known subsystem bugs: not yet testable.
