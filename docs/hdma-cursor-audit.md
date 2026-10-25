# HDMA operation cursor audit

Date: 2026-10-25

## Scope

The shared HDMA table engine now exposes a caller-owned operation cursor.
Frame initialization and each requested visible line yield overhead, table-read
and A/B transfer operations one at a time. The caller can advance a scheduler
to the operation endpoint before supplying a table byte or performing a transfer.
The older atomic functions run the same cursor to completion, so the logical
HDMA rules have one implementation.

An active cursor rejects a second frame/line start. Repeated peeks return the
same operation, and a commit checks the operation and a snapshot of channel
registers and logical HDMA state. This detects external edits before committing
the cursor's own pointer/counter changes. The caller owns the A/B side effect;
it must perform it once at the admitted endpoint and then commit. The cursor
cannot roll back an external side effect if that caller violates the snapshot
contract. The scheduler integration must keep this operation pending across a
horizon or event-budget return.

## Timing plan and limits

The cursor emits an approximate 18-master-clock global overhead when an enabled
channel participates. Frame initialization costs eight clocks per direct
channel or 24 per indirect channel, including its descriptor read. A visible
line costs eight clocks per active channel plus eight per data byte and 16 if
an indirect pointer is reloaded. A line-end descriptor read carries no
additional clocks beyond that channel charge. This follows the HDMA section of
[Anomie's Timing Doc](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt),
which explicitly marks some phase details approximate. The cursor does not
advance time, arbitrate CPU/general DMA, model electrical read/write subphases,
or determine when a scanline request fires. Refresh must be handled by the
timing bridge. The existing zero-descriptor indirect edge remains open.

Only hardware documentation and independently published hardware research were
consulted; no source code from another emulator was used or indexed.

## Verification

- [x] The direct and indirect atomic fixtures still pass through the cursor.
- [x] The granular frame fixture checks 18 + 8 direct + 24 indirect clocks,
  table order, pointer advancement only after commit and final report.
- [x] Repeated peek is stable; altered channel state is rejected without cursor
  advancement; an active cursor refuses reinitialization.
- [x] The first granular visible-line transfer is exposed once with the expected
  A/B addresses and advances its direct pointer only after commit.
- [x] A direct/indirect line costs 74 clocks in the synthetic fixture, with
  descriptor reads committed at zero additional cost.
- [x] `make -j2 BUILD_DIR=build/hdma-cursor test` and
  `python3 tools/check_repository.py` pass on host C99 `cc` (2026-10-25).

The block is complete when the host fixtures and PS2/PSP/host CI builds pass.
Timed machine integration and actual PS2/PSP execution are separate open work.
