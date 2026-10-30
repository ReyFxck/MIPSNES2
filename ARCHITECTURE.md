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
| core/ppu | State/registers, rendering and timing, not presentation | Non-interlaced H/V counters, 64 KiB buffered VRAM ports, 512-byte CGRAM ports, 544-byte OAM ports, OBJ line, four Mode 0 or three Mode 1 BG lines and priority mixers, scheduled overflow state and 4bpp tile decode; optional reference video-line callback |
| core/dma | Channel state and transfer planning | Eight register banks, mode/address helpers and bounded general-DMA/HDMA cursors; resumable scheduler, shared refresh, A/B bus policy, same-channel cancellation, VBlank deactivation and final-channel zero-descriptor shortening integrated; electrical timing pending |
| core/apu | S-CPU/S-SMP communication and timing | Directional ports plus nominal rational master-to-S-SMP clock bridge; electrical race order and oscillator drift pending |
| core/apu/spc700 | S-SMP address map, timers and SPC700 execution/state | All 256 opcodes, ARAM/IPL, ports, `$F2/$F3` DSP route and three-stage timers under the APU clock; exact bus phases and interrupt/reset entry pending |
| core/apu/dsp | Emulated SNES DSP and sample generation | Register/event cadence, eight BRR/noise voices, Gaussian interpolation, ADSR/GAIN, `OUTX`, `PMON`, signed voice/main volume, ordered dry mixing, ARAM echo/FIR/feedback, mute-gated final samples and optional bounded native PCM handoff; exact microphases and device transport absent |
| core/scheduler | Deterministic emulated time and ordered events | Fixed event queue, rational clocks, 5A22 access classes, resumable CPU-operation bridge plus atomic compatibility bridge, non-interlaced H/V phase, interrupt-line aggregation and shared master-clock coordinator |
| core/enhancement | Future chip state/bus/timing integration | Directory only |
| core/debug | Optional tracing and introspection | Directory only |
| core/machine | Stable instance composition and timed power-on/CPU/APU execution | Caller-owned ROM/WRAM/SRAM/optional VRAM/CGRAM/OAM/ARAM/IPL/PCM storage, cartridge, CPUs, DSP register state, bus, CPU-I/O, counters, H/V and scheduler |
| platform/ps2 | EE lifecycle, I/O, GS presentation, final SPU2 output | Default five-case diagnostic; explicit ROM path, single-frame GS submission and bounded 48 kHz accepted-prefix/RFAuds2 lifecycle diagnostic; GS/audio target runtime unverified |
| platform/psp | Allegrex lifecycle, GU presentation, final audio output | Build probe only |
| platform/host | Headless tests/tools, never product architecture | Foundation smoke executable |

Directories without source contain an explicit scope README, not stub functions.
There is a partial machine instance, an explicit-path PS2 ROM-file adapter and a
host-verified PS2 audio transport contract, but no full ROM runner, complete renderer
or device binding. `mipsnes2_machine_init` validates an image and constructs the
cartridge in place; `power_on` reinitializes CPU/I-O, H/V, bus, scheduler and
interrupts, APU communication latches and VRAM/CGRAM/OAM port registers, retaining caller-owned WRAM, SRAM and
optional VRAM/CGRAM/OAM/ARAM bytes while resetting optional PCM queue state. The
machine exposes the cartridge's live SRAM view and exact revision acknowledgement
as the platform persistence boundary; paths, copying, durable writes and scheduling
remain backend responsibilities. An optional S-SMP map borrows 64 KiB ARAM
and optional caller-provided 64-byte IPL; without the IPL, mapped high-page
reads fail explicitly. PS2 reserves ARAM, while PSP merely compiles the same
core. The S-SMP map and S-CPU bus meet through machine-owned directional
ports. The map also owns three APU-clock-driven timers, with an explicit unit-test
advance API: timer 0/1 observe a
shared 128-cycle stage-1 edge and timer 2 each 16-cycle subdivision; `$F1`
enable edges, `$FA-$FC` targets and `$FD-$FF` read-clear outputs are routed.
The caller-owned SPC700 foundation fetches through this map and atomically executes
all 256 opcode values. Direct-page, pointer, stack and absolute wrapping remain
inside the S-SMP address domain and all mapped accesses retain MMIO side effects.
The APU bridge converts absolute machine master time to nominal 1.024 MHz S-SMP
cycles with a carried rational remainder. It conservatively admits whole atomic
instructions, advances timers and S-DSP cadence, and keeps running devices while
SLEEP/STOP halt instruction execution. Machine configuration can copy an explicit
SPC700 register seed; undocumented reset registers are not fabricated.

