# Architecture

Status: project foundation plus a 256-opcode 65C816, partial real SNES logical bus,
base LoROM/HiROM mapping, conservative cartridge-header selection and deterministic
scheduler/timing foundation, not a working SNES emulator. Read
[STATUS.md](STATUS.md).

## Dependency direction and ownership

`platform/{ps2,psp,host} -> core/` is the only platform dependency direction.
The shared core imports standard C headers and its own headers, never PS2SDK,
PSPSDK, GS/GU, SPU2, controller, filesystem, DMA or host-window APIs.

| Area | Responsibility | Bootstrap implementation |
|---|---|---|
| core/common | Logical address types, metadata, diagnostic sink contract | Address helpers and logging utility |
| core/cpu/65c816 | Registers, interpreter, logical bus accesses, interrupts | Caller-owned state, 256-opcode dispatch, reset/vector and IRQ/NMI boundary |
| core/bus | WRAM/ROM/SRAM/MMIO/mirrors/open bus | Test region table plus partial SNES map with WRAM, mirrors, ports, MMIO routes, FastROM state and per-access clocks |
| core/cartridge | Validated metadata, mappings, SRAM data | Base LoROM/HiROM and Mode 25/35 ExHiROM, registration/header analysis, checksum and conservative selection |
| core/ppu | State/registers, rendering and timing, not presentation | Directory only |
| core/apu/spc700 | SPC700 execution/state | Directory only |
| core/apu/dsp | Emulated SNES DSP and sample generation | Directory only |
| core/dma | SNES DMA/HDMA, bus ownership and timing | Directory only |
| core/scheduler | Deterministic emulated time and ordered events | Fixed event queue, rational clocks, 5A22 access classes and atomic CPU-step bridge |
| core/enhancement | Future chip state/bus/timing integration | Directory only |
| core/debug | Optional tracing and introspection | Directory only |
| platform/ps2 | EE lifecycle, I/O, GS presentation, final SPU2 output | Build probe only |
| platform/psp | Allegrex lifecycle, GU presentation, final audio output | Build probe only |
| platform/host | Headless tests/tools, never product architecture | Foundation smoke executable |

Directories without source contain an explicit scope README, not stub functions.
There is no machine instance, ROM runner, renderer or APU. CPU reset implements only
the documented programmer-visible foundation; complete RESET timing remains open.
The partial SNES bus is not a complete machine map: it has no complete cartridge
board coverage, implemented peripheral registers, enhancement windows or verified
hardware open-bus domains. It does connect meaningful CPU callbacks to individual
5A22 address-speed charges. No per-access function-pointer design has been benchmarked
or selected for final hot paths.

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

The separate `mipsnes2_snes_bus` is the first real machine-map layer. It receives
caller-owned 128 KiB WRAM, maps banks `7E-7F` plus `00-3F/80-BF:0000-1FFF`, implements
the `$2180-$2183` WRAM data/address ports, owns `$420D` bit 0, and routes the currently
defined PPU, APU, CPU-I/O, DMA and cartridge windows to distinct replaceable
interfaces. Missing routes return one deterministic latch; that is an explicit
foundation approximation, not a complete open-bus claim. Every read/write normalizes
to 24 bits and adds the address-dependent master-clock count. Cartridge callbacks
receive logical addresses; the base mapper now implements the supported subset below.

The cartridge-slot interface differs intentionally from ordinary MMIO callbacks: a
read/write returns whether the cartridge decoded that address. A declined read leaves
the bus's open latch intact. The cartridge implementation accepts an explicit LoROM,
HiROM or ExHiROM mapper plus caller-owned ROM/SRAM buffers. It models the ordinary
32 KiB LoROM and 64 KiB HiROM wiring, standard SRAM windows and read-only ROM. It
accepts mapper-bank-aligned ROMs through 4 MiB when the image is either a power of two
or one larger power-of-two region followed by one smaller power-of-two tail. The tail
is repeated across the next power-of-two address span; the span and first-region size
are precomputed at initialization. Layouts needing arbitrary padding remain rejected.
SRAM remains power-of-two within each base mapper's addressable window. Header
selection is a separate analysis step. Mode 25/35 ExHiROM uses its documented
`$40FFC0` internal-header location and separate 8 MiB-capable address transform.
Unofficial ExLoROM, board-specific layouts and enhancement chips remain separate work;
no title or game identity selects behavior.

