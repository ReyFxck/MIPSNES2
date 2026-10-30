# Exact frame-boundary horizon audit

Date: 2026-10-30

## Problem

The PS2 ROM diagnostic previously advanced to multiples of `262 × 1364` master
clocks. That is not an exact NTSC frame cadence: field one removes four clocks from
line 240. The first current non-interlaced frame is 357,364 clocks and the next is
357,368 clocks. Reusing the longer constant stopped four clocks into alternating
frames and accumulated the wrong capture/input/audio/video boundaries.

## Implementation

`mipsnes2_hv_clocks_until_frame_boundary` computes the remaining clocks from the
current valid H/V state without mutating it. The calculation handles a call before
the H=1/V=0 field toggle, a partial current scanline, the field-one NTSC short line
and PAL's normal 312-line frame. Invalid state leaves the caller's output unchanged.

Before each bounded PS2 frame, the launcher asks this shared timing state for the
distance and adds it to `machine.scheduler.now` to form an absolute horizon. It
continues with audio pumping and GS/DMA/vsync only when the machine reports that it
reached that horizon. Input is sampled before the machine call. None of those device
operations can request additional machine work or change the next H/V calculation.

## Verification and limits

The H/V regression checks null/uninitialized inputs, the initial 357,364-clock NTSC
field, a 1,000-clock partial phase, the following 357,368-clock field, a complete PAL
frame and the final clock before its boundary. The complete host suite and repository
boundary/link checker pass. A focused ASan+UBSan H/V suite and GCC `-fanalyzer` on
the timing source also pass. PS2 CI is required after publication because the
changed launcher cannot execute in the host fixture.

This fixes frame-horizon arithmetic only. It does not add a continuous gameplay
loop, dynamic PAL/overscan selection, interlace, device-driven pacing, frameskip or
target runtime evidence. No compatibility or performance result is claimed.

## Primary next milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
emulator version/settings and on-device ELF checksum recorded.