The S-DSP layer exposes `$F2/$F3`, a 128-byte register file, bit-7 write
inhibition, `ENDX` clear, internal `FLG` reset state and a 32-S-SMP-cycle sample
event. Its eight voice records poll `KON`/`KOFF` every other sample, latch
`DIR`/`SRCN` source addresses from borrowed ARAM and model the five-event key-on
preparation period. A side-effect-free API decodes one BRR block with exact
filter/clamp/wrap arithmetic. Attached voices preload three four-sample groups,
rotate a physical twelve-sample ring as the 14-bit 2.12 pitch position crosses
four samples, and traverse end/loop blocks. A hardware-published 512-entry table
feeds the documented four-point Gaussian wrap/clamp arithmetic. BRR or shared
signed-15 noise sources pass through the 11-bit envelope as left-aligned output,
`OUTX`, sample-level `PMON`, signed voice volumes, ordered saturation and signed
main volume. A shared countdown phase drives the documented 32 envelope
rates/offsets; each voice has an 11-bit ADSR/GAIN/release value and an `ENVX`
shadow. `EON` selects a separately saturated voice sum for the stereo echo path.
Each sample reads a left-aligned signed-15 pair from the 16-bit-wrapping
`ESA`/`EDL` ARAM ring, advances two eight-entry histories, applies the signed
1.7 FIR coefficients with the documented partial wrap/final clamp, and combines
the result through signed `EVOLL`/`EVOLR` and `EFB`. `FLG` bit 5 suppresses only
the left-aligned ring write; bit 6 gates the final DAC-facing sample while the
entire pipeline continues. An optional caller-owned FIFO receives that signed-16
left/right pair once per 32 kHz sample event. It drops newest and counts when full,
returns no frame when empty and never controls S-DSP time. A separate optional
integer-linear converter drains already-produced frames into bounded caller output
blocks at rates greater than or equal to 32 kHz; it retains at most two input frames,
synthesizes no samples on starvation and cannot advance the machine. Neither layer
performs device output. Exact
instruction bus phases, within-DSP-loop register visibility, mailbox races and
physical oscillator drift remain open.
`resume` advances one
timed RESET sequence or instruction to a caller-specified horizon; `run_until`
also bounds completed sequences and retains a paused cursor. The instance
has a stable address because its CPU and bus callbacks point into it; callers
must not copy or move it. PS2 accepts a file path from an explicit command-line
argument into preallocated EE memory, while PSP still has no file adapter. Neither
backend produces a verified continuous SNES video/audio stream. CPU reset implements only
the documented programmer-visible foundation and resumable vector reads; complete
signal-level RESET timing remains open.
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
`$4218-$421F`. Platform backends translate physical state into SNES serial order
and inject it through `mipsnes2_machine_set_joypad`; physical sampling remains
outside the core. The same words feed the standard manual serial-port
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
Machine-owned VRAM, CGRAM and OAM ports run before the optional external PPU callback;
unhandled PPU addresses use that callback. CGRAM high reads share the PPU2
open-bus state held by the counter registers. The machine schedules an OAM
VBlank pointer reload in the shared clock domain and tracks `$2100` forced
blank for that gate and its first-VBlank-line release transition. An OBJ
event at HBlank start prepares the next visible line and accumulates `$213E`
range/time overflow bits until the frame boundary. This phase is provisional;
the first sprite follows `$2103` priority rotation and the current internal
OAM pointer, and forced blank suppresses this model's
selection. When caller-owned VRAM exists, selected 4bpp OBJ slivers produce a
256-entry color/priority line. The same event prepares four Mode 0 BG 2bpp
lines or three Mode 1 lines (4bpp BG1/BG2 and 2bpp BG3) using machine-owned
`$2105/$2107-$2114` subset state when VRAM exists. `$212C/$212D`
independently select main/sub layers for the mode's priority mixer.
`$2123-$212B/$212E/$212F` produce separate layer masks before
selection. Shared passes resolve unattenuated main/sub BGR555 lines through
caller-owned CGRAM, with `$2132` fixed color as the sub backdrop. A bounded
`$2130/$2131` math pass combines them or uses fixed color, then applies
`$2100` brightness in nominal 5-bit digital levels. The `$2125/$212B`
color-window predicate feeds `$2130` main clipping and math suppression.
Forced blank leaves the scheduled lines black. Scroll-write latches use a
published but hardware-unverified formula. Exact mid-line behavior and
core display output are absent. An optional synchronous callback hands the
completed 256-pixel BGR555 line to caller-owned presentation code; the
buffer stays machine-owned and can be reused after the callback returns.
The PS2 ROM adapter copies that callback into a PS2-only RGBA32 frame;
PSP remains independent and has no presentation path yet.
The PS2 backend can submit the captured buffer once to a GS framebuffer;
there is no verified runtime display or frame loop. PSP still has no
presentation path.

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
address-speed accesses, while signal-level reset timing, exact cycle placement, wait
states and arbitration remain future scheduler/bus work. All 256 byte values dispatch. A defensive unimplemented-
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
logical 24-bit reads, writes and internal processor cycles. All 256 opcode values
have complete-bus resumable sequences; `LEGACY_ATOMIC` remains only for
missing-write fail-stop preflight. A changed descriptor is rejected as stale before
an effect. Active state must be retained and resumed; interleaving another CPU step
or discarding a partially committed cursor is not supported by this foundation.

