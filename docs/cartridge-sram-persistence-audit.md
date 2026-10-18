# SRAM persistence handoff audit

Date: 2026-10-18

## Scope

This milestone adds change tracking and a race-resistant save handoff to the existing
caller-owned SRAM buffer. It does not perform filesystem I/O. Platform code can read
a live, const SRAM view containing:

- buffer pointer and mapper-visible byte count;
- whether any value-changing write is unsaved;
- a monotonically advancing nonzero revision token for changed data.

The platform records the revision it is saving and acknowledges that exact token only
after durable output succeeds. If emulation changes SRAM while the save is in flight,
the revision no longer matches, acknowledgement fails and dirty state remains set.
This avoids clearing a newer write because an older asynchronous save completed.

## Behavior contract

- Initialization begins at revision zero and clean state. The caller may populate the
  SRAM buffer from persistent storage before cartridge construction.
- A decoded SRAM write advances the revision only when the byte value actually
  changes. Aliases observe the same storage and same revision.
- ROM writes and unmapped writes never dirty SRAM.
- Revision wrap skips zero so zero remains the initial revision.
- A stale acknowledgement returns `false` and preserves dirty state.
- A matching acknowledgement clears dirty state without changing data or revision.
- A no-SRAM cartridge exposes a null, zero-length, clean view at revision zero.
- The view is live, not a copied snapshot. A platform must copy it or otherwise
  coordinate emulation while writing persistent storage.

The core never chooses paths, file formats, atomic-replace policy, save timing or
platform APIs. PS2, PSP and host persistence backends remain separate future work.

## Verification

Generated cases in `tests/test_cartridge.c` cover initial/no-SRAM views, null
arguments, ROM and unmapped writes, a first changed SRAM byte, same-value alias
writes, a newer write invalidating an older token, successful acknowledgement,
same-value writes after acknowledgement and deterministic revision wrap behavior.

Verified locally from `feature/cartridge-sram-persistence` on 2026-10-18:

- `make clean && make test` — passed with the default GCC host configuration;
- `make BUILD_DIR=build/release-local CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`
  — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-local CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed;
- `python3 tools/check_repository.py` and `git diff --check` — passed.

Local Clang, PS2SDK and PSPSDK are unavailable. GitHub Host/PS2/PSP CI evidence will
be recorded only after it actually passes. No real PS2/PSP storage device or
emulator was tested.

## Remaining work

- Platform file location and naming policy.
- Initial save-file loading, exact-size validation and error reporting.
- Crash-safe temporary write plus atomic replacement where supported.
- Save scheduling/debounce appropriate to each console.
- Real memory-card/storage validation on PS2 and PSP.
