# S-DSP voice-control and BRR foundation audit

Date: 2026-10-27

## Scope

This change adds the first stateful voice-control slice to the portable S-DSP
core. It does not generate PCM. The implemented boundary is:

- eight independently inspectable voice records;
- caller-owned 64 KiB ARAM attachment through the existing S-SMP map;
- 16 kHz `KON`/`KOFF` polling on alternating 32 kHz sample events;
- last-write-wins `KON` capture before the next poll;
- `DIR` plus `SRCN * 4` source-directory lookup with 16-bit wrapping;
- start/loop address latching, per-voice `ENDX` clear and `ENVX`/`OUTX` zeroing
  at key-on;
- five zero-sample preparation events before the voice becomes ready for the
  future sample pipeline;
- `KOFF` release-state entry and `FLG.RES` return to key-on-waiting state; and
- a pure nine-byte BRR block decoder producing sixteen signed 15-bit samples.

`ACTIVE` means the key-on preparation delay is complete. It does not mean that
pitch, interpolation, envelopes, volume, mixing or an output buffer exists.

## Evidence and decisions

The manufacturer [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual)
defines the eight voice bits in `KON`/`KOFF`, the two-sample minimum spacing,
`FLG.RES`, `DIR`, the four-byte little-endian source-directory entry and the
nine-byte/16-nibble BRR block. The directory address is implemented as
`(DIR << 8) + (SRCN << 2)` in the S-DSP's 16-bit ARAM address domain.

[Anomie's S-DSP hardware research, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
documents alternating-sample key polling, the five zero output samples after
key-on, and the exact BRR predictor arithmetic. The current model fixes the
otherwise hardware-dependent initial polling phase so the first sample event is
a polling event. Exact operations inside the 32-step DSP loop remain open.

The BRR decoder follows the documented order: sign-extend each high nibble then
low nibble, apply shifts 0-12 or the special 13-15 result, apply filter 0-3,
clamp to signed 16 bits, then truncate/wrap to signed 15 bits. C helpers define
arithmetic right shift and signed wrapping explicitly, avoiding dependence on
implementation-defined negative shifts or undefined signed left shifts.

No emulator implementation was copied. The code was written against the
manufacturer description and independent hardware equations.

## Executable evidence

`tests/test_dsp.c` covers:

- invalid BRR arguments without output mutation;
- high-nibble-first order and shifts 0, 1 and 13;
- all four filters with exact expected samples;
- signed-16 clamp followed by signed-15 wrap;
- loop/end header flags and returned predictor history;
- ARAM attachment boundaries and `DIR`/`SRCN` address decoding;
- key-on register cleanup, five-event delay and active state;
- alternating-event `KOFF`, last-write-wins `KON`, soft reset; and
- identical voice state after one-shot and irregularly chunked clock advance.

The existing S-SMP, APU-clock and machine tests exercise automatic ARAM
attachment and the same structure-copy transaction path used by timing checks.

## Deliberate limits

This foundation does not yet connect decoded blocks to voices. It has no BRR
ring, group scheduling, block-end/loop traversal, pitch counter, Gaussian
interpolation, ADSR/GAIN, release decay, noise, pitch modulation, voice/main
volumes, echo/FIR, PCM buffering or device output. `KOFF` records release state
but cannot decay an envelope that is not implemented. Register visibility is at
sample granularity rather than exact S-DSP microsteps.

The next coherent block is the per-voice BRR stream/ring plus pitch progression
and interpolation input path, followed by envelope and mixer work.