The migrated instruction sequences are opcode fetch plus `NOP` (`EA`), every
`LDA/STA/LDX/LDY/STX/STY` addressing form, `REP` (`C2`), `SEP` (`E2`), `BRK`
(`00`) and `COP` (`02`), absolute `JSR` (`20`) and `RTS` (`60`). They expose
width-selected operand bytes, the final flag/internal action, low-before-high
writes, signature reads, subroutine stack accesses and interrupt stack/vector
accesses as applicable.
Long `JSL` (`22`) and `RTL` (`6B`) are also migrated, including their linear
multi-byte stack exception in emulation mode and PBR save/restore order.
Indexed-indirect `JSR` (`FC`) is migrated with its WDC-ordered early PCH/PCL writes,
late high-operand read, internal operation and bank-local pointer reads. Its
two-byte stack transfer uses the same documented linear emulation-mode exception.
All five `JMP/JML` forms (`4C/5C/6C/7C/DC`) are migrated through shared jump phases.
Direct operands remain in PBR, ordinary indirect tables wrap in bank zero,
indexed-indirect tables wrap in PBR, and long forms commit PBR only with the final
target byte.
All ten conditional, relative-always and relative-long branches
(`10/30/50/70/80/82/90/B0/D0/F0`) use shared branch phases. The cursor exposes the
operand reads and address-free taken/page-cross/long internal cycles; targets wrap
inside PBR, and the extra page-cross cycle remains exclusive to taken 8-bit branches
in emulation mode.
All sixteen stack push/pull/effective-address opcodes
(`08/0B/28/2B/48/4B/5A/62/68/7A/8B/AB/D4/DA/F4/FA`) use shared stack phases. The
cursor exposes internal operations, width-selected bytes, high-before-low pushes,
low-before-high pulls, Direct Page pointer reads and the documented linear
emulation-mode multi-byte stack exceptions. Ordinary byte transfers retain `01xx`
wrapping, while native transfers retain full 16-bit S wrapping.
Twenty-seven implied status, transfer and register increment/decrement opcodes
(`18/1A/1B/38/3A/3B/58/5B/78/7B/88/8A/98/9A/9B/A8/AA/B8/BA/BB/C8/CA/D8/E8/EB/F8/FB`)
use shared internal phases. Their architectural effect occurs only when the final
internal descriptor commits. `XBA` uses two distinct internal tokens for its
three-cycle sequence; the remaining members use one internal token.
All thirty-six accumulator/memory ASL/LSR/ROL/ROR, memory INC/DEC, TSB/TRB and STZ
opcodes use shared RMW phases. Direct Page penalties, fixed indexed cycles,
width-selected reads/writes and linear versus Direct high-byte rules remain explicit;
distinct internal tokens separate Direct penalty, indexing and modification commits.
All forty-five `LDA/STA/LDX/LDY/STX/STY` opcodes use one shared load/store state
machine. It separates program operands, Direct Page and stack address formation,
regular and long pointer reads, conditional indexed cycles and M/X-selected data
transfers without introducing backend or title-specific behavior.
The same operand state machine covers all 101 `ORA/AND/EOR/ADC/SBC/CMP/CPX/CPY/BIT`
opcodes; only the final architectural effect differs. Dedicated phases cover `RTI`
stack restoration, `WDM` signature consumption, `MVP/MVN` byte transfers and the
final wait/stop transition of `WAI/STP`.
Boundary `ABORT`, `NMI` and `IRQ` entry also use the cursor, with the accepted cause
and entry state latched before any effect. `mipsnes2_cpu_step` drives this same
contract to completion, preserving the established instruction API.