Cartridge image analysis is also caller-owned and heap-free. It separates a possible
512-byte copier prefix from the ROM payload using an explicit detect/reject/require
policy, then parses LoROM, HiROM and ExHiROM internal-header candidates. Whole-image checksum
calculation excludes that prefix, normalizes the candidate checksum fields and mirrors
an unambiguous smaller power-of-two tail to the declared power-of-two span. Candidate
selection requires the full checksum and complement pair, matching mapper mode,
supported base chipset class, ROM-visible RESET vector, matching declared ROM span and
supported SRAM declaration. Exactly one candidate must satisfy every gate; zero candidates
return no-confident-header and multiple matches return ambiguous-header. Title text
and reset-opcode guessing are not selection inputs. Non-power-of-two shapes that need arbitrary padding
can be checksummed diagnostically under the documented zero-padding rule but cannot be
selected confidently. The analysis does not configure a cartridge, resolve special
board/header variants or perform file I/O. The 16 bytes preceding the title are parsed
as raw Nintendo registration metadata without turning ASCII contents into a
confidence gate.

The CPU's `instruction_cycles` counts completed instruction or block-move-byte base
cycles only; it is not master time. The timing bridge combines it with observed
address-speed accesses, while reset timing, exact cycle placement, wait states and
arbitration remain future scheduler/bus work. All 256 byte values dispatch. A defensive unimplemented-
opcode result remains for decoder refactoring; no opcode silently acts as NOP.
Program fetch wraps PC inside PBR. Implemented data addressing is linear across the
24-bit space for absolute/long forms: word accesses and absolute indexing may carry
into the next bank. Direct and Direct-indexed forms always emit bank-zero addresses.
With E=1 and DL=0, their effective low byte wraps within the D high-byte page;
otherwise D, the operand and any index add across the full 16-bit bank-zero range.
Implemented 16-bit Direct-indirect pointers use that same page-wrap rule, then DBR
supplies the data bank. The WDC exception for 24-bit Direct-indirect-long pointers
instead advances the pointer-byte reads linearly in bank zero; the pointer itself
supplies the data bank. Post-indexing advances the resulting full 24-bit address.
These distinctions are owned by CPU addressing helpers rather than the generic bus.
Implemented A/X/Y transfers expose the full 16-bit source register internally, then
use the destination's M- or X-selected width for storage and N/Z. This keeps mixed
M/X behavior in shared helpers rather than duplicating interpreter paths.
Direct/stack transfers remain full-width where the architecture requires it. Stack
destinations enforce `SH=01` only in emulation mode, and XBA exchanges the stored
accumulator bytes independently of M. These rules remain in the platform-free core.
Relative branches sign-extend their encoded displacement without host-dependent
signed casts, add it to the already-advanced 16-bit PC and never change PBR. The
shared branch helper owns taken and emulation-only page-cross cycle additions; BRL
uses a separate fixed four-cycle signed-16-bit path.
Implied CLD/SED, CLI/SEI and CLV modify only their documented P bit, advance PC by
one byte and take two processor cycles in either emulation or native mode. They do
not invoke width/mode normalization or touch registers.
Accumulator INC/DEC apply the M-selected width and preserve B in 8-bit mode. INX,
DEX, INY and DEY apply the X-selected width and leave an 8-bit index zero-extended.
All six wrap at their selected width, update only N/Z, and take two cycles.
LDA, STA, ORA, AND and EOR use M-selected accumulator width; 8-bit LDA and logic
operations preserve B, while STA writes only the selected accumulator width. ORA,
AND and EOR
update only N/Z. CMP uses M while CPX/CPY use X; comparisons preserve operands and
set N/Z from the selected-width subtraction plus C for no borrow. BIT uses M, never
changes A, and sets Z from A AND operand. Memory BIT also copies the operand's top
two selected-width bits into N/V; immediate BIT preserves N/V. ORA, AND, EOR and CMP
reuse the same Direct, indexed, indirect, long and stack-relative address helpers as
LDA/STA. Stack-relative forms use explicit bank-zero `S + offset` addressing; indirect-indexed
forms read a bank-zero 16-bit pointer, apply DBR and then add Y across the 24-bit
space. Sixteen-bit direct stack-relative data accesses wrap within bank zero, while
indirect-indexed data accesses advance linearly from their 24-bit effective address.
The LDA/STA/ORA/AND/EOR/CMP/ADC/SBC stack-relative forms share these explicit
address rules; stack push/pull behavior remains separate. Ordinary byte pushes and
pulls adjust S one byte at a time and wrap inside `01xx` when E=1. The documented
fixed-width exceptions (`JSL`, `JSR (a,X)`, `PEA`, `PEI`, `PER`, `PHD`, `PLD` and
`RTL`) instead form their multi-byte bank-zero addresses linearly before restoring
the emulation-mode `SH=01` invariant. Width-selected PHA/PLA use M and PHX/PLX plus
PHY/PLY use X; pulls set N/Z only where the instruction specifies it.
Absolute and indirect jumps keep PC-local targets in PBR unless the long form supplies
a new bank. Bank-zero pointer tables serve `JMP (a)` and `JML [a]`; PBR-local tables
serve `JMP (a,X)` and `JSR (a,X)`. JSR/JSL stack the address of the final instruction
byte and RTS/RTL increment the pulled address, with RTL also restoring PBR.
All ADC/SBC addressing forms share one M-selected arithmetic path and the established
address helpers. It preserves B for 8-bit accumulator results and derives C/V/N/Z at
the selected width. D selects independently tested packed-BCD correction without
changing address or aggregate cycle rules.
ASL/LSR/ROL/ROR use shared width and C/N/Z helpers across accumulator and memory
forms. Memory INC/DEC reuse the N/Z-only modification path; TSB/TRB derive Z from the
original operand before their bit update, and STZ preserves flags. Direct and
absolute RMW forms retain their established bank-zero/DBR address rules. The current
bus interface exposes final writes and aggregate instruction cycles, not the W65C816
dummy/modify write sequence or MLB signaling.

