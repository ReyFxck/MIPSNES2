# S-DSP long PCM hash audit

Date: 2026-10-29  
Scope: deterministic long-run host regression for native S-DSP PCM, echo ARAM
and cycle-chunk independence

## Boundary and intent

The focused S-DSP tests already pin short, independently reasoned vectors for
noise, envelopes, signed voice/main volumes, ordered saturation, FIR, feedback,
mute and the PCM FIFO. This slice adds a long regression over those implemented
rules. It is designed to catch accidental output drift and hidden dependence on
the caller's advancement granularity.

The fixed hashes are implementation-regression evidence, not an independent
hardware golden recording. They do not prove exact S-DSP microphases, analog DAC
polarity, RFAuds2/SPU2 behavior or audible output on PS2/PSP.

## Fixture

`tests/test_dsp.c` constructs a synthetic, allocation-free S-DSP scene with:

- two keyed noise voices using different signed stereo volumes and direct GAIN;
- signed main and echo volumes, one voice routed through `EON`, signed feedback
  and all eight FIR coefficients;
- a deterministic 4,096-byte initial echo image at `ESA=$60`, with `EDL=2` and
  echo writes enabled;
- a 128-frame caller-owned PCM FIFO that is drained after every advance call;
- exactly 65,536 native 32 kHz stereo frames, followed by 19 retained sub-sample
  S-SMP cycles.

PCM frames are serialized explicitly as little-endian left-low, left-high,
right-low, right-high bytes before 64-bit FNV-1a hashing. The complete final
64 KiB ARAM image is hashed in address order with the same algorithm, so host
endianness and structure padding cannot affect either value.

## Chunk-independence check

The scene is executed twice from a fresh state:

1. repeated 2,048-cycle calls, each covering 64 sample events;
2. a repeating irregular schedule of 1, 31, 63, 97, 511, 1,025, 7, 2,047, 13
   and 3 cycles, including boundaries smaller than one sample event.

Both executions must match the complete PCM and ARAM hashes, frame/sample/cycle
counters, noise/envelope state, echo offset/history position, final DAC pair and
the retained 19-cycle phase. Both also require exactly 65,536 enqueued and
consumed frames with zero drops.

The pinned results are:

- PCM stream: `368899e61f053bab`;
- final ARAM: `2fa2329372f44e6c`.

## Verification and open limits

Local verification for this slice includes the focused DSP fixture, the complete
host suite, ASan+UBSan, GCC `-fanalyzer`, repository checks and diff checks. CI
links are recorded in `STATUS.md` after the published revision completes.

Exact 32-cycle S-DSP pipeline phases, register-write races, post-DAC polarity and
comparison against a sourced hardware capture remain open. Platform pacing remains
outside emulated time: neither this fixture nor the PCM consumer can advance the
machine in response to device demand.

Next primary milestone: capture the existing bounded RFAuds2 lifecycle diagnostic
on the named Android PS2 emulator with version/settings and ELF checksum.