`mipsnes2_cpu_cycle_begin_reset` starts a separate RESET path in the same cursor.
Its zero-time control descriptor validates the read bus and commits the documented
mode/register invariants before two explicit reads at `00:FFFC` and `00:FFFD`.
The high-vector commit installs PC and releases WAI/STP/diagnostic stop state.
`mipsnes2_cpu_reset` is the compatibility wrapper that drives this path to completion.
The path deliberately preserves the existing zero `instruction_cycles` result and
does not invent unsourced internal/dummy reset cycles.

`mipsnes2_cpu_timing_state` wires this cursor into an opt-in scheduler bridge without
putting scheduler state in the CPU. A timed READ, WRITE or INTERNAL descriptor is
latched with its 6/8/12-clock cost, may remain in flight across requested horizons or
event-budget failures, and commits exactly once after the shared time domain reaches
its endpoint. Events at the endpoint run before the CPU/bus commit under the current
inclusive scheduler contract. RESET is selected explicitly and retains its distinct
zero-processor-cycle accounting. A bounded mid-instruction `ABORT` pulse restores
pre-fetch CPU registers before the following boundary entry. Writes block until
the caller qualifies each external effect or explicitly inhibits the target while
still accounting the bus cycle. This is not automatic SNES external-gate behavior;
signal-level RESET, RMW electrical and RTI/interrupt exceptions, dummy cycles and undocumented bus
activity remain absent.
The original atomic timed-step
entry points retain their established semantics. The initial
foundation is recorded in [the cycle-contract audit](docs/65c816-cycle-contract-audit.md);
interrupt migration scope and verification are recorded in
[the interrupt-cycle audit](docs/65c816-interrupt-cycle-audit.md); the first
subroutine pair is recorded in
[the subroutine-cycle audit](docs/65c816-subroutine-cycle-audit.md), followed by the
[long-subroutine-cycle audit](docs/65c816-long-subroutine-cycle-audit.md) and the
[read/modify/write-cycle audit](docs/65c816-rmw-cycle-audit.md). Completion of the
opcode cursor and RESET-vector extension are recorded in the
[complete opcode-cycle audit](docs/65c816-final-resumable-cycle-audit.md) and
[RESET-cycle audit](docs/65c816-reset-cycle-audit.md). Scheduler wiring and its
transactional limits are recorded in the
[operation-timing audit](docs/cpu-operation-timing-audit.md).
The ABORT slice and its hard limits are recorded in the
[mid-instruction ABORT audit](docs/65c816-mid-instruction-abort-slice-audit.md).
The recoverable per-write decision and its electrical limits are recorded in
[the ABORT write-qualification audit](docs/65c816-abort-write-qualification-audit.md).

## Scheduler and timing foundation

The caller-owned scheduler uses unsigned 64-bit SNES master-clock timestamps and no
heap. Its fixed-capacity event queue orders first by timestamp, then caller-supplied
priority and finally insertion token. A dispatch budget prevents an event callback
from creating an unbounded same-timestamp loop. Exact rational conversion carries
integer remainder across calls, avoiding host floating point and chunk-size drift.
Wall clocks never advance emulated state.