## Scheduler and timing foundation

The caller-owned scheduler uses unsigned 64-bit SNES master-clock timestamps and no
heap. Its fixed-capacity event queue orders first by timestamp, then caller-supplied
priority and finally insertion token. A dispatch budget prevents an event callback
from creating an unbounded same-timestamp loop. Exact rational conversion carries
integer remainder across calls, avoiding host floating point and chunk-size drift.
Wall clocks never advance emulated state.

The 5A22 timing classifier returns the documented 6, 8 or 12 master clocks for one
logical-address access and accepts the FastROM-enable state explicitly. The SNES bus
owns that state and accounts each meaningful CPU read/write. `mipsnes2_cpu_timed_step`
drains events due at the current boundary, executes one atomic CPU instruction or
interrupt entry, charges the remaining aggregate WDC cycles as 6-clock internal
cycles and advances the scheduler by the resulting elapsed master clocks. The bridge
rejects bus mismatches, counter regressions, access/cycle inconsistencies, overflows
and scheduler errors instead of hiding them.

This bridge is intentionally partial. The interpreter does not expose all W65C816
dummy reads/writes, internal-cycle positions or MLB state, so the remaining-cycle
charge has correct aggregate class only when those missing cycles are internal.
Events timestamped inside an instruction retain their scheduled timestamp but are
dispatched after the atomic CPU step; they can affect the following CPU boundary,
not pause the active instruction. WRAM refresh stalls, DMA/HDMA arbitration,
complete mapping and simultaneous-subsystem priority remain future work.

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
- 2026-10-10: Implemented Direct Page operands remain in bank zero and follow the
  sourced E/DL page-wrap rule; the reference bus remains addressing-mode agnostic.
- 2026-10-10: Implemented Direct-indirect forms keep pointer reads in bank zero;
  regular pointers follow E/DL wrapping and use DBR, while long pointers use the
  documented linear pointer-byte exception and supply their own data bank.
- 2026-10-10: A/X/Y transfers expose the full source and apply destination-selected
  M/X width to storage and N/Z; mixed-width behavior remains platform independent.
- 2026-10-11: Direct/stack transfers and XBA follow their fixed-width, emulation-SH
  and byte-exchange rules in shared C; the complete register-transfer family is now
  represented without implementing stack memory operations.
- 2026-10-11: Relative branch targets remain within PBR, signed displacements use
  explicit portable sign extension, and the complete branch family shares tested
  taken/page-cross timing rules in the platform-independent interpreter.
- 2026-10-11: Implied decimal, interrupt-disable and overflow status controls change
  only their documented P bit with fixed two-cycle timing in both CPU modes.
- 2026-10-11: Accumulator/index register increment and decrement operations wrap at
  M/X-selected width, update only N/Z and retain fixed two-cycle timing.
