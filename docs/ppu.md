# PPU

## Scope and current state

Separate state, register behavior, rendering, timing and platform presentation.
`core/ppu/counters.c` implements the non-interlaced H/V counter-register
foundation. `core/ppu/vram_ports.c` adds caller-owned 64 KiB VRAM storage and
the `$2115-$2119` write-port subset. VRAM reads, CGRAM/OAM, pixel composition
and presentation are absent. GS/GU
must not be called here; no render API exists.

## Counter-register foundation

Caller-owned state holds two nine-bit captured values, independent read selectors,
the latch flag, PPU2 open bus and version. `$2137` reads capture the current H/V
snapshot if WRIO bit 7 is high and return CPU open bus. `$213C/$213D` alternate low
eight bits and the high bit, using PPU2 open bus for the other seven bits. Capturing
does not reset the selectors. `$213F` resets both selectors and reports current field,
region, version, latch flag and PPU2 open bit 5. The chosen flag-clear policy requires
WRIO bit 7 high. Register writes are ignored without invoking fallback callbacks.

The bus adapter consumes WRIO or external bit-7 falling edges and samples the same
H/V object used by the timing bridge. Horizontal values include dots 323/327 lasting
six master clocks on normal lines and four on the NTSC short line. The complete
clock-by-clock projection is host-tested. This remains a register/timing foundation,
not a rendering implementation or full PPU model.

Initial values are deterministic storage, not a hardware RESET assertion. Capture
is currently immediate at the supplied timestamp and may repeat while the latch flag
is already set. The bus's atomic CPU bridge samples the pre-instruction timestamp.
Exact WRIO capture delay, simultaneous flag read/capture and held-low behavior remain
unverified; do not infer cycle-perfect gun or raster behavior. External edge setters
must be followed by bus synchronization before advancing time. See
[hv-counter-latch-audit.md](hv-counter-latch-audit.md) for sources and verification.

## VRAM write-port subset

The machine optionally accepts a 64 KiB caller-owned VRAM buffer and routes
PPU writes to the write-port state before any external PPU callback. VMAIN
selects increment after low/high byte, step 1/32/128 words, and 32/64/128-word
full-graphic remapping. The 16-bit logical address wraps naturally; physical
VRAM wraps at 32K words. A machine power-on resets port registers but retains
buffer bytes. This is storage semantics only: `$2139/$213A` reads, active-display
access qualification, DMA/HDMA, register reset values on actual hardware, and
rendering remain open. See [VRAM port audit](ppu-vram-write-audit.md).

## Design direction

Begin with sourced register semantics and independent state tests, then reference scanline output. Decide synchronization granularity from software-visible behavior, including mid-frame writes. Candidate accelerated pixel kernels need exact formats, alignment and reference equivalence; final frame upload belongs to platform.

## Verification criteria

Fixtures must exercise port increment/remap, VRAM/CGRAM/OAM boundaries, tile decode, all modes, priority ties, sprite limits, clipping/windows, math saturation, hires/interlace/overscan and latching/open bus. Keep pixel golden data independently derived. A visible title screen is insufficient.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Unchecked items
remain incomplete; narrow verified counter behavior does not complete the PPU.

- [ ] [IN PROGRESS] PPU register model — counter and VRAM write subsets
- [ ] [IN PROGRESS] VRAM — 64 KiB storage and CPU write port; reads and rendering absent
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
- [ ] [IN PROGRESS] Relevant latching behavior — H/V subset with explicit timing approximations
- [ ] [IN PROGRESS] Relevant open-bus behavior — counter-register PPU2 latch only
- [ ] Scanline integration
- [ ] [IN PROGRESS] VRAM port increment/remap behavior — write path checked; read and access-window behavior absent
- [ ] Sprite limits and overflow flags
- [ ] Mid-frame register effects
- [ ] [IN PROGRESS] PPU tests — counter and VRAM write-port fixtures
- [ ] Reference renderer integration

## Research and decisions

Counter evidence and unresolved hardware behavior are recorded in the audit above.
Use manufacturer documentation and independent hardware research, never another
emulator's implementation. SDK documentation is not a source for SNES behavior.