The 5A22 timing classifier returns the documented 6, 8 or 12 master clocks for one
logical-address access and accepts the FastROM-enable state explicitly. The SNES bus
owns that state and accounts each meaningful CPU read/write. The opt-in
`mipsnes2_cpu_timed_resume*` APIs advance one cursor operation through scheduler
and optional H/V time before committing its effect. Their caller-owned state binds
to one CPU/bus/domain tuple while active, latches FastROM and access cost at the
operation start and survives requested-horizon or event-budget returns without
restarting elapsed clocks. The `mipsnes2_cpu_timed_step*` compatibility family retains
its instruction-atomic, aggregate-cycle semantics.

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

The resumable bridge retains CPU-I/O NMI/IRQ edges during an in-flight operation,
but drives the CPU inputs only before starting the cursor and after completing it.
Events at an operation endpoint are dispatched before the CPU/bus commit under the
inclusive scheduler contract. The bridge is still partial: not all W65C816 dummy
reads/writes, electrical sub-cycle positions or MLB state are represented. WRAM
the electrical phases of same-channel DMA/HDMA arbitration, complete mapping
and simultaneous-subsystem priority remain future work.
The same unresolved per-cycle placement limits exact math-unit observation for
instructions whose internal cycles occur before a result-register access; ordinary
bus cycles and aggregate NOP/internal waits are deterministic and tested.

An attempted synchronous clock sink inside bus callbacks was removed after review:
the then-atomic interpreter could not abort or resume an instruction, so an event-
budget failure could leave CPU, memory and scheduler state partially committed.
The replacement operation bridge arrived only after all opcode and interrupt-entry
paths had incremental sequences; it retains a pending operation across budget errors.
Full active-cursor cancellation still requires checkpoint recovery.
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
The resumable interrupt-aware bridge retains CPU-I/O transitions during an in-flight
operation and delivers them at the following instruction boundary. Exact electrical
sampling and final same-time subsystem priority remain unverified.

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
samples into a bounded caller-owned FIFO; a platform adapter transports them. The preferred future PS2 transport is
RFAuds2 behind a PS2-only audio interface. PSP and host retain independent backends.
RFAuds2 never enters `core/`, and its fixed SPU2 output rate never controls SNES time.
The shared reference rate converter is integer-only and consumer-driven: it can
drain queued 32 kHz frames to a bounded 48 kHz PS2 block, but it cannot request
more emulation work, fabricate underrun silence or call a device API. The PS2 adapter
borrows fixed staging storage and performs at most one transport operation per pump.
A launch copies one request; later polls report pending or a completed accepted
prefix. Any ordered tail remains owned by the adapter and is relaunched before more
source PCM is drained. Device pacing therefore cannot become an emulation clock. The
optional RFAuds2 binding is PS2-only and maps exactly this two-phase contract; module
lifecycle and prebuffer/start policy are coordinated by fixed-storage PS2 code. The
IRX is a generated build input, playback starts only after accepted-frame accounting,
and stop/flush/shutdown remain outside core. The bounded diagnostic uses fixed
absolute emulation horizons; target and physical-device behavior remain unverified.

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
- 2026-10-10: RFAuds2 is the selected PS2 PCM transport, isolated behind a platform
  audio adapter and subject to an exact stable pin plus target validation.
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
- 2026-10-22: The complete ten-opcode branch family now exposes operand and internal
  boundaries through shared core phases, including conditional, emulation page-cross,
  BRA and BRL paths; 229 opcode remainders remain without backend-specific behavior.
- 2026-10-22: All sixteen stack push/pull/effective-address opcodes now expose
  width-selected transfers, internal operations, Direct Page pointer reads and
  ordinary-versus-linear stack rules through shared core phases; 213 opcode
  remainders remain without backend-specific or game-specific behavior.
- 2026-10-22: Twenty-seven implied status, register-transfer and register
  increment/decrement opcodes now commit through shared internal phases; `XBA`
  exposes both internal cycles, leaving 186 opcode remainders without backend- or
  game-specific behavior.
- 2026-10-22: All thirty-six accumulator/memory shift/rotate, memory INC/DEC,
  TSB/TRB and STZ opcodes now use shared address/read/modify/write phases, leaving
  150 opcode remainders without backend- or game-specific behavior.
- 2026-10-22: All forty-five `LDA/STA/LDX/LDY/STX/STY` opcodes now use a shared
  resumable operand/address/pointer/data state machine, leaving 107 opcode
  remainders without backend- or game-specific behavior.
