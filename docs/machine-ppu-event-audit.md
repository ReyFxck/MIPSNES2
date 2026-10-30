# Machine PPU event integration audit

Date: 2026-10-29  
Scope: recurring machine HBlank line events, reference rendering and synchronous
BGR555 video handoff across two NTSC frames

## Integrated path

`tests/test_machine_ppu_frame.c` builds a synthetic valid LoROM whose 5A22 stays
in a branch loop. Caller-owned VRAM contains a hand-built Mode 0 2bpp tile and a
32×32 tilemap with varying palettes; caller-owned CGRAM contains a deterministic
256-color pattern. The fixture enables BG1 and full brightness through the real
machine bus, then advances only through `mipsnes2_machine_run_until`.

At each provisional HBlank event, the existing machine path renders the selected
background, resolves CGRAM and brightness, and invokes the caller's synchronous
video-line callback with 256 nominal BGR555 pixels. The callback verifies line
order and hashes each line number followed by every pixel as explicit
little-endian bytes. No GS/GU, VSync, wall clock or platform buffer participates.

## Event and chunk evidence

The final absolute horizon covers one 262-line NTSC frame, one short-field frame
and then stops one master clock before the third frame's first HBlank callback.
The complete setup is run twice:

1. regular 50,000-master-clock increments;
2. irregular increments of 1, 1,095, 1, 1,363, 32,768, 49,999, 17 and 41,000
   clocks, deliberately splitting the first HBlank and scanline boundaries.

Both executions must produce:

- 448 callbacks in exact `0..223, 0..223` order;
- 43,008 non-black pixels;
- canonical BGR555 stream hash `911273792f0b2425`;
- two completed H/V frames and matching final scheduler/H/V state;
- `V=0` and `H=1095`, immediately before the next callback at `H=1096`.

The fixed hash is a regression sentinel over the existing independently checked
tile, BG, mixer, CGRAM, brightness, scheduler and handoff components. It is not a
hardware capture and does not validate unimplemented PPU modes or exact internal
render phases.

## Verification and limits

Local evidence consists of the focused fixture, complete host suite, ASan+UBSan,
GCC `-fanalyzer`, repository checks and diff checks. Published Host, PS2 and PSP
workflow results are recorded in `STATUS.md` after completion. Cross-builds do not
prove target display output.

The Phase 9 PPU event/scanline connection is host verified. Mid-line register
effects, remaining modes, hires/interlace, exact access windows, scaling, frame
buffering and final GS/GU presentation remain open.

Next primary milestone: capture the existing bounded RFAuds2 lifecycle diagnostic
on the named Android PS2 emulator with version/settings and ELF checksum.
