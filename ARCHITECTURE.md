# Architecture

Status: project foundation plus a partial 65C816/reference-bus milestone, not a
working SNES emulator. Read [STATUS.md](STATUS.md).

## Dependency direction and ownership

`platform/{ps2,psp,host} -> core/` is the only platform dependency direction.
The shared core imports standard C headers and its own headers, never PS2SDK,
PSPSDK, GS/GU, SPU2, controller, filesystem, DMA or host-window APIs.

| Area | Responsibility | Bootstrap implementation |
|---|---|---|
| core/common | Logical address types, metadata, diagnostic sink contract | Address helpers and logging utility |
| core/cpu/65c816 | Registers, interpreter, logical bus accesses, interrupts | Caller-owned state, reset/vector path and 27-opcode reference subset |
| core/bus | WRAM/ROM/SRAM/MMIO/mirrors/open bus | Fixed reference regions for direct bytes/MMIO test fixtures; no SNES map |
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
There is no machine instance, ROM runner, renderer or APU. CPU reset implements only
the documented programmer-visible foundation; complete RESET timing remains open.
The concrete reference bus does not implement the SNES map. No per-access function-
pointer design has been benchmarked or selected for final hot paths.

## Memory and interfaces

C99 is the starting language; GNU Make keeps the foundation small. Use fixed-width
integers and caller-owned state. Logical address helpers explicitly mask to 24 bits;
they do not decide bank wrapping for CPU addressing modes. The logger takes a
caller-supplied callback/context; the core never formats or prints output itself.
Messages are borrowed and valid only during the synchronous callback. Null sinks
are allowed. `MIPSNES2_ENABLE_LOGGING=0` compiles the callback invocation out.

Bus reads/writes operate on logical addresses. The current fixed-size reference table
separates direct storage from callback-based MMIO and uses no heap. Its deterministic
unmapped latch is a test policy, not verified SNES open-bus behavior. MMIO side effects
and elapsed cycles must eventually be specified at the bus/scheduler boundary.
Page/block mapping remains a candidate. Keep rare MMIO separate from common RAM/ROM
paths. Avoid broad `#ifdef PS2/PSP` in core. No implicit global singleton or target
physical pointer belongs in CPU state.

The CPU's `instruction_cycles` counts completed instruction base cycles only. It is
not master time; reset timing, 5A22 memory speed, wait states and arbitration remain
future scheduler/bus work. An unimplemented opcode enters a sticky diagnostic stop.
Program fetch wraps PC inside PBR. Implemented data addressing is linear across the
24-bit space: word accesses and absolute indexing may carry into the next bank. This
distinction is owned by CPU addressing helpers rather than the generic bus.

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

Audio follows an emulated-PCM boundary: SPC700/S-DSP owns SNES time and produces
samples; a platform adapter transports them. The preferred future PS2 transport is
RFAuds2 behind a PS2-only audio interface. PSP and host retain independent backends.
RFAuds2 never enters `core/`, and its fixed SPU2 output rate never controls SNES time.

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
- 2026-10-10: Reference CPU/bus foundation uses caller-owned C99 state, a replaceable
  bus interface and completed-instruction base cycles; no optimized target path yet.
- 2026-10-10: Implemented data operands use linear 24-bit increments and indexed
  carry; instruction fetch remains bank-local through PBR:PC.
- 2026-10-10: RFAuds2 is the preferred future PS2 PCM transport, isolated behind a
  platform audio adapter and subject to pinning plus real-PS2 validation.
- Changes to these decisions require rationale, tests, status updates and measurements
  if performance is used as justification.
