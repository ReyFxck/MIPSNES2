# MIPSNES2

**A performance-first SNES emulator built from the ground up for PlayStation 2, with PSP as a secondary target.**

**Built for the hardware, not ported to it.**

> **DO NOT TURN THIS PROJECT INTO A SNES9X PORT.**

> Accuracy is a tool, not the final objective. Compatibility and sustained full-speed operation on real PS2 and PSP hardware are the primary objectives.

SNES emulator dedicated to MIPS for PS2/PSP hardware. PS2 is the primary target;
PSP receives its own Allegrex/VFPU/GU optimization strategy. Host PC exists only
for development, testing, debugging and tooling. This is an original implementation,
not a port or wrapper around another emulator. Fix hardware behavior, not games.

## Current state

**The project has a partial 65C816 interpreter and reference bus. It cannot run a SNES
ROM.** Eighty-two opcodes now cover the foundation, the complete register-transfer
and branch families, implied single-flag status controls, accumulator/index register
increment-decrement operations, and direct,
direct-indexed, direct-indirect, absolute,
absolute-indexed and absolute-long load/store paths. Deterministic host regressions
cover widths, flags, cycles, signed branch offsets, program-bank-local PC wrapping,
Direct Page pointer wrapping, DBR selection and 24-bit bank crossing. Console entry
points remain build probes, not usable emulators. No PPU, APU, cartridge integration,
compatibility or performance claim exists.

Current milestone: Phase 2 — incremental 65C816 reference interpreter.
[STATUS.md](STATUS.md) is the canonical record of exactly which builds, tests and
workflows were actually verified. [Bootstrap audit](docs/bootstrap-audit.md) contains
commands, evidence and limitations. [ROADMAP.md](ROADMAP.md) sequences future work.

Phase 1's foundation exit gate, the complete register-transfer and branch families,
the implied single-flag status controls and accumulator/index register increment-
decrement operations are verified. The next focused milestone should implement the
immediate ORA/AND/EOR group with exact M-width, N/Z and cycle tests. Do not start with
a dynarec, renderer or enhancement chips.

## Build and test the foundation

Host requirements: a C99 compiler (GCC or Clang), GNU Make, Python 3.10+.

```sh
make
make test
./build/host/mipsnes2-host
```

Use `make BUILD_DIR=build/clang CC=clang test` for a separate Clang build.
These test the implemented CPU/bus foundation plus address/logging contracts and
repository consistency. They do not establish complete SNES behavior.

For pinned Linux x86-64 console toolchains see [GitHub setup](docs/github-setup.md).
With an installed SDK and the documented environment:

```sh
make -C platform/ps2
make -C platform/psp
```

Outputs: `platform/ps2/mipsnes2-ps2.elf` and `platform/psp/EBOOT.PBP` (plus PSP ELF/PRX).
Read [PS2](docs/ps2.md), [PSP](docs/psp.md) and [host](docs/host.md) before building.
Compilation does not establish console runtime correctness. Live CI state is never
inferred from workflow files; see STATUS.md for dated verification evidence.

## Architecture and project memory

Platform backends call a shared, platform-independent SNES core. The intended core
contains CPU, bus, cartridge, scheduler, DMA/HDMA, PPU and APU. GS/GU present frames;
SPU2/PSP audio receive emulated audio. They do not replace SNES PPU/DSP behavior.
RFAuds2 is the preferred future PS2 PCM transport behind a PS2-only adapter; it is
not a core dependency and does not control emulated audio time.
Target-specific MIPS, MMI, scratchpad, DMA, VU and VFPU paths are encouraged only
when correct, tested and measurably beneficial on their respective hardware.

- [Project constitution](PROJECT_GOALS.md)
- [Architecture and boundaries](ARCHITECTURE.md)
- [Canonical status](STATUS.md) and [roadmap](ROADMAP.md)
- [Performance evidence](PERFORMANCE.md) and [compatibility evidence](COMPATIBILITY.md)
- [Contribution rules](CONTRIBUTING.md) and [agent instructions](AGENTS.md)
- [Testing](docs/testing.md), [debugging](docs/debugging.md), [GitHub setup](docs/github-setup.md)

PRs run host and console build workflows. Successful jobs are configured to produce
commit-identified development artifacts with SHA-256 files. Manual prereleases are
gated on all three build/test workflows at the same revision; ordinary commits do
not create releases. Runtime emulation remains future work regardless of CI color.

## Contributing and license

Read AGENTS.md and PROJECT_GOALS.md before modifying code. Do not casually replace
working low-level optimizations. Record measurements and honest verification, preserve
TODOs and blockers, and keep STATUS.md current. Submit focused PRs; bootstrap is
reviewed before merging. Never upload commercial ROMs or copy emulator implementations.

Licensed under [GNU GPL-3.0](LICENSE). The original repository license is preserved.
