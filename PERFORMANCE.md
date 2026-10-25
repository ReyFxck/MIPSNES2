# Performance evidence

No emulator performance has been measured. Foundation build/test durations are not
SNES emulation benchmarks. Every value below is TBD. No full-speed claim exists.

Approximately 60 Hz allows about 16.67 ms/frame; region-specific timing and actual
emulated frame cadence must be recorded. FPS alone is insufficient: record frame
time distribution, emulation speed, frameskip, audio underruns and sustained duration.

## Real PlayStation 2

| Test | Before | After | Difference | Notes |
|---|---:|---:|---:|---|
| Base SNES workload | TBD | TBD | TBD | No implementation or hardware benchmark |

## Real PSP

| Test | Before | After | Difference | Notes |
|---|---:|---:|---:|---|
| Base SNES workload | TBD | TBD | TBD | No implementation or hardware benchmark |

## Frame budget attribution

Costs must use mutually exclusive attribution or clearly identify nested/inclusive
measurements. In particular, bus time within CPU time must not be double-counted.

| Subsystem | PS2 | PSP | Notes |
|---|---:|---:|---|
| 65C816 | TBD | TBD | |
| Bus | TBD | TBD | |
| PPU | TBD | TBD | OBJ selection scans up to 128 records at each visible-line HBlank (224 NTSC or 239 overscan events/frame); optional OBJ composition decodes up to 34 full 4bpp tiles per line. PS2 and PSP cost unmeasured. |

Each enabled Mode 0 BG reference path currently fetches a tilemap entry and
2bpp row per pixel at a scanline event; the mixer then scans the four layer
pixels and OBJ. The color pass uses a 32-entry brightness table and a
256-pixel CGRAM lookup. Measure on PS2 before replacing these reference
paths with cached-tile paths; no console throughput claim is made.
Mode 1 follows the same per-pixel map lookup pattern, fetching four
bitplanes for BG1/BG2 and two for BG3. Its PS2 and PSP cost is unmeasured.
The main-screen window pass currently evaluates up to five layer predicates
at each of 256 pixels. Its cost and cache behavior remain unmeasured on PS2
and PSP.
The sub-screen now adds another window pass, priority mix and CGRAM lookup
when VRAM and CGRAM are present. Combined per-line cost remains unmeasured
on either target; no frame-rate inference follows from cross-build success.
The reference color-math pass currently performs three channel operations and
brightness divisions per pixel. PS2 and PSP cost remains unmeasured.
The PS2 ROM path now reserves a 256×256 RGBA32 frame (256 KiB) and expands
captured BGR555 lines on the EE. Its cost and cache/DMA behavior are unmeasured.
The optional PS2 GS path now sends one 256 KiB RGBA32 buffer over GIF DMA
after a bounded run. No throughput, transfer latency, cache-stall or frame
budget measurement exists; it is not a continuous renderer.
The color-window predicate adds one evaluation per pixel before arithmetic;
the reference path has no target timing measurement.
| DMA/HDMA | TBD | TBD | Shared bridge snapshots CPU/channel state for each slot; HDMA events begin after a nonzero `$420C` write. PS2/PSP cost remains unmeasured. |
| SPC700 | TBD | TBD | |
| DSP | TBD | TBD | |
| Scheduler | TBD | TBD | |
| Video upload | TBD | TBD | |
| Audio output | TBD | TBD | |
| Miscellaneous | TBD | TBD | |
| Peak memory | TBD | TBD | |

## Development environments

Host: no emulation benchmark. PCSX2: not tested. PPSSPP: not tested.
Keep these measurements separate from real-console tables. They cannot verify full
speed on hardware. Record emulator/version/settings if used later.

## Measurement protocol

Record exact revision, compiler/toolchain, flags, console model, clock/configuration,
ROM/test identity and region (hash is fine; never distribute commercial ROMs), input
replay, warm-up, run length, sample count, timer/calibration, min/median/p95/max frame
time, audio state, frameskip and memory. Compare the same workload before/after;
rerun correctness regressions. A gain on PS2 says nothing about PSP without evidence.

## Optimization log

No attempts retained or rejected yet. See [docs/optimization.md](docs/optimization.md)
for the mandatory per-attempt template and reference/optimized validation policy.
