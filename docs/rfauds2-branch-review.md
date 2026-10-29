# RFAuds2 branch and integration review

Date: 2026-10-29
Repositories: `ReyFxck/RFAuds2` and `ReyFxck/MIPSNES2`

## Branch map

RFAuds2 currently has three visible branches:

- `main` at `f82e0967104fc96f37259e1abc124341eef6a76c`;
- `fix/async-pcm-transport` at `9c5e5b4fe48e7f1afd89c5dde12325d8cd0f6076`;
- `fix/comprehensive-hardening` at
  `3b22578484ea3f7bb1b4d4dc6aeb6d8949a15856`.

The async branch is already an ancestor of `main`: it was merged by
[RFAuds2 PR #1](https://github.com/ReyFxck/RFAuds2/pull/1). Its partial nonblocking
admission and asynchronous stats work are therefore not missing from `main`.

The comprehensive-hardening branch is the open
[RFAuds2 PR #2](https://github.com/ReyFxck/RFAuds2/pull/2), two commits ahead of
`main`. Its final head passed both its
[push CI](https://github.com/ReyFxck/RFAuds2/actions/runs/37813570998) and
[PR CI](https://github.com/ReyFxck/RFAuds2/actions/runs/37813709630). A fresh local
GCC host run, ASan+UBSan run and `-fanalyzer` pass also succeeded. These results cover
the host models and cross-build, not physical SPU2 scheduling.

## Review verdict

The hardening branch is directionally good but is not selected for merge or as the
MIPSNES2 pin yet. Useful fixes include:

- cache writeback before publishing a refilled DMA half;
- per-half pending/ready state and a dedicated silence source for missed deadlines;
- protocol handshake and public constants;
- partial-initialization cleanup, shutdown and rebind support;
- transactional start priming and guarded latency shrink;
- broader transport/lifecycle fixtures and pinned CI inputs.

The following issues remain before that branch should replace `main`:

1. `stop()` clears the low-level masks but does not synchronize with an already
   running refill iteration. That iteration can finish after a restart and mark a
   block ready in the new generation. A generation token or explicit refill-thread
   quiescence/acknowledgement is required around stop/restart.
2. `sync_missed_refills()` updates underrun and silent-frame counters without the
   ring mutex while the playback thread updates the same counters under that mutex.
   This permits lost or inconsistent diagnostic updates.
3. The transport fixture replaces `spu2_direct.c` with mocks. It therefore does not
   execute the new pending/ready/silence state machine; CI proves it cross-compiles,
   not that its interrupt transitions are correct.
4. The follow-up test commit uses translation-unit-wide `-Wno-unused-function` to
   hide two unused mock helpers. Narrow annotations or compiling the relevant
   low-level model would preserve warnings-as-errors more precisely.

Physical FAT/Slim validation remains mandatory even after deterministic state-machine
coverage is added.

## MIPSNES2 pin and binding decision

MIPSNES2 provisionally pins the already-merged RFAuds2 `main` commit
`f82e0967104fc96f37259e1abc124341eef6a76c`. It exposes the required stable
`rfauds2_submit_s16_async()` and `rfauds2_submit_poll()` behavior without depending on
the unmerged protocol-v2 additions.

The PS2 Makefile enables the external binding only with `MIPSNES2_RFAUDS2=1` and an
`RFAUDS2_ROOT` whose Git HEAD exactly matches that SHA. PS2 CI fetches and builds the
same exact commit before cross-linking MIPSNES2. No RFAuds2 implementation source is
copied into MIPSNES2; the host fixture contains only the narrow public type/function
surface needed to verify the mapping.

Next primary milestone: connect the pinned optional binding to the PS2 runtime
lifecycle with fixed storage, explicit prebuffer/start/stop order and diagnostics.
