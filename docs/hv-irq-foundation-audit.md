# H/V Timer IRQ Foundation Audit

Date: 2026-10-19

## Scope

This milestone extends the caller-owned non-interlaced H/V timing component with
Ricoh 5A22 H/V timer-match generation. A match calls the existing CPU-I/O TIMEUP/IRQ
latch input; it does not enter the 65C816 immediately or bypass normal IRQ masking.

Implemented stable-register behavior:

- H-IRQ on every eligible scanline at `14 + HTIME * 4` master clocks after that
  line's dot 0;
- late HTIME 338/339 matches carried into the next scanline at master clocks 2/6;
- V-IRQ at 1374 master clocks after the previous scanline's dot 0, normally H=2.5
  or master clock 10 of the selected V line;
- V-IRQ at master clock 14 when the selected line follows the 1360-clock NTSC short
  line;
- HV-IRQ using the H formula only on VTIME, with HTIME=0 using the V-IRQ formula;
- HTIME values 340-511 and regional out-of-range VTIME values do not match;
- HTIME 153 is suppressed on the NTSC short line and the last line of a frame;
- an IRQ mode enabled exactly at an otherwise pending match boundary can assert;
- at most one internal timer match is consumed per scanline.

All addresses and timing remain SNES logical/master-clock state. The component owns
no heap, platform API, host clock or target pointer.

## Deliberate limits

The full IRQ task remains in progress:

- Interlace and the PAL interlaced long line are absent.
- Timer-register/mode changes are observed at explicit H/V advance boundaries.
  Exact effects of writes inside an atomic CPU instruction cannot be represented
  until the CPU exposes bus-cycle placement and the timing components share one
  event horizon.
- Reprogramming and acknowledging multiple potential matches within one scanline is
  not claimed; the stable-register path consumes at most one timer event per line.
- TIMEUP's narrow read-at-the-match exception remains unimplemented.
- Cartridge/expansion IRQ lines are not combined with the internal timer IRQ.
- CPU IRQ sampling still occurs at the interpreter's atomic instruction boundary;
  the H/V component is not yet driven by `mipsnes2_scheduler.now`.
- WRAM refresh, DMA/HDMA arbitration and PPU events are absent.

No approximation above is used to claim complete IRQ timing or Phase 4 completion.

## Tests

`tests/test_hv_timing.c` now verifies:

- every HTIME value 0-339 against the documented master-clock formula;
- HTIME 338/339 spill into the following scanline;
- ordinary V-IRQ and HV-IRQ, including the HTIME=0 special case;
- VTIME 0 after frame rollover and VTIME 241 after the NTSC short line;
- suppression at HTIME 153 on the short and final scanlines;
- out-of-range H/V values;
- enabling H-IRQ at its exact match boundary;
- TIMEUP assertion/acknowledgement and no duplicate same-line assertion;
- one-shot/chunked determinism with H-IRQ enabled.

Verified locally on 2026-10-19 with GCC 13.3.0:

- `make clean && make test` — passed;
- `make BUILD_DIR=build/release-irq CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`
  — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-irq CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed;
- `python3 tools/check_repository.py` and `git diff --check` — passed;
- `sha256sum LICENSE` remained
  `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

LeakSanitizer is disabled locally because this environment cannot inspect the
required `/proc` state. Local Clang, PS2SDK and PSPSDK are unavailable.

For implementation commit `9936fd24a2a8cfdd736d4fe7df707c11f0103e55`, GitHub
Actions subsequently passed:

- Host CI run `39340295232`: GCC and Clang release, sanitizer and artifact jobs;
- PS2 CI run `39340295322`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps;
- PSP CI run `39340295286`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps.

No emulator or real PS2/PSP executed the code. Cross-compilation proves the shared
source remains accepted by the pinned scaffold toolchains, not interrupt timing or
runtime correctness on target hardware.

## Sources

- [Super Famicom Development Wiki: Timing](https://wiki.superfamicom.org/timing),
  `Interrupts`, inspected 2026-10-19. It records the internal-timer mode table, the
  exact `14 + H*4` formula, the HTIME=0/V-IRQ 1374-clock formula, out-of-range and
  short/final-line exceptions, and the enable-at-match observation.
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), `SNES
  Timing H/V Events`, inspected 2026-10-19, used to cross-check the H/V/HV event
  positions and CPU-counter limits.

These are independent hardware research/community documentation, not emulator
implementations. No emulator source or commercial ROM was consulted or copied.
