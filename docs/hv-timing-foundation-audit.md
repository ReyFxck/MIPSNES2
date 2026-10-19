# H/V Timing Foundation Audit

Date: 2026-10-19

## Scope

This milestone adds a caller-owned, platform-independent non-interlaced SNES H/V
timing component in `core/scheduler/hv_timing.*`. It advances only from explicit
SNES master-clock input; host wall time, PS2/PSP APIs and native pointers are absent.

Implemented behavior:

- NTSC 262-line and PAL 312-line non-interlaced frame geometry;
- 1364-master-clock normal scanlines;
- the 1360-master-clock NTSC field-1 line 240;
- field toggling at H=1/V=0, represented at master clock 4;
- HBlank clear at H=1 and set at H=274;
- VBlank start at line 225 or line 240 when overscan is selected;
- VBlank end and automatic RDNMI flag clear at H=0/V=0;
- RDNMI/NMI flag assertion at H=0.5, two master clocks after VBlank starts;
- deterministic elapsed-clock/frame/H/V/field state across one-shot and chunked
  advances;
- direct handoff of HBlank, VBlank and NMI-flag signals to the caller-owned 5A22
  CPU-I/O state.

The earlier CPU-I/O contract is refined so VBlank status and the RDNMI/NMI flag are
separate signals. This represents the documented two-master-clock gap rather than
asserting both at the same instant. Enabling NMI while an already-set RDNMI flag is
active still produces the existing gated edge.

## Deliberate limits

This is not a PPU, renderer or complete Phase 4 timing implementation.

- Interlace is not represented; the PAL field-1 long scanline is therefore absent.
- H is stored as a master-clock phase, not exposed as the latched `$213C` PPU-dot
  counter. Normal-line long-dot placement is not yet modeled; only the aggregate
  line length and events before those dots are used.
- Overscan is fixed when the timing state is initialized. No SETINI register or
  mid-frame mode change exists.
- H/V timer compare generation was absent from this milestone and was added by the
  later stable-register foundation documented in
  [H/V timer IRQ audit](hv-irq-foundation-audit.md). Exact within-instruction writes,
  TIMEUP's narrow read-at-match case, automatic joypad timing, WRAM refresh, HDMA and
  PPU rendering events remain absent.
- The component is not yet driven from `mipsnes2_scheduler.now` or the CPU timing
  bridge. A future machine owner must advance it from the same master-clock delta
  and define simultaneous-event ordering.
- Initialization anchors deterministic state at H=0/V=0/F=0 with HBlank active;
  it is an emulator state contract, not a claim about every power-up latch value.

These limits keep the canonical H/V task in progress.

## Tests

`tests/test_hv_timing.c` uses no ROM or emulator-derived fixture. It verifies:

- invalid arguments and overflow rejection;
- HBlank boundaries and scanline rollover;
- VBlank start/end and the delayed RDNMI/NMI flag;
- NMI edge and RDNMI acknowledgement behavior;
- 224-line and overscan VBlank starts;
- NTSC field toggling, short line and exact two-frame total of 714732 master clocks;
- PAL 312-line/425568-master-clock non-interlaced frames;
- identical timing and CPU-I/O state after one-shot versus varied chunk advances.

Verified locally on 2026-10-19:

- `make clean && make test` — passed with the default host compiler;
- `make BUILD_DIR=build/release-hv CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`
  — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-hv CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed after the complete
  target set was built; a second `make ... test` invocation confirmed every test
  executable under ASan+UBSan.
- `python3 tools/check_repository.py` — passed as part of all three suites.

LeakSanitizer is disabled locally because this environment cannot inspect the
required `/proc` state. Local Clang, PS2SDK and PSPSDK are unavailable.

For implementation commit `6ab26ca22c4d4ec838f513702b490a681196d610`, GitHub
Actions subsequently passed:

- Host CI run `39303643924`: GCC and Clang release, sanitizer and artifact jobs;
- PS2 CI run `39303643805`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps;
- PSP CI run `39303643856`: pinned-toolchain scaffold cross-build, artifact and
  SHA-256 steps.

No console or emulator runtime has executed the code. Cross-compilation proves the
new shared source remains accepted by the scaffold toolchains, not timing correctness
on real PS2/PSP hardware.

## Sources

- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), `SNES
  Timing H/V Counters`, `Long and Short Scanlines`, and `SNES Timing H/V Events`,
  inspected 2026-10-19. The independent hardware-research document specifies the
  1364/1360 master-clock scanlines, NTSC/PAL line totals, short-line condition,
  blank transitions, field toggle and delayed NMI flag used here.
- [SNESdev Wiki timing](https://snes.nesdev.org/wiki/Timing), inspected 2026-10-19,
  used as a community cross-check for regional master-clock/frame organization.

No emulator source implementation or commercial ROM was consulted or copied.