- 2026-10-23: The final 101 read/ALU opcodes reuse that shared state machine and
  `RTI/WDM/MVP/MVN/WAI/STP` use dedicated core phases. All 256 opcode values are
  resumable on a complete bus; reset and scheduler cursor wiring remain separate.
- 2026-10-23: RESET now starts through the same caller-owned cursor, with a
  side-effect-free control peek and explicit low/high vector reads. The compatibility
  wrapper remains atomic; unsourced internal/electrical reset cycles and scheduler
  cursor wiring remain separate work.
- 2026-10-23: The opt-in scheduler/5A22 bridge advances each CPU cursor operation
  through requested/event horizons, retains in-flight time across budget failures
  and commits the CPU/bus effect once at the endpoint. RESET has an explicit
  zero-processor-cycle mode; legacy atomic entry points keep their semantics.
  Electrical/dummy phases, refresh, arbitration and mid-instruction ABORT remain open.
- 2026-10-25: The shared S-SMP timer block models the free-running common
  prescaler, 128/16-cycle edges, target divisors, 0-to-1 enable clear and
  four-bit read-clear outputs behind `$F1/$FA-$FF`. Advancement is explicitly
  denominated in S-SMP cycles; real power-on values, DSP-relative phase,
  TEST-register effects and S-CPU/S-SMP synchronization remain open.
- 2026-10-25: Caller-owned SPC700 state now exposes A/X/Y/SP/PSW/PC, bounded
  mapped opcode fetch and atomic dispatch. `NOP`, CLRP/SETP, CLRC/SETC, EI/DI,
  CLRV and NOTC are implemented with documented costs feeding the timer block;
  the other 247 opcode values fail explicitly without architectural commit.
- 2026-10-25: SPC700 register/immediate MOV dispatch now covers A/X/Y immediate
  loads plus A/X/Y/SP transfers, including N/Z semantics, two-byte PC wrap and
  unavailable-operand rollback. The remaining memory-addressed MOV forms stay open.
- 2026-10-26: SPC700 direct-page MOV dispatch now covers A/X/Y loads and stores,
  with PSW.P page selection, mapped-MMIO routing, documented N/Z behavior and
  3/4-cycle costs. Indexed, indirect and absolute MOV addressing remains open.
- 2026-10-26: SPC700 indexed-direct-page MOV dispatch now covers A/Y through X
  and X through Y plus the matching stores. The direct-page sum wraps to eight
  bits inside the P-selected page, mapped MMIO remains visible, and documented
  4/5-cycle costs feed timers. Indirect and absolute MOV addressing remains open.
- 2026-10-26: SPC700 absolute MOV dispatch now covers A/X/Y loads and stores,
  with little-endian 16-bit operands, independent PC wrap, mapped-MMIO routing,
  load-only N/Z effects and documented 4/5-cycle costs. Indexed-absolute and
  indirect MOV addressing remains open.
- 2026-10-26: SPC700 absolute-indexed MOV dispatch now covers A loads and stores
  through X/Y. The effective address wraps in the full 16-bit S-SMP space,
  mapped MMIO remains visible after indexing, and documented 5/6-cycle costs feed
  timers. Register-indirect and direct-indirect MOV addressing remains open.
- 2026-10-26: SPC700 byte MOV coverage now also includes `(X)`/`(X)+`,
  `[dp+X]`/`[dp]+Y`, `MOV dp,#imm` and `MOV dp,dp`; page-selected pointer wrap,
  mapped pointer/target failures, post-increment commit and 3/4/5/6/7-cycle
  costs are host verified. Word MOV and exact bus phases remain open.
- 2026-10-26: one 100-opcode SPC700 block adds 54 accumulator ALU forms, 18
  direct-memory ALU forms, 16 shifts/rotates and 12 byte INC/DEC forms. All 150
  implemented bytes retain deterministic dispatch; exhaustive ADC/SBC vectors,
  mapped-memory failure and timer coupling are host verified.
