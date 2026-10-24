# PPU

## Scope and current state

Separate state, register behavior, rendering, timing and platform presentation.
`core/ppu/counters.c` implements the non-interlaced H/V counter-register
foundation. `core/ppu/vram_ports.c` adds caller-owned 64 KiB VRAM storage,
the `$2115-$2119` write ports and `$2139/$213A` buffered read ports.
`core/ppu/cgram_ports.c` owns the `$2121/$2122/$213B` palette port state
over a caller-owned 512-byte CGRAM buffer. `core/ppu/oam_ports.c` adds
`$2101-$2104/$2138` over 544 caller-owned OAM bytes. `core/ppu/tile4.c`
decodes one 4bpp VRAM tile into color indices. Pixel composition
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

## VRAM port subset

The machine optionally accepts a 64 KiB caller-owned VRAM buffer and routes
its VRAM ports before any external PPU callback. VMAIN
selects increment after low/high byte, step 1/32/128 words, and 32/64/128-word
full-graphic remapping. The 16-bit logical address wraps naturally; physical
VRAM wraps at 32K words. A machine power-on resets port registers but retains
buffer bytes. `$2139/$213A` return the previous buffered word; the selected
incrementing read fetches the current remapped word before advancing the address.
Changing VMADD does not reload the buffer, so the first selected read is a
dummy read. Initial buffer contents are a deterministic zero policy, not a
hardware RESET claim. Active-display access qualification, mixed read/write
effects, DMA/HDMA, register reset values on actual hardware and rendering remain
open. See the [write](ppu-vram-write-audit.md) and
[read](ppu-vram-read-audit.md) audits.

## CGRAM port subset

The machine optionally accepts 512 caller-owned CGRAM bytes and routes
`$2121/$2122/$213B` through the shared core before external PPU callbacks.
`$2121` selects one of 256 colors and resets the low/high selector. The first
`$2122` write latches the low byte; the second commits a 15-bit color and
advances the eight-bit address. `$213B` reads low then high and advances after
high; high bit 7 comes from the PPU2 open-bus latch. Power-on resets the port
state but retains memory. Active-display access and mixed read/write sequences
need hardware checks. See the [CGRAM audit](ppu-cgram-port-audit.md).

## OAM port subset

The machine optionally accepts 544 caller-owned OAM bytes. `$2102/$2103`
select a nine-bit word address and reset the internal byte pointer on either
write. `$2104` commits low-table pairs on the odd-byte write and high-table
bytes immediately. `$2138` reads one byte and advances the pointer. The
high table mirrors over its 512-byte logical region, physically using 32
bytes. Port state resets on machine power-on; OAM bytes remain. Priority
rotation chooses the first sprite from the current internal OAM address. A machine
scheduler event reloads the internal address at horizontal dot 10 of the
VBlank start line while forced blank is off; `$2100` bit 7 supplies the gate.
This timing choice follows independent hardware notes and still needs a real
hardware check. Power-on begins in deterministic forced-blank state. A
`$2100` bit-7 falling edge also reloads OAM during the first VBlank scanline.
Independent sources disagree about transitions on other scanlines; that case,
active-display access and rendering remain open. See the
[OAM port audit](ppu-oam-port-audit.md).

`$2101` is now retained alongside the OAM ports. A read-only core helper
decodes one of 128 sprite records directly from caller-owned OAM without
touching the CPU port pointer. It exposes the packed high-table X/size bits
and low-table fields; a second helper maps the six documented `$2101` size
pairs to square pixel dimensions. Undocumented size codes 6-7 return failure.
The descriptor helper does not decide scanline visibility or priority rotation.
An additional pure helper resolves an unflipped local 8×8 tile to its first
VRAM word using OBJSEL base/name bits. It wraps rows and columns within the
selected 16×16 character grid and rejects tile coordinates outside the
selected sprite size. It does not fetch or decode tile pixels.

## OBJ scanline selection

`core/ppu/sprite_scanline.c` scans the 128 OAM records from an explicit
first-sprite index and sprite-addressed Y value. It retains at most 32
sprites, then selects up to 34 visible 8-pixel slivers from those sprites
in reverse priority order. X=`$100` counts against both limits despite
being off-screen. The returned sliver records carry their source tile
coordinates after flip and true screen X. The machine now evaluates the
next visible line at HBlank start, retains its bounded selection and ORs
the two overflow bits into `$213E` until the next frame begins. Reads
return PPU1 version 1 and the PPU1 open-bus bit 5. Priority rotation uses
the current internal OAM word address at that event. This is a documented
phase approximation: exact overflow assertion timing,
forced-blank transitions, active-display OAM writes, tile fetch and pixels
remain open at exact hardware timing. With caller-owned VRAM, the machine
composes selected 4bpp slivers into a 256-entry OBJ line, carrying palette
index, priority and source sprite; transparent color zero leaves lower sprites
visible. BG mixing and output are absent. See the
[scanline audit](ppu-sprite-scanline-audit.md).

## 4bpp tile decode

The shared decoder reads the four interleaved bitplanes of one 8×8 tile
from caller-owned 64 KiB VRAM and writes 64 row-major indices in 0-15.
The OBJ tile-address helper can supply its word address; the decoder itself
also applies the 32K-word VRAM wrap. Color index zero remains zero for the
caller to interpret in its layer context. No palette, flips, sprite range,
pixel composition or target presentation is part of this API. See the
[tile audit](ppu-tile4-audit.md).

## Design direction

Begin with sourced register semantics and independent state tests, then reference scanline output. Decide synchronization granularity from software-visible behavior, including mid-frame writes. Candidate accelerated pixel kernels need exact formats, alignment and reference equivalence; final frame upload belongs to platform.

## Verification criteria

Fixtures must exercise port increment/remap, VRAM/CGRAM/OAM boundaries, tile decode, all modes, priority ties, sprite limits, clipping/windows, math saturation, hires/interlace/overscan and latching/open bus. Keep pixel golden data independently derived. A visible title screen is insufficient.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Unchecked items
remain incomplete; narrow verified counter behavior does not complete the PPU.

- [ ] [IN PROGRESS] PPU register model — counter, VRAM, CGRAM and OAM port subsets
- [ ] [IN PROGRESS] VRAM — 64 KiB storage and CPU read/write ports; access windows and rendering absent
- [ ] [IN PROGRESS] CGRAM — CPU read/write ports; access windows and rendering absent
- [ ] [IN PROGRESS] OAM — CPU ports, scheduled VBlank reload, sprite descriptor decode and tile-address lookup; display access and rendering absent
- [ ] [IN PROGRESS] Sprite limits — 32 OBJ/34 tile selection and scheduled `$213E` flags; exact timing and pixels absent
- [ ] [IN PROGRESS] Tile decode fixtures — 4bpp tile indices and OBJ address handoff tested; other formats and renderer absent
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
- [ ] [IN PROGRESS] Relevant open-bus behavior — counter and CGRAM high-read PPU2 subset
- [ ] Scanline integration
- [ ] [IN PROGRESS] VRAM port increment/remap behavior — read/write paths checked; access-window and mixed-port behavior pending
- [ ] Sprite limits and overflow flags
- [ ] Mid-frame register effects
- [ ] [IN PROGRESS] PPU tests — counter, VRAM, CGRAM and OAM port fixtures
- [ ] Reference renderer integration

## Research and decisions

Counter evidence and unresolved hardware behavior are recorded in the audit above.
Use manufacturer documentation and independent hardware research, never another
emulator's implementation. SDK documentation is not a source for SNES behavior.
