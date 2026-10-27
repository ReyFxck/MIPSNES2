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
all jump/subroutine call/return forms, software/hardware interrupt entry, a
boundary-visible ABORT foundation, RTI,
WDM, MVP/MVN and WAI/STP. Deterministic host regressions
cover widths, flags, cycles, signed branch offsets, program-bank-local PC wrapping,
Direct Page pointer wrapping, DBR selection and 24-bit bank crossing. Console entry
points remain incomplete: PS2 now displays a five-case core diagnostic and PSP
remains a build probe; neither is a usable SNES emulator. The scheduler uses integer SNES
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
No-match and ambiguous images are rejected instead of guessed. PS2 has an
explicit-path ROM adapter and a bounded CPU-only run; there is no file picker,
unofficial ExLoROM, padded/board-specific irregular mapping or enhancement hardware.
Only PPU counter registers, VRAM ports, CGRAM ports and OAM ports exist; there is no renderer or active-display access model.
The caller-owned SPC700 foundation covers all 256 opcode values atomically and a
nominal rational APU clock now advances it, the three S-SMP timers and a first
S-DSP register/sample-cadence foundation from machine master time. `$F2/$F3`,
128 register bytes, one event per 32 S-SMP cycles, eight voice-control records,
key polling/source-directory latches, exact BRR arithmetic, four-sample group
streaming, a twelve-sample ring, raw pitch/interpolation inputs and 11-bit
ADSR/GAIN/release progression are testable. A shared 15-bit noise LFSR now follows
all 32 `FLG.NCK` rates and `NON` selects it per voice without stopping BRR
transport. Gaussian output, envelope/sample multiplication, mixing, PCM output,
a full ROM runner, compatibility and performance remain.

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
the partial SNES map. Mid-instruction ABORT rollback/sampling, signal-level RESET/bus
behavior, exact dummy/internal bus sequencing, full CPU event-horizon integration and
broader independent validation remain. The first caller-owned CPU-I/O component now
covers the documented
interrupt/timer subset plus `$4202-$4206` multiplication/division inputs and
`$4214-$4217` results. It also exposes caller-owned logical controller snapshots,
`$4218-$421F` automatic-read results and a deterministic VBlank polling window.
The math unit advances in processor cycles rather than SPU,
host or master-clock output rates; exact busy-write collision behavior and
mid-instruction internal-cycle placement remain under research. A separate H/V
component advances deterministic non-interlaced NTSC/PAL master-clock state, blank
transitions, the NTSC short line, delayed NMI flag and stable-register H/V timer
matches. A shared master-clock coordinator now advances H/V state and scheduler
events from each atomic CPU-step delta with explicit same-timestamp ordering. A
caller-owned interrupt-line aggregator preserves independent timer/cartridge/
expansion IRQ sources and can transfer CPU-I/O IRQ/NMI state at atomic CPU boundaries.
Exact register-write/match windows, TIMEUP's narrow read exception, interlace,
mid-instruction event pausing, real auto-joypad phase variation, nonstandard controller
peripherals and final simultaneous-subsystem priority remain incomplete. The two
standard serial controller ports now expose deterministic `$4016/$4017` strobe,
snapshot and 16-bit shifting for four logical lines. Multitap/mouse/light-gun
protocols, output-pin wiring and hardware-faithful contention with automatic polling
remain incomplete. The programmable I/O port now covers `$4201/$4213`, caller-owned
external pin levels and a consumable bit-7 falling edge. A caller-owned PPU counter
component now implements `$2137/$213C/$213D/$213F`, WRIO/external-edge capture,
independent byte selectors and a PPU2 open-bus latch. It samples the current H/V
snapshot; exact within-instruction capture and WRIO's measured one-dot latency
difference remain incomplete. That counter-register milestone added no rendering
or PPU memory; the later VRAM write-port subset is documented in
[docs/ppu.md](docs/ppu.md).
Unofficial ExLoROM remains research-only until stronger evidence exists.
Do not start with a dynarec,
renderer or enhancement chips.

A caller-owned peek/commit CPU-operation foundation now gives all 256 opcode values
resumable sequences when the bus contract is complete. The final block adds every
`ORA/AND/EOR/ADC/SBC/CMP/CPX/CPY/BIT` addressing form through the existing shared
operand/address/pointer/data phases, plus dedicated `RTI`, `WDM`, `MVP`, `MVN`,
`WAI` and `STP` sequences. Boundary `ABORT`, `NMI` and `IRQ` entry use the same
cursor. `LEGACY_ATOMIC` is retained only for missing-write fail-stop preflight so
the historical `INVALID_BUS` accounting stays unchanged. RESET now exposes a
zero-time control descriptor followed by deterministic `00:FFFC` and `00:FFFD`
reads; the existing reset function drives that cursor to completion. The scheduler
bridge is still instruction-atomic, and signal-level RESET cycles, exact dummy
cycles, mid-instruction `ABORT`, refresh and arbitration remain open. This is a
complete opcode-and-reset-vector cursor migration, not a claim of complete hardware
cycle fidelity.

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
- [Optional XERJ first-party code/document search](docs/xerj-workflow.md)

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