- 2026-10-26: the remaining 106 SPC700 bytes complete instruction-atomic dispatch
  for all 256 opcodes, including word arithmetic, branches/calls/returns, stack,
  bit, multiply/divide, decimal-adjust and wait-state families. Aggregate 2-12-cycle
  costs and taken deltas feed timers; exact bus phases, interrupt entry and
  synchronized S-SMP/S-DSP execution remain open.
- 2026-10-27: a nominal rational master-to-S-SMP bridge now advances SPC700,
  timers and S-DSP sample cadence from machine time. `$F2/$F3` reach a 128-byte
  S-DSP register foundation with bit-7 write inhibition, `ENDX` clear and internal
  `FLG` reset state. Voices, PCM and exact cross-clock/bus phase behavior remain.
- 2026-10-27: eight S-DSP voice-control records now poll key registers at a
  deterministic alternating-sample phase, latch `DIR`/`SRCN` entries from ARAM
  and complete a five-event key-on preparation state. A pure BRR decoder covers
  all shifts/filters and explicit clamp/wrap arithmetic; streaming and PCM remain.
- 2026-10-27: per-voice BRR transport now decodes four-sample groups into a
  twelve-sample ring, carries predictor history across blocks, follows end/loop
  traversal with `ENDX`, advances unmodulated 14-bit pitch and exposes the future
  Gaussian interpolator's four samples/fraction. Gaussian output, envelopes,
  mixing, echo and PCM remain.
- 2026-10-27: the S-DSP global envelope counter now supplies all 32 documented
  rates and phase offsets. Eleven-bit voice state covers ADSR, Direct GAIN, four
  timed GAIN curves, key-off/reset/end release and `ENVX`; interpolation output,
  envelope multiplication, mixing and PCM remain separate.
- 2026-10-27: the same counter now clocks a shared 15-bit noise LFSR from
  `FLG.NCK`; `NON` exposes that signed source per voice while BRR transport keeps
  running. Logarithmic bulk advancement is equivalent to scalar recurrence;
  output multiplication, mixing and exact DSP microphases remain separate.
- 2026-10-27: resolved `NON` voices now multiply the signed-15 source by the
  current 11-bit envelope, publish `OUTX`, then apply signed left/right voice
  volumes. BRR/Gaussian source resolution, ordered mixing and PCM remain open.
- 2026-10-28: the resolved dry path now sums voices 0 through 7 with signed-16
  saturation after every addition, exposes the included-voice mask, then applies
  signed `MVOLL`/`MVOLR`. BRR voices, echo, mute/DAC and PCM remain unresolved.
- 2026-10-28: the hardware-published 512-entry Gaussian table now resolves BRR
  output with explicit partial wrap/final clamp. BRR and noise envelope output is
  left-aligned signed-15, and sample-level `PMON` uses the preceding voice with
  staged signed shifts, target-`NON` suppression and the `$7fff` position cap.
  Exact DSP microphases, echo, mute/DAC and PCM remain unresolved.
- 2026-10-28: the sample-level echo path now separates `EON` voices, reads the
  16-bit-wrapping `ESA`/`EDL` ARAM ring, runs the documented eight-tap FIR,
  applies `EFB` feedback and protected writes, mixes signed `EVOLL`/`EVOLR`, and
  exposes a `FLG.MUTE`-gated DAC-facing sample. Exact 32-cycle microphases,
  post-DAC polarity, buffering and platform delivery remain unresolved.
- 2026-10-28: an optional caller-owned PCM FIFO now receives typed signed-16
  stereo frames at the native 32 kHz sample cadence. Full queues drop newest and
  count without backpressuring S-DSP time; empty reads return no frame. Resampling,
  platform delivery, target synchronization and audible output remain unresolved.
- 2026-10-29: a bounded integer-linear rate converter now drains the PCM FIFO to
  caller blocks at rates greater than or equal to 32 kHz. Its rational phase,
  signed rounding and starvation behavior are deterministic across chunk sizes;
  device transport, underrun concealment and audible output remain unresolved.
- 2026-10-29: the PS2-only RFAuds2 coordinator now embeds the exact pinned IRX,
  configures fixed storage, starts after accepted prebuffer frames and exposes
  statistics before ordered stop/flush/shutdown. The ROM diagnostic keeps device
  results outside its fixed emulated-time horizons; target execution remains open.
- Changes to these decisions require rationale, tests, status updates and measurements
  if performance is used as justification.
