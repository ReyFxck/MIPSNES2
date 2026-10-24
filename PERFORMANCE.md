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
| DMA/HDMA | TBD | TBD | |
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