- 2026-10-11: Base immediate/Direct/absolute ORA, AND, EOR and CMP paths share
  selected-width ALU helpers; the complete BIT, CPX and CPY families establish their
  distinct M/X and N/V/C flag contracts without decimal arithmetic.
- 2026-10-12: ORA, AND, EOR and CMP share the established non-stack Direct,
  indexed, indirect and long address paths; stack-relative modes remain a separate
  explicit milestone rather than being approximated through unrelated helpers.
- 2026-10-12: Stack-relative accumulator ALU paths use full 16-bit bank-zero
  `S + offset` formation and a distinct bank-zero pointer reader; `(d,S),Y` applies
  DBR before full 24-bit Y indexing and has no Direct Page or index-width penalty.
- 2026-10-12: Stack-relative LDA/STA reuse those effective-address rules while
  direct 16-bit data reads/writes explicitly wrap inside bank zero; indirect-indexed
  data remains linear at the final 24-bit address.
- 2026-10-15: Every ADC/SBC address form shares the M-width arithmetic and existing
  Direct, indexed, indirect, stack-relative and long address helpers; binary behavior
  is verified without creating platform-specific interpreter paths.
- 2026-10-16: Packed-BCD ADC/SBC is implemented at both M widths and independently
  audited against a pinned instruction-vector corpus; addressing and aggregate cycle
  rules remain shared with binary operation.
- 2026-10-16: ASL/LSR/ROL/ROR, memory INC/DEC, TSB/TRB and STZ share M-selected
  helpers and established logical address paths. Bus-cycle dummy writes and MLB
  remain explicit future work rather than being approximated.
- 2026-10-17: All architectural stack push/pull/effective-address instructions and
  jump/subroutine call/return forms use explicit bank-zero stack and PBR/direct-bank
  pointer rules. WDC's emulation-mode multi-byte stack exceptions remain distinct
  from ordinary byte-at-a-time page wrapping, and callback-order tests cover JSL
  and JSR `(a,X)` meaningful bus accesses.
- 2026-10-17: The scheduler foundation uses 64-bit master-clock timestamps, a bounded
  caller-owned event table, explicit priority plus insertion tie order and exact
  rational clock remainders. The 5A22 address classifier records 6/8/12-clock access
  classes separately from the still-incomplete CPU bus-cycle integration.
- 2026-10-17: The partial SNES bus maps caller-owned WRAM and low-bank mirrors,
  implements WRAM ports and `$420D` FastROM state, routes subsystem windows and
  charges meaningful accesses at 5A22 address speeds. The atomic CPU timing bridge
  infers only the remaining aggregate internal cycles; exact cycle placement,
  refresh and bus arbitration remain incomplete rather than silently claimed.
- 2026-10-17: The cartridge slot can explicitly decline addresses so open-bus
  ownership stays in the SNES bus. Original caller-owned C99 base LoROM/HiROM maps
  support power-of-two ROM/SRAM only; detection and irregular board layouts remain
  incomplete instead of being guessed or keyed to games.
- 2026-10-18: Base mapper initialization accepts only power-of-two ROMs or an
  unambiguous larger power-of-two region plus smaller power-of-two tail, aligned to
  the mapper's bank size. It precomputes the address span and rejects layouts that
  would require arbitrary padding.
- 2026-10-18: Documented Map Mode 25/35 ExHiROM is distinct from base HiROM: its
  registration/header candidate lives at payload offset `$40FFB0/$40FFC0`, the first
  4 MiB maps through banks `C0-FF`, and the extended region maps through `40-7D` with
  the corresponding upper-half mirrors. Unofficial ExLoROM is not inferred from it.
- 2026-10-18: Base LoROM/HiROM selection is an explicit conservative analysis step,
  separate from executable mapper initialization. It requires agreement among full
  checksum, complement, map mode, chipset, RESET vector and declared sizes, returns
  ambiguity/no-match explicitly and never scores title text or a guessed reset opcode.
- 2026-10-17: Cartridge image layout and internal-header parsing are distinct from
  executable mapper configuration. Copier-prefix handling is an explicit caller policy;
  raw header fields are diagnostic evidence, never a title-based selection rule.
- 2026-10-10: RFAuds2 is the preferred future PS2 PCM transport, isolated behind a
  platform audio adapter and subject to pinning plus real-PS2 validation.
- Changes to these decisions require rationale, tests, status updates and measurements
  if performance is used as justification.
