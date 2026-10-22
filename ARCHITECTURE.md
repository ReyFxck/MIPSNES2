# Architecture

Status: project foundation plus a 256-opcode 65C816, partial real SNES logical bus,
base LoROM/HiROM mapping, conservative cartridge-header selection, deterministic
scheduler/timing, partial 5A22 CPU-I/O and non-interlaced H/V foundations, not a
working SNES emulator. Read
[STATUS.md](STATUS.md).

## Dependency direction and ownership

`platform/{ps2,psp,host} -> core/` is the only platform dependency direction.
The shared core imports standard C headers and its own headers, never PS2SDK,
PSPSDK, GS/GU, SPU2, controller, filesystem, DMA or host-window APIs.

| Area | Responsibility | Bootstrap implementation |
|---|---|---|
| core/common | Logical address types, metadata, diagnostic sink contract | Address helpers and logging utility |
| core/cpu/65c816 | Registers, interpreter, logical bus accesses, interrupts | Caller-owned state, 256-opcode dispatch, reset/vector and IRQ/NMI/ABORT boundary |
| core/bus | WRAM/ROM/SRAM/MMIO/mirrors/open bus | Test region table plus partial SNES map with WRAM, CPU-I/O latches/math, mirrors, MMIO routes, FastROM state and per-access clocks |
| core/cartridge | Validated metadata, mappings, SRAM data | Base LoROM/HiROM and Mode 25/35 ExHiROM, registration/header analysis, checksum and conservative selection |
| core/ppu | State/registers, rendering and timing, not presentation | Non-interlaced H/V counter-register subset only; no renderer or PPU memory |
| core/apu/spc700 | SPC700 execution/state | Directory only |
| core/apu/dsp | Emulated SNES DSP and sample generation | Directory only |
| core/dma | SNES DMA/HDMA, bus ownership and timing | Directory only |
| core/scheduler | Deterministic emulated time and ordered events | Fixed event queue, rational clocks, 5A22 access classes, atomic CPU-step bridge, non-interlaced H/V phase, interrupt-line aggregation and shared master-clock coordinator |
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

The optional caller-owned `mipsnes2_cpu_io` state sits behind that CPU-I/O route. It
implements NMITIMEN, the 9-bit H/V compare values, RDNMI, TIMEUP, HVBJOY and the
unsigned 8x8 multiply/16-by-8 divide register set. It accepts four caller-owned
logical 16-bit controller inputs and exposes completed automatic reads through
`$4218-$421F`; input sampling remains outside the core and no platform controller API
crosses this boundary. The same words feed the standard manual serial-port
foundation: a `$4016` falling strobe snapshots all four lines, and `$4016`/`$4017`
independently shift the paired first/third and second/fourth lines. This is
platform-neutral CPU-I/O state, not a host controller backend. Math triggers retain old visible
results until an 8- or 16-processor-cycle countdown completes; divide-by-zero yields
`FFFF` plus the dividend remainder. CPU-bus reads clock the unit after sampling and
writes clock it before latching the new value, so the trigger write itself is not
counted as elapsed work. Aggregate internal cycles are advanced at the instruction
boundary. Busy-time retriggering currently deterministically replaces the pending
job, but that is an explicit unverified approximation rather than a hardware claim.
The
separate caller-owned H/V component supplies non-interlaced NTSC/PAL blank, NMI-flag
and stable-register H/V timer-match transitions. VBlank status and the RDNMI flag are
distinct inputs because the flag begins two master clocks after VBlank. CPU-I/O
exports a latched NMI edge and timer-IRQ level without depending on host or platform
APIs. The H/V component does not yet model exact mid-instruction timer-register
writes, repeated reprogrammed matches in one scanline or the narrow read-at-match
TIMEUP exception. Automatic joypad polling uses a deterministic H=298 start on the
first VBlank line and a 4224-master-clock busy period. The duration and atomic result
commit are evidence-backed; the hardware's varying start phase and busy-time partial
register contents remain deliberately approximated. Manual shifting is intentionally
independent of automatic polling until their physical contention is verified.
CPU-I/O does not implement nonstandard peripheral protocols, wire the two auxiliary
`$4016` output pins to devices, or combine cartridge IRQs.

