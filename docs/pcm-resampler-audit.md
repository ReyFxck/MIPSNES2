# Deterministic PCM resampler audit

Date: 2026-10-29
Scope: bounded native-rate conversion, signed interpolation, starvation and chunk
equivalence

## Boundary and ownership

The S-DSP remains the sole producer of native signed-16 stereo frames at 32 kHz.
`mipsnes2_pcm_resampler` is a transport-side reference utility: it drains an
already-produced `mipsnes2_pcm_buffer` into caller-owned output storage and cannot
advance the machine, change S-SMP/S-DSP clocks or call a platform device API.

Initialization accepts output rates greater than or equal to 32 kHz. This covers
the documented 48 kHz PS2 SPU2 boundary without making RFAuds2 a core dependency,
and leaves PSP/host adapters free to choose their own supported rate. The converter
allocates no memory and retains at most the current and next source frame across
calls. Every process call is bounded by the caller's output-frame capacity.

## Deterministic conversion

The phase is stored in output-rate units and advances by 32,000 after each emitted
frame. Crossing the output rate advances the source interval. For phase `p`, output
rate `R`, current sample `a` and next sample `b`, each channel is:

```text
round_nearest_away_from_zero(((R - p) * a + p * b) / R)
```

All arithmetic before the final signed-16 result uses signed 64-bit integers. At
phase zero the current source frame is emitted exactly and does not require
lookahead. A nonzero phase requires the next source frame; if it is unavailable,
the call stops without changing phase or fabricating silence. Resuming after more
input arrives produces the same sequence as a single large call.

The converter records saturating source-consumption and output-production counters.
Its reset preserves the configured output rate while clearing phase, retained frames
and diagnostics. Input FIFO overflow remains drop-newest; device underrun counting,
silence insertion, block retry and asynchronous accepted-tail ownership remain the
platform adapter's responsibility.

## Verification

`tests/test_pcm.c` covers invalid and identity-rate construction, zero-capacity
calls, exact identity output, the PS2 32-to-48 kHz interpolation pattern,
starvation/resume, signed extrema, one-frame versus one-shot chunk equivalence,
state reset, saturating counters and a 1,000-input-frame 32-to-44.1 kHz ratio run.

Local evidence for this tree includes a clean `make -j2 test`, the full GCC
ASan+UBSan suite with leak detection disabled because LeakSanitizer cannot inspect
the managed runtime, and clean GCC `-fanalyzer` passes for `pcm.c` and its focused
test. The first sanitizer run also produced one unrelated `test-smp-memory` binary
without its executable mode; restoring that generated artifact's mode let the same
binary and the full suite pass, and no source workaround was added. Console
cross-compilation and CI will establish only build portability; neither can prove
audible polarity, device latency, underrun recovery or sustained SPU2 playback.

## Open boundaries

- This integer-linear reference policy is deterministic, not a claim of optimal
  reconstruction quality. Any optimized or higher-order target path needs measured
  benefit and reference-output or explicitly revised policy tests.
- No PS2/PSP device adapter, asynchronous queue, accepted-tail retry or concealment
  policy exists yet.
- RFAuds2 remains an external PS2-only transport candidate and must be pinned and
  reviewed before runtime integration.
- Real PS2 FAT/Slim and PSP measurements remain authoritative for latency,
  throughput and audible-output decisions.

Next primary milestone: define a PS2-only 48 kHz adapter ownership contract that
retains unaccepted tails and measures underruns without allowing device demand to
advance emulated time.
