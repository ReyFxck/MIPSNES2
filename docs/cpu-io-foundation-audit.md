# Ricoh 5A22 CPU-I/O foundation audit

Date: 2026-10-18

## Scope

This milestone adds a caller-owned, platform-neutral CPU-I/O state component behind
the existing SNES-bus route. Its implemented register subset is:

| Register | Implemented behavior |
|---|---|
| `$4200` NMITIMEN | NMI enable, H/V IRQ mode and automatic-joypad-enable storage; disabling both IRQ mode bits clears the timer IRQ latch |
| `$4207-$4208` HTIME | 9-bit compare-value storage |
| `$4209-$420A` VTIME | 9-bit compare-value storage |
| `$4210` RDNMI | VBlank flag, caller-supplied CPU version, open-bus composition and read acknowledgement |
| `$4211` TIMEUP | timer IRQ flag, open-bus composition and read acknowledgement |
| `$4212` HVBJOY | caller-supplied VBlank, HBlank and auto-joypad-busy inputs |

The component detects a rising edge of `NMITIMEN.NMI-enable AND RDNMI.flag`, including
enabling NMI while an unacknowledged VBlank flag is already set. It exposes a
single pending edge for later CPU/scheduler delivery. An explicit timer-match input
sets TIMEUP only when an H/V IRQ mode is enabled and exposes that latch as an IRQ
level.

## Deliberate limits

- No H/V counter or regional frame geometry exists yet.
- The component accepts timer-match and blank inputs; it does not schedule them.
- The documented narrow TIMEUP read-at-the-match exception is not modeled because
  exact match-cycle placement does not yet exist.
- Cartridge/expansion IRQ inputs are not combined with the timer IRQ.
- Automatic joypad polling and `$4218-$421F` data are not implemented.
- `$4201-$4206`, `$420B-$420C` and `$4213-$421F` remain on the existing fallback
  route for later I/O, math, DMA and controller milestones.
- Initialization uses the documented power-on compare value `$1FF`; a future machine
  reset contract must preserve fields that hardware leaves unchanged rather than
  re-running component initialization.

This is therefore a verified register/latch foundation, not complete CPU-I/O,
interrupt timing or Phase 4 completion.

## Verification

`tests/test_cpu_io.c` uses no ROM data. It checks initialization, invalid arguments,
unimplemented-offset rejection, 9-bit timer writes, write-only reads, meaningful-bit
masks, disabled/enabled NMI transitions, repeated transitions, RDNMI/TIMEUP read
acknowledgement, IRQ disable acknowledgement, all IRQ modes, HVBJOY state and
open-bus composition.

`tests/test_snes_bus.c` additionally verifies mirrored-bank routing to the state,
fallback routing for an unimplemented CPU-I/O register, RDNMI behavior through the
logical bus and independent bus-owned MEMSEL behavior.

Verified locally on 2026-10-18:

- `make clean && make test` — passed with the default GCC host configuration;
- `make BUILD_DIR=build/release-local CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`
  — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-local CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed on an isolated clean
  rerun;
- `python3 tools/check_repository.py` and `git diff --check` — passed.

The first sanitizer invocation was launched concurrently with the release build and
produced a zero-byte `test-contracts` file, causing `Permission denied`; it was not
counted as a test pass. The isolated clean sanitizer rebuild produced executable
artifacts and the complete suite passed. Local Clang, PS2SDK and PSPSDK are
unavailable. Real-hardware execution remains unverified.

For implementation commit `f0517d99ade3f1768fa803a8c3aa90bafab58370`, GitHub
Actions subsequently passed:

- Host CI run `39290102034`: GCC and Clang release, sanitizer and artifact jobs;
- PS2 CI run `39290101982`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps;
- PSP CI run `39290101965`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps.

No emulator or real PS2/PSP executed the code. Cross-build success proves SDK
compatibility for the current scaffold, not interrupt timing on target hardware.

## Sources

- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), `SNES
  I/O Map` and `SNES PPU Interrupts`, inspected 2026-10-18;
- [SNESdev Wiki MMIO registers](https://snes.nesdev.org/wiki/MMIO_registers),
  inspected 2026-10-18.

No emulator source implementation was consulted or copied.