The same CPU-I/O component owns the programmable `$4201` output latch and `$4213`
combined pin read. Caller-owned external levels are wired-AND with the output latch,
matching open-collector lines where either side can pull low. A combined bit-7
high-to-low transition is retained as a one-shot H/V-latch request. CPU-I/O itself
does not call the PPU. The optional SNES-bus counter adapter consumes the request.

`mipsnes2_ppu_counters` owns only the latched nine-bit H/V values, separate low/high
read selectors, latch flag, caller-selected PPU2 version and PPU2 open-bus byte.
`$2137` captures the supplied non-interlaced H/V snapshot when WRIO bit 7 enables it;
`$213C/$213D` read the values and `$213F` reports field/region/version plus resets
the selectors. H conversion accounts for six-clock dots 323 and 327 on normal lines
and four-clock dots throughout the NTSC short line. This does not change the 5A22's
timer formula. The bus can optionally bind this state, CPU-I/O and the same H/V object
used by the timing bridge; a different or absent bridge H/V domain is rejected.
Other PPU accesses still use the existing callback, not fake register behavior.

WRIO writes are synchronized immediately at the bus boundary. External input setters
require `mipsnes2_snes_bus_sync_hv_latch` before advancing emulated time. The pending
edge has no timestamp and coalesces multiple unsynchronized edges. During an atomic
CPU instruction, captures see its starting H/V snapshot rather than exact bus-cycle
time. The measured WRIO-versus-SLHV latency, held-low flag behavior and repeated
capture details remain documented hardware-validation gaps. See the
[counter audit](docs/hv-counter-latch-audit.md).

`mipsnes2_interrupt_lines` is the machine-wiring layer between independent IRQ/NMI
producers and the single CPU input pair. Its bitmask keeps internal timer, cartridge
and expansion IRQ levels independent, so acknowledging TIMEUP cannot erase another
asserted source. It consumes the CPU-I/O NMI edge into its own pending latch and
drives the existing 65C816 input API only at an explicit CPU boundary. This is not a
new emulated chip and contains no platform behavior.

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

Cartridge preparation joins these layers without taking platform ownership. A
caller-supplied complete file buffer becomes an immutable image view and conservative
selection plan. The plan reports header-declared SRAM bytes; construction then binds
the selected mapper to caller-owned ROM/SRAM storage and reuses the executable
mapper's validation. Failed analysis/construction leaves destination objects
unchanged. Paths, file handles, allocation and persistence remain platform concerns.

SRAM contents also remain caller-owned. The mapper marks only value-changing SRAM
writes dirty and advances a revision token. Platform persistence code reads a live
const view and may clear dirty state only by acknowledging the exact revision it
successfully stored; a concurrent newer write makes the token stale. File naming,
atomic replacement and save scheduling remain outside the shared core.

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

## Incremental CPU operation contract

`mipsnes2_cpu_cycle_state` is caller-owned resumable state. A caller begins one
instruction, peeks at the next descriptor without changing CPU or bus state, then
commits that exact descriptor once. Descriptors distinguish zero-time control,
logical 24-bit reads, writes, internal processor cycles and an explicit
`LEGACY_ATOMIC` remainder. A changed descriptor is rejected as stale before an
effect. Active state must be retained and resumed; interleaving another CPU step or
discarding a partially committed cursor is not supported by this foundation.

The migrated instruction sequences are opcode fetch plus `NOP` (`EA`), immediate
`LDA` (`A9`), absolute `STA` (`8D`), `REP` (`C2`), `SEP` (`E2`), `BRK` (`00`) and
`COP` (`02`), absolute `JSR` (`20`) and `RTS` (`60`). They expose width-selected
operand bytes, the final flag/internal action, low-before-high writes, signature
reads, subroutine stack accesses and interrupt stack/vector accesses as applicable.
Long `JSL` (`22`) and `RTL` (`6B`) are also migrated, including their linear
multi-byte stack exception in emulation mode and PBR save/restore order.
Indexed-indirect `JSR` (`FC`) is migrated with its WDC-ordered early PCH/PCL writes,
late high-operand read, internal operation and bank-local pointer reads. Its
two-byte stack transfer uses the same documented linear emulation-mode exception.
All five `JMP/JML` forms (`4C/5C/6C/7C/DC`) are migrated through shared jump phases.
Direct operands remain in PBR, ordinary indirect tables wrap in bank zero,
indexed-indirect tables wrap in PBR, and long forms commit PBR only with the final
target byte.
Boundary `ABORT`, `NMI` and `IRQ` entry also use the cursor, with the accepted cause
and entry state latched before any effect. `mipsnes2_cpu_step` drives this same
contract to completion, preserving the established instruction API. The remaining
239 opcodes return an explicit legacy-atomic operation after their opcode fetch and
use the existing evaluator for the remainder.

