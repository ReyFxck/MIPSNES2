# Architecture

Status: foundation contracts only, not a working emulator. Read [STATUS.md](STATUS.md).

## Dependency direction and ownership

`platform/{ps2,psp,host} -> core/` is the only platform dependency direction.
The shared core imports standard C headers and its own headers, never PS2SDK,
PSPSDK, GS/GU, SPU2, controller, filesystem, DMA or host-window APIs.

| Area | Responsibility | Bootstrap implementation |
|---|---|---|
| core/common | Logical address types, metadata, diagnostic sink contract | Address helpers and logging utility |
| core/cpu/65c816 | Registers, interpreter, logical bus accesses, interrupts | Directory only |
| core/bus | WRAM/ROM/SRAM/MMIO/mirrors/open bus | Read/write callback typedefs only |
| core/cartridge | Validated metadata, mappings, SRAM data | Directory only |
| core/ppu | State/registers, rendering and timing, not presentation | Directory only |
| core/apu/spc700 | SPC700 execution/state | Directory only |
| core/apu/dsp | Emulated SNES DSP and sample generation | Directory only |
| core/dma | SNES DMA/HDMA, bus ownership and timing | Directory only |
| core/scheduler | Deterministic emulated time and ordered events | Directory only |
| core/enhancement | Future chip state/bus/timing integration | Directory only |
| core/debug | Optional tracing and introspection | Directory only |
| platform/ps2 | EE lifecycle, I/O, GS presentation, final SPU2 output | Build probe only |
| platform/psp | Allegrex lifecycle, GU presentation, final audio output | Build probe only |
| platform/host | Headless tests/tools, never product architecture | Foundation smoke executable |

Directories without source contain an explicit scope README, not stub functions.
No reset, execute-frame, render or audio API pretends to work. There is no machine
instance or ROM runner yet. The bus callback contract is provisional: its presence
does not implement mappings, timing or a CPU. No per-access function-pointer design
has been benchmarked or selected for final hot paths.

## Memory and interfaces

C99 is the starting language; GNU Make keeps the foundation small. Use fixed-width
integers and caller-owned state. Logical address helpers explicitly mask to 24 bits;
they do not decide bank wrapping for CPU addressing modes. The logger takes a
caller-supplied callback/context; the core never formats or prints output itself.
Messages are borrowed and valid only during the synchronous callback. Null sinks
are allowed. `MIPSNES2_ENABLE_LOGGING=0` compiles the callback invocation out.

Bus reads/writes operate on logical addresses. MMIO side effects and elapsed cycles
must eventually be specified at the bus/scheduler boundary. Page/block mapping is a
candidate, not a chosen measured implementation. Keep rare MMIO separate from common
RAM/ROM paths. Avoid broad `#ifdef PS2/PSP` in core. State allocation, buffers and
ownership must be documented before whole-machine integration. No implicit global
singleton or target physical pointer belongs in CPU state.

## Scheduler and timing proposal

A central integer emulated-time scheduler will coordinate CPU, PPU, DMA/HDMA,
timers, IRQ/NMI, SPC700/DSP and chips. Define timestamp units, rational clock ratios,
event priority and tie ordering in docs/scheduler.md before implementation. A host
wall clock must never advance emulated state. Same ROM, initial state and input
sequence should yield identical state. Timing granularity may evolve with hardware
evidence; do not scatter magic timing corrections around unrelated modules.

## Platform-Specific Optimization Architecture

**PS2 and PSP do not need identical low-level implementations when different implementations better fit each machine.**

Start with correct reference C. Profile on each actual console, identify hotspots,
record baseline, attempt suitable C improvements, then evaluate specialized kernels.
Candidate interfaces include scanline conversion, pixel kernels and audio sample
processing with explicit buffer lengths, alignment, overlap and ownership contracts.
No such kernel interface is frozen in bootstrap. Compare optimized outputs and side
effects with reference tests, including edge cases. Retain reference fallback where
valuable; record complexity and before/after measurements.

PS2: EE initially owns emulation; consider cache/layout, MIPS/MMI/scratchpad and DMA
first where profiling supports them. GS presents output, SPU2 buffers final audio;
IOP serves platform services. VU0/VU1/VIF offload must justify vector suitability and
communication costs. PSP has independent Allegrex/cache/MIPS/VFPU/GU/audio choices.
Hardware acceleration must not leak GS/GU behavior into the SNES PPU model.

## Cartridge and enhancement boundaries

Cartridge configuration selects documented mapping/chip hardware, not game-name
fixes. Chip implementations will own state and expose bus windows, events and IRQ
lines through explicit integration contracts. Base SNES precedes chips. CPU and
scheduler must account for contention and timing; a chip is not a per-game patch.

## Testing architecture

Host C tests use explicit failures, not `assert` disabled by NDEBUG. Tests link the
same core sources; a separate logging-disabled build checks that configuration.
The Python standard-library consistency checker checks foundation files, license,
local Markdown links, checkbox syntax and forbidden core dependencies. It is a
heuristic guard, not proof of emulation correctness. Integration tests and licensed
ROM fixtures come later. Console cross-compilation is distinct from runtime and
real-hardware validation. See docs/testing.md and docs/bootstrap-audit.md.

## Decisions

- 2026-10-09: GNU Make/C99 selected for small SDK-native scaffolds; no desktop GUI.
- 2026-10-09: No machine API, fake subsystems, dynarec, assembly or acceleration yet.
- 2026-10-09: Toolchains use upstream versioned binary archives plus SHA-256 pins;
  cache archives, not cross-PR compiler output. No custom container registry yet.
- Changes to these decisions require rationale, tests, status updates and measurements
  if performance is used as justification.
