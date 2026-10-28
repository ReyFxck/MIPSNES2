# S-DSP echo and final-output audit

Date: 2026-10-28  
Scope: sample-level echo-send selection, ARAM ring, FIR, feedback, final mixing
and mute-gated output state

## Evidence and implementation boundary

The implementation was independently written from hardware documentation and
published technical descriptions. No emulator source or emulator-derived table
was used.

- Nintendo's SNES Development Manual, Book I sound-DSP block diagram and echo
  cautions establish the voice/main/echo/feedback topology and echo-RAM ownership.
- The SFC Sound Manual describes `EON`, signed volumes, `ESA`, `EDL`, FIR
  coefficients and `FLG` controls.
- SnesLab's S-DSP register, FIR-filter and sample-generation-loop pages document
  register addresses, signed 1.7 tap arithmetic, history order, channel separation,
  echo-write protection and aggregate access order.
- Anomie's S-DSP document revision 1212 independently specifies the signed volume
  shifts, left-aligned signed-15 RAM representation, first-seven FIR wrap,
  newest-tap clamp, `EDL=0` behavior, delayed `ESA` use and continuing processing
  while muted or write-protected.

Exact URLs and provenance are recorded in [references.md](references.md).

This slice operates once per existing 32-S-SMP-cycle sample event. It does not
claim the exact cycles 22-30 echo microphases or register-write race windows.

## Implemented state and arithmetic

`mipsnes2_dsp_echo` exposes two eight-entry signed-15 histories, the ordered
`EON` send mix, FIR and feedback results, the value offered to ARAM, resolved
voice mask, current address, latched base, offset, length and write gate.
`mipsnes2_dsp_output` exposes the echo-volume result, final clamped mix and the
mute-gated DAC-facing pair. These are deterministic inspection boundaries, not
a PCM transport.

For each attached-ARAM sample event the reference path:

1. Relatches `(EDL & 15) << 11` bytes only when the ring offset is zero, treating
   zero as one four-byte stereo entry.
2. Forms the current address from the previously latched `ESA << 8` plus the
   byte offset, with natural 16-bit wrapping.
3. Reads two little-endian signed words and arithmetic-shifts each right once to
   restore the right-aligned signed-15 FIR inputs.
4. Inserts that pair into independent eight-entry left/right histories. Tap 0
   consumes the oldest sample and tap 7 the newest.
5. Multiplies each history value by its signed coefficient and arithmetic-shifts
   by 6. The first seven products are summed then wrapped to signed 16 bits; the
   newest product is added with signed-16 clamping; bit 0 is cleared.
6. Multiplies the FIR result by signed `EVOLL`/`EVOLR`, shifts by 7 with signed
   rounding, wraps to 16 bits and clamps its addition to the post-`MVOL` dry path.
7. Independently accumulates only resolved `EON` voices in Voice-0-to-7 order,
   clamping after every post-voice-volume addition.
8. Multiplies FIR by signed `EFB`, shifts by 7 and wraps, then clamps its addition
   to the echo-send mix and clears bit 0 for the left-aligned signed-15 RAM word.
9. Writes the stereo pair unless `FLG` bit 5 is set. Reads, filtering, feedback
   calculation, history and pointer movement continue while writes are protected.
10. Advances by four bytes, wraps at the latched length, and latches the current
    `ESA` for the next sample. `FLG` bit 6 zeros only the DAC-facing pair; all dry,
    echo, feedback and ring work continues.

The public `mipsnes2_dsp_fir` helper accepts eight right-aligned signed-15 samples
from oldest to newest and eight signed coefficients. It gives tests and future
optimized kernels one explicit reference contract without exposing platform APIs.

## Verification

`tests/test_dsp.c` now covers:

- invalid FIR/API calls without partial output mutation;
- a normal eight-tap vector and first-seven signed-wrap result;
- positive and negative newest-tap clamping plus final low-bit clearing;
- separate `EON` and dry mixes using a resolved noise voice;
- one-sample-delayed `ESA`, little-endian stereo reads and `EDL=0` four-byte wrap;
- signed `EVOLL`/`EVOLR` and `EFB`, exact feedback/write values and negative paths;
- `FLG` echo-write protection without stopping FIR/pointer movement;
- `FLG.MUTE` zeroing only the DAC-facing sample while echo RAM continues changing;
- delayed `EDL` relatching, explicit 16-bit address wrap and stereo history motion;
- one-shot versus irregularly chunked DSP advancement with separate ARAM images.
- the APU clock bridge's read-only overflow preflight, proving that a copied DSP
  cannot perform a hidden first echo write through its borrowed ARAM pointer.

The existing BRR fixtures now set the hardware echo-write-disable bit while their
source bytes occupy ARAM. This preserves their intended isolation; with echo writes
enabled the real DSP is allowed to overwrite the same memory.

Local evidence for this revision consists of a clean `make -j2 test`; focused
DSP/APU/machine ASan+UBSan runs with leak detection disabled only for the managed
runtime; GCC `-fanalyzer` over `dsp.c`, `timing.c` and both focused tests; repository
checks; and diff checks. Published host/PS2/PSP workflow URLs are added to `STATUS.md` after
the commit is available. Cross-compilation does not prove audible output or real
console timing.

## Open boundaries

- Echo and register visibility is sample-granular, not the exact 32-cycle S-DSP
  pipeline. Left/right write-gate races and coefficient fetch races remain open.
- The exposed DAC-facing pair stops before any disputed analog/post-amplifier
  polarity convention. No polarity inversion is silently claimed.
- There is no PCM queue, sample-format ABI, resampling, backpressure, underrun
  policy, SPU2/RFAuds2 adapter, PSP audio backend or audible-output result.
- Real SNES and real PS2/PSP measurements remain authoritative for unresolved
  hardware timing and target performance.

Next primary milestone: add a bounded deterministic PCM buffer and a
platform-independent consumer interface while keeping platform pacing strictly
outside emulated S-DSP time.