This is deliberately not wired into the scheduler yet. Remaining opcodes, reset,
dummy cycles, mid-instruction `ABORT` and undocumented/signal-level bus activity are
still atomic or absent. Consequently the central timed-step bridge remains
instruction-atomic and must not dispatch events from bus callbacks. The initial
foundation is recorded in [the cycle-contract audit](docs/65c816-cycle-contract-audit.md);
interrupt migration scope and verification are recorded in
[the interrupt-cycle audit](docs/65c816-interrupt-cycle-audit.md); the first
subroutine pair is recorded in
[the subroutine-cycle audit](docs/65c816-subroutine-cycle-audit.md), followed by the
[long-subroutine-cycle audit](docs/65c816-long-subroutine-cycle-audit.md).

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

`mipsnes2_master_timing_run_until` is the first explicit common-domain coordinator.
It requires the scheduler timestamp and H/V elapsed timestamp to agree, advances H/V
state to each pending scheduler-event timestamp, then dispatches callbacks. Therefore
a callback at an H/V transition observes the updated blank/IRQ/NMI-latch state. The
dispatch budget and same-timestamp insertion rules remain visible, and budget
exhaustion leaves both domains aligned at the stopped event timestamp. The optional
`mipsnes2_cpu_timed_step_hv` entry point feeds one atomic CPU step's elapsed delta
through this coordinator. The older scheduler-only entry point remains available for
focused tests and callers that have not constructed H/V and CPU-I/O state.
`mipsnes2_cpu_timed_step_hv_interrupts` additionally samples CPU-I/O plus the caller's
other IRQ sources before and after the atomic step. Inputs visible before the step can
enter the CPU immediately; transitions during the step become CPU inputs for the next
boundary. The report distinguishes NMI-edge delivery from the resulting IRQ level.

This bridge is intentionally partial. The interpreter does not expose all W65C816
dummy reads/writes, internal-cycle positions or MLB state, so the remaining-cycle
charge has correct aggregate class only when those missing cycles are internal.
Events timestamped inside an instruction retain their scheduled timestamp but are
dispatched after the atomic CPU step; they can affect the following CPU boundary,
not pause the active instruction. WRAM refresh stalls, DMA/HDMA arbitration,
complete mapping and simultaneous-subsystem priority remain future work.
The same unresolved per-cycle placement limits exact math-unit observation for
instructions whose internal cycles occur before a result-register access; ordinary
bus cycles and aggregate NOP/internal waits are deterministic and tested.

An attempted synchronous clock sink inside bus callbacks was removed after review:
the then-atomic interpreter could not abort or resume an instruction, so an event-
budget failure could leave CPU, memory and scheduler state partially committed.
Meaningful-access event dispatch must wait until the incremental CPU-operation
contract covers every path the bridge can execute, including interrupt entry, and
has explicit scheduler failure/cancellation handling.
The analysis is retained in the
[access-timing post-mortem](docs/cpu-access-timing-postmortem.md).

