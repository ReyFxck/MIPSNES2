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

## Asynchronous pump and backpressure contract

Each pump performs one bounded action:

1. If a request is in flight, poll it without touching the source FIFO.
2. A pending poll retains the whole staging block.
3. A completed poll advances only its accepted prefix; zero/partial acceptance leaves
   an ordered tail for a later launch.
4. Without an in-flight request, launch an existing tail or convert at most the
   staging capacity from 32 kHz to 48 kHz.
5. If no output exists, report starvation without calling the transport.

Launch must copy the offered block before returning zero. Poll returns zero pending,
one completed with an accepted prefix, or a negative completed transport error. A
positive launch, an unexpected positive poll result or acceptance beyond the offered
count is a contract error. No staged frame is discarded on transport/contract error.
Reset refuses an in-flight request, preventing reuse of storage still owned by the
transport. The adapter is intentionally serialized and non-reentrant.

Per-call reports expose generated, offered, accepted and still-pending frame counts.
Lifetime counters for those quantities plus launch/poll calls, pending, starvation,
backpressure, transport and contract errors saturate at `UINT64_MAX`. A safe reset
discards the retained tail and diagnostics while preserving caller storage and
callback configuration.

## Verification

`tests/test_ps2_audio_adapter.c` verifies invalid construction, empty-source
starvation, the exact 32-to-48 kHz interpolation sequence, a pending poll,
accepted-prefix limits of two/zero/three frames, ordered tail relaunch, no new source
consumption while pending, launch/poll errors, reset safety, oversized acceptance and
diagnostic saturation. `tests/test_ps2_rfauds2_transport.c` verifies the exact thin
mapping, frame layout, 960-frame external limit and error propagation.

Local evidence for this tree includes the focused test, clean GCC `-fanalyzer` passes
for the adapter and fixture, the full host suite, full ASan+UBSan suite and repository
consistency checks. Cross-compilation establishes only source/build portability; it
cannot establish audible output, latency, thread behavior or physical SPU2 stability.

## Open boundaries

- Merged RFAuds2 `main` commit `38cd7ae09a1ad646c9009e5ff38f675ecc8ee584`
  is the exact pin after the protocol-v2/refill-lifecycle review. The optional thin
  binding exists, but runtime
  lifecycle is not connected to the PS2 entry path.
- Device-thread serialization, IOP/SPU2 startup, underrun concealment, shutdown and
  recovery remain platform-runtime work.
- FAT/Slim PS2 latency, sustained playback, backpressure and audible polarity require
  physical-hardware measurements.
- PSP retains an independent audio backend and does not use this PS2 adapter.

Next primary milestone: connect the pinned optional binding to the PS2 runtime
lifecycle with fixed storage, explicit IRX load/bind/prebuffer/start/stop order and
diagnostics.
