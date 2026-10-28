# Deterministic PCM buffer audit

Date: 2026-10-28
Scope: native S-DSP output format, bounded caller-owned FIFO, overflow/empty
semantics and machine-level attachment

## Boundary and ownership

This slice starts at the existing mute-gated `dac_sample`; it does not change
voice, echo, FIR or final-mix arithmetic. Each completed 32-S-SMP-cycle sample
event offers exactly one stereo frame to an optional platform-neutral FIFO.
The 1.024 MHz nominal S-SMP clock divided by the existing 32-cycle cadence gives
the explicit 32,000-frame-per-second native rate.

The API uses typed frames with signed 16-bit `left` and `right` fields in host
endianness. This is a core-to-adapter contract, not a serialized byte ABI.
Adapters must choose their device byte layout, rate conversion and cache/thread
coordination without changing S-DSP time.

The caller supplies both the `mipsnes2_pcm_buffer` object and its fixed-capacity
frame storage. The common core does not allocate memory. Machine configuration
can borrow the storage for its lifetime, resets FIFO indices and diagnostics on
power-on, and reattaches the same buffer after the DSP state is reinitialized.
PCM machine configuration requires ARAM because the machine advances the DSP
only through the configured APU clock path.

## Full and empty policy

The producer never blocks and device pacing never stalls emulated time. When the
FIFO is full, the new frame is discarded, older queued frames remain ordered,
`dropped_frames` increments saturating at `UINT64_MAX`, and the S-DSP continues.
Successful enqueue and read counters also saturate rather than wrap.

An empty read returns zero frames and does not fabricate silence or mutate an
underrun counter. Device adapters can choose silence/concealment and collect
target-specific underrun measurements. The FIFO is not internally synchronized;
an adapter using another thread or interrupt must serialize access outside the
emulation core.

## Verification

`tests/test_pcm.c` covers invalid construction, empty reads, full-buffer
drop-newest behavior, FIFO wrap, bulk read order, available/free counts, reset
without losing caller storage, and saturating diagnostics.

`tests/test_dsp.c` proves that:

- attachment forces one enqueue attempt for every sample event, including idle
  muted samples that would otherwise take the DSP bulk shortcut;
- a non-zero echo/final-mix vector reaches the FIFO unchanged;
- a full FIFO records every dropped frame without changing `sample_count`;
- detach stops delivery; and
- one-shot and irregularly chunked advancement produce identical queued frames
  and overflow counts.

`tests/test_machine.c` rejects partial PCM configuration, exercises a bounded
FIFO through normal machine/APU advancement and verifies that power-on empties
and reattaches it. Host tests remain offline evidence: they do not prove an
SPU2/RFAuds2 path, PSP device delivery, audible polarity or target latency.

## Open boundaries

- Exact within-32-cycle DSP microphases and post-DAC analog polarity remain open.
- No resampler, platform audio adapter, device pacing, threaded handoff or
  sustained target playback is implemented.
- Core overflow semantics are fixed, but target underrun concealment and latency
  thresholds require PS2-first measurements.
- Real SNES and real PS2/PSP measurements remain authoritative for unresolved
  hardware timing and target performance.

Next primary milestone: define a deterministic resampling contract and bounded
rate-conversion state without allowing device consumption to drive emulated time.
