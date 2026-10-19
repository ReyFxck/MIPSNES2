# MIPSNES2

**A performance-first SNES emulator built from the ground up for PlayStation 2, with PSP as a secondary target.**

**Built for the hardware, not ported to it.**

> **THIS PROJECT DOES NOT USE ANY OTHER EMULATOR FOR REFERENCES, IT WILL ALSO ONLY USE RELIABLE SITES TO HELP CREATE EACH CHIP AND PROCESSOR. IT IS NOT ALLOWED TO PORT THIS CORE WITHOUT MY AUTHORIZATION.**

> Accuracy is a tool, not the final objective. Compatibility and sustained full-speed operation on real PS2 and PSP hardware are the primary objectives.

> **DO NOT TURN THIS PROJECT INTO A SNES9X PORT.**

SNES emulator dedicated to MIPS for PS2/PSP hardware. PS2 is the primary target;
PSP receives its own Allegrex/VFPU/GU optimization strategy. Host PC exists only
for development, testing, debugging and tooling. This is an original implementation,
not a port or wrapper around another emulator. Fix hardware behavior, not games.

## Current state

**The project has a 256-opcode reference 65C816 interpreter, a partial real SNES
logical bus, base LoROM/HiROM mapping, conservative internal-header selection and a
deterministic scheduler/timing foundation with a partial non-interlaced H/V model.
It cannot run a SNES ROM as a machine.**
All opcode values now cover the foundation, the complete register-
transfer and branch families, implied single-flag status controls, accumulator/index
register increment-decrement operations, complete ORA/AND/EOR/CMP address families,
a stack-relative foundation, a logic/compare foundation, and complete LDA/STA address
families alongside the implemented index-register load/store paths, complete
ADC/SBC addressing families including packed-BCD decimal operation, all shifts and
rotates, memory INC/DEC, TSB/TRB and STZ, the complete stack push/pull family and
all jump/subroutine call/return forms, software/hardware interrupt entry, RTI,
WDM, MVP/MVN and WAI/STP. Deterministic host regressions
cover widths, flags, cycles, signed branch offsets, program-bank-local PC wrapping,
Direct Page pointer wrapping, DBR selection and 24-bit bank crossing. Console entry
points remain build probes, not usable emulators. The scheduler uses integer SNES
master-clock timestamps, deterministic event ordering and an exact rational clock
converter. The SNES bus maps caller-owned 128 KiB WRAM and its low-bank mirrors,
routes defined PPU/APU/CPU-I/O/DMA/cartridge windows, owns the WRAM ports and `$420D`
FastROM selection, and charges each meaningful CPU access at its 5A22 6/8/12-clock
class. Aggregate CPU cycles not represented by a callback are currently charged as
6-clock internal cycles. Exact dummy/internal cycle placement, WRAM refresh,
DMA/HDMA arbitration and complete open-bus domains remain. The cartridge foundation
accepts caller-owned ROM/SRAM buffers and implements base LoROM/HiROM plus documented
Mode 25/35 ExHiROM windows. Power-of-two images and unambiguous larger-region/
smaller-tail layouts are supported within each mapper's verified size limit. A separate
image view validates 32 KiB-granular payloads, explicitly handles
optional 512-byte copier prefixes, calculates the mirrored whole-image checksum and
can select one LoROM/HiROM/ExHiROM candidate only when independent header evidence
agrees. A platform-neutral, allocation-free preparation contract then reports required
SRAM and constructs the executable mapper over caller-owned buffers. Value-changing
SRAM writes carry a revisioned dirty-state handoff so platform save backends cannot
acknowledge stale asynchronous data.
No-match and ambiguous images are rejected instead of guessed. There is no file loader,
unofficial ExLoROM, padded/board-specific irregular mapping or enhancement hardware.
No PPU, APU, ROM runner, compatibility or performance claim exists.

Current milestone: Phase 4 — Ricoh 5A22 interrupt/timer foundation.
[STATUS.md](STATUS.md) is the canonical record of exactly which builds, tests and
workflows were actually verified. [Bootstrap audit](docs/bootstrap-audit.md) contains
commands, evidence and limitations. [ROADMAP.md](ROADMAP.md) sequences future work.

Phase 1's foundation exit gate, the complete register-transfer and branch families,
the implied single-flag status controls, accumulator/index register increment-
decrement operations and the complete BIT/CPX/CPY/ORA/AND/EOR/CMP families are
verified. The LDA/STA families are also complete, including both stack-relative modes.
All ADC/SBC addressing forms are verified in binary and decimal modes. Decimal
coverage includes exhaustive valid 8-bit packed-BCD inputs, targeted 16-bit and
invalid-digit cases, plus an independent SingleStepTests vector audit. The complete
shift/rotate families and the current memory read-modify-write group are also
verified. Opcode dispatch and the generic scheduler/event foundation are complete.
Meaningful CPU bus accesses now receive address-dependent Ricoh 5A22 timing through
the partial SNES map. ABORT, signal-level RESET/bus behavior, exact dummy/internal
bus sequencing, full CPU event-horizon integration and broader independent validation
remain. The first caller-owned CPU-I/O register/latch component now covers the
documented `$4200`, `$4207-$420A` and `$4210-$4212` foundation. A separate H/V
component advances deterministic non-interlaced NTSC/PAL master-clock state, blank
transitions, the NTSC short line, delayed NMI flag and stable-register H/V timer
matches. Exact register-write/match windows, TIMEUP's narrow read exception,
interlace and connection to the central scheduler/CPU boundary remain incomplete.
Unofficial ExLoROM remains research-only until stronger evidence exists.
Do not start with a dynarec,
renderer or enhancement chips.

## Build and test the foundation

Host requirements: a C99 compiler (GCC or Clang), GNU Make, Python 3.10+.

```sh
make
make test
./build/host/mipsnes2-host
```

Use `make BUILD_DIR=build/clang CC=clang test` for a separate Clang build.
These test the implemented CPU/bus/scheduler foundation plus address/logging contracts and
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
- [Curated hardware, test and SDK references](docs/references.md)

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
