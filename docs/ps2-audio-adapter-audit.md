# PS2 bounded audio-adapter audit

Date: 2026-10-29
Scope: PS2-only 48 kHz staging ownership, accepted-prefix retry and diagnostics

## Boundary and ownership

`mipsnes2_ps2_audio_adapter` lives under `platform/ps2/` and borrows a fixed-capacity
stereo-frame array from its caller. It owns the valid contents of that staging array
until the configured transport accepts them. It allocates no memory and contains no
PS2SDK, RFAuds2 or SPU2 call, so the contract remains host-testable while the device
API is reviewed separately.

The shared S-DSP FIFO remains the producer at 32 kHz. A private deterministic
resampler drains only frames that emulation already produced and emits the fixed PS2
boundary rate of 48 kHz. The adapter cannot step the machine, request another sample
event, insert silence or turn device demand into emulated time.

## Pump and backpressure contract

Each pump performs one bounded action:

1. If an unaccepted tail exists, offer that tail without touching the source FIFO.
2. Otherwise, convert at most the staging capacity from 32 kHz to 48 kHz.
3. If no output is available, report starvation without calling the transport.
4. Offer the pending frames once. The callback returns the accepted prefix length.
5. Retain a zero/partial-acceptance tail in order for the next pump.

A callback result greater than the offered count is a contract error. No staged frame
is discarded in that case, so a valid later call can retry the same tail. The adapter
is intentionally serialized and non-reentrant; a real transport must copy or consume
the accepted prefix before returning.

Per-call reports expose generated, offered, accepted and still-pending frame counts.
Lifetime counters for those quantities plus calls, starvation, backpressure and
contract errors saturate at `UINT64_MAX`. Reset discards the retained tail and
diagnostics while preserving caller storage and callback configuration.

## Verification

`tests/test_ps2_audio_adapter.c` verifies invalid construction, empty-source
starvation, the exact 32-to-48 kHz interpolation sequence, accepted-prefix limits of
two/zero/three frames, ordered tail retry, no new source consumption during retry,
subsequent resampler continuity, reset/configuration preservation, oversized callback
rejection and diagnostic saturation.

Local evidence for this tree includes the focused test, clean GCC `-fanalyzer` passes
for the adapter and fixture, the full host suite, full ASan+UBSan suite and repository
consistency checks. Cross-compilation establishes only source/build portability; it
cannot establish audible output, latency, thread behavior or physical SPU2 stability.

## Open boundaries

- RFAuds2 is not linked or called. Its current revision and public API still require
  an explicit pin and review before a thin callback binding is written.
- Device-thread serialization, IOP/SPU2 startup, underrun concealment, shutdown and
  recovery remain platform-runtime work.
- FAT/Slim PS2 latency, sustained playback, backpressure and audible polarity require
  physical-hardware measurements.
- PSP retains an independent audio backend and does not use this PS2 adapter.

Next primary milestone: pin and review the current RFAuds2 revision/API, then
implement only the thin PS2 transport binding against this accepted-prefix contract.