`mipsnes2_hv_timing` owns a deterministic non-interlaced beam phase in SNES master
clocks. It covers 262-line NTSC and 312-line PAL frames, 224/239-line VBlank starts,
HBlank boundaries, the alternating NTSC short line, the delayed NMI flag and H/V/HV
timer matches for stable register state. H matches use `14 + HTIME * 4` clocks and
V-only plus HV-with-HTIME-zero matches use the previous-line-relative 1374-clock
position, including the NTSC short-line offset and documented H=153 suppression.
It does not render, own PPU memory or model interlace/latched PPU dot counters. The
shared coordinator now drives it and `mipsnes2_scheduler.now` from one elapsed-clock
delta. H/V transitions precede general scheduler callbacks at the same timestamp as
a deterministic provisional policy; exact hardware priority still requires research.
Because the CPU step is atomic, a transition inside an instruction is observed only
after that instruction and can affect the following CPU boundary. CPU-I/O IRQ/NMI
latches are not yet automatically sampled into the CPU inputs.

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
- 2026-10-19: The first H/V component uses caller-owned non-interlaced NTSC/PAL
  master-clock state and drives blank/NMI inputs independently. VBlank begins at H=0
  while RDNMI/NMI asserts at H=0.5; interlace remains explicit future work.
- 2026-10-19: Stable-register H/V timer matches are generated by the H/V component
  from documented master-clock formulas and drive the existing CPU-I/O TIMEUP latch.
  Exact within-instruction register-write ordering, repeat reprogramming within one
  scanline and the narrow TIMEUP read-at-match exception remain explicit future work.
- 2026-10-19: The H/V component and event scheduler share an explicit coordinator
  that rejects clock-domain divergence, advances H/V before same-timestamp callbacks
  and preserves alignment on event-budget exhaustion. CPU stepping remains atomic;
  exact within-instruction boundaries remain open.
- 2026-10-19: CPU interrupt wiring uses a caller-owned source bitmask rather than
  letting the H/V timer overwrite the CPU IRQ input. CPU-I/O NMI/IRQ state is sampled
  at atomic CPU boundaries; exact within-instruction sampling and final subsystem tie
  priorities remain open.
- 2026-10-20: The CPU-I/O math unit uses explicit processor-cycle countdowns, not
  master-clock or platform-audio time. Reads sample before advancing their bus cycle;
  writes advance an existing job before latching a new trigger. Busy retriggering is
  deterministic but remains an acknowledged approximation pending hardware evidence.
- 2026-10-21: A caller-latched ABORT pulse now implements the sourced vectors,
  stack/status effects, interrupt priority and WAI no-wake/restart-address behavior
  at CPU boundaries. Physical mid-instruction rollback and cycle-sensitive exceptions
  remain blocked on a resumable CPU cycle/micro-operation contract.
- 2026-10-21: A caller-owned peek/commit CPU-operation cursor exposes exact-once
  sequences for opcode fetch and `NOP`, immediate `LDA`, absolute `STA`, `REP` and
  `SEP`. The other 251 opcodes and interrupt entry remain explicitly atomic, and the
  scheduler is not yet connected to the cursor.
- 2026-10-21: The cursor now also exposes `BRK`, `COP` and boundary `ABORT`, `NMI`
  and `IRQ` entry. Accepted interrupt state is latched before effects; stack writes
  and vector reads are explicit. Hardware entries use two provisional internal
  operations rather than inventing unsourced dummy bus traffic. The remaining 249
  opcodes, reset, exact electrical phase placement, mid-instruction `ABORT` and
  scheduler integration remain open.
- 2026-10-21: Absolute `JSR` and `RTS` now expose their sourced internal, operand
  and stack-operation order through the same cursor. Distinct internal tokens prevent
  stale consecutive commits; 247 opcode remainders and scheduler integration remain.
- 2026-10-22: `JSL` and `RTL` now expose sourced internal, operand, PBR and linear
  stack-access order through the same cursor. Their shared-core implementation adds
  no platform or game-specific branch; 245 opcode remainders remain.
- 2026-10-22: Indexed-indirect `JSR` (`FC`) now exposes the WDC Table 5-7 operand,
  linear stack, internal and PBR-local pointer order through the shared-core cursor;
  244 opcode remainders remain and no backend or game-specific path was added.
- 2026-10-22: The complete five-opcode `JMP/JML` family now exposes direct,
  long, bank-zero indirect and PBR-indexed-indirect reads through shared jump phases;
  239 opcode remainders remain with no backend or game-specific path.
- Changes to these decisions require rationale, tests, status updates and measurements
  if performance is used as justification.
