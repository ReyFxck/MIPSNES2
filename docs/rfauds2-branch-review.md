# RFAuds2 branch and integration review

Date: 2026-10-29
Repositories: `ReyFxck/RFAuds2` and `ReyFxck/MIPSNES2`

## Final branch map

The relevant RFAuds2 history is now:

- `fix/async-pcm-transport` at
  `9c5e5b4fe48e7f1afd89c5dde12325d8cd0f6076`, merged by
  [RFAuds2 PR #1](https://github.com/ReyFxck/RFAuds2/pull/1);
- final `fix/comprehensive-hardening` head
  `7e7a0d0f0392f97330b680e754f9e587575ee57a`;
- `main` merge commit `38cd7ae09a1ad646c9009e5ff38f675ecc8ee584`, produced by
  [RFAuds2 PR #2](https://github.com/ReyFxck/RFAuds2/pull/2).

The final hardening head passed its
[push CI](https://github.com/ReyFxck/RFAuds2/actions/runs/40960415541) and
[PR CI](https://github.com/ReyFxck/RFAuds2/actions/runs/40960426182). Local GCC host
tests, ASan+UBSan and `-fanalyzer` also passed. The resulting merge commit passed
[main CI](https://github.com/ReyFxck/RFAuds2/actions/runs/40960573209). These establish
deterministic host behavior and EE/IOP cross-build portability; they do not establish
physical SPU2 timing.

## Review findings and resolution

The original branch review found four issues before merge. The final branch head
resolves each one:

1. A playback lifecycle mutex now serializes the refill thread with `start()`,
   `pause()`, `resume()`, `flush()`, `stop()` and `shutdown()`. An old refill cannot
   cross a later stop/restart boundary and publish into the new playback generation.
2. Missed-refill telemetry is synchronized under the ring mutex used by the playback
   counters. Counter additions saturate instead of wrapping during a long run.
3. The pending/ready/silence transition logic was extracted from SPU2 register access
   into `refill_state.c`, which is both linked into the IRX and executed by deterministic
   host tests covering ping-pong, deadline misses, silence recovery, stop/restart and
   saturation.
4. The translation-unit-wide `-Wno-unused-function` suppression and its unused mock
   helpers were removed. Host tests retain `-Werror` without that exception.

The merged code also retains the earlier useful changes: cache writeback before DMA
half publication, dedicated silence instead of stale PCM replay, protocol-v2
handshake, shutdown/rebind cleanup, transactional start priming, guarded latency
shrink and pinned cross-build inputs.

Physical FAT/Slim validation and long-duration lifecycle/underrun stress remain
mandatory before production-readiness claims.

## MIPSNES2 pin and binding decision

MIPSNES2 pins the merged RFAuds2 `main` commit
`38cd7ae09a1ad646c9009e5ff38f675ecc8ee584` exactly. This is RFAuds2
`0.1.0-alpha.2` with the protocol-v2 and refill-lifecycle hardening reviewed above.

The PS2 Makefile enables the external binding only with `MIPSNES2_RFAUDS2=1` and an
`RFAUDS2_ROOT` whose Git HEAD exactly matches that SHA. PS2 CI fetches and builds the
same commit before cross-linking MIPSNES2. No RFAuds2 implementation source is copied
into MIPSNES2; the host fixture models only the narrow public type/function surface
needed to verify the binding.

Next primary milestone: connect the pinned optional binding to the PS2 runtime
lifecycle with fixed storage, explicit IRX load/bind/prebuffer/start/stop order and
on-screen diagnostics.
