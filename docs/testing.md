# Testing and verification

## Host tests

Run `make test`. The contract test checks logical-address masks, all bank/offset
combinations and logging sink semantics. A second executable compiles with logging
disabled and verifies callbacks are absent. `test-ps2-diagnostic` runs the five
embedded PS2 diagnostic cases through the same platform-neutral suite linked into
the target ELF; the native screen path still requires an ARMSX2 or hardware run.
`test-cpu-bus` checks the reference bus,
reset/vector foundation, all 256 opcode values, widths/modes/flags, logical
addresses, linear data bank crossing, indexed penalties and base cycles. Direct Page regressions additionally cover bank-zero selection,
aligned emulation-mode page wrapping, unaligned-D carry, 8/16-bit transfers and the
DL cycle penalty. Direct-indirect regressions distinguish 16-bit pointer page wrapping
from the 24-bit long-pointer linear-increment exception, verify DBR versus pointer-bank
selection, pre/post indexing, full-address carry and indexed-read penalties. The host
register-transfer regressions cover emulation, native and mixed M/X widths, including
destination high-byte rules, zero extension, N/Z and exact cycles. Special-transfer
regressions cover the fixed-width C/D/S paths, emulation/native stack-high rules,
8/16-bit TSX/TXS and XBA byte exchange. Branch regressions execute all eight
conditional opcodes in taken and non-taken paths plus BRA and BRL; they cover signed
offsets, PBR-local 16-bit PC wrapping, unchanged status and exact 2/3/4-cycle rules.
Status-control regressions cover CLD/SED, CLI/SEI and CLV in emulation and native
modes, including exact unrelated-flag preservation, PC movement and two-cycle timing.
Register increment/decrement regressions cover accumulator and both index registers
at 8/16-bit widths, wraparound, B preservation, index zero extension, N/Z isolation,
PC movement and exact two-cycle timing.
Logic/compare regressions cover ORA/AND/EOR/CMP immediate, Direct and absolute forms;
all CPX/CPY and BIT opcodes; native 8/16-bit and an emulation-width case; M/X-selected
operands; N/Z/C/V isolation; Direct Page DL penalties; indexed BIT penalties; and
16-bit data reads crossing a bank boundary.
Extended accumulator-ALU regressions exercise all 40 non-stack-relative indexed,
indirect and long ORA/AND/EOR/CMP opcodes at both M widths. They verify DBR versus
long-pointer banks, pre/post indexing, Direct-X emulation wrapping, page and bank
carry, DL/M/X cycle additions, PC lengths and B/flag/operand preservation.
Stack-relative accumulator-ALU regressions execute those eight ORA/AND/EOR/CMP
opcodes at both M widths. They cover full 16-bit `S + offset` formation in native and
emulation modes, bank-zero operand and pointer wrapping, DBR plus Y carry across a
bank boundary, result/compare flags, operand preservation, exact 4/7-cycle bases and
the absence of Direct Page, page-cross or X-width penalties.
Stack-relative accumulator load/store regressions execute all four LDA/STA forms at
both M widths. They cover 8-bit B preservation, LDA N/Z isolation, STA status
preservation, bank-zero 16-bit load/store wrap, DBR plus Y carry, emulation-mode full
S addressing, PC movement and exact 4/7-cycle bases plus the M=0 width cycle.
ADC/SBC regressions execute all thirty addressing forms at both M widths. They cover
carry/borrow input, carry/no-borrow output, signed overflow, selected-width N/Z, B
and unrelated-status preservation, Direct Page and stack-relative bank-zero rules,
DBR and long-pointer bank selection, pre/post indexing, 24-bit carry, PC movement and
exact width/DL/indexed cycle additions.
Decimal regressions exhaust 40,000 valid 8-bit packed-BCD ADC/SBC operand/carry
combinations, add targeted 16-bit and invalid-digit cases, and execute all thirty
addressing forms at both M widths with D=1. A separate read-only audit compares the
algorithm's result and C/V/N/Z behavior against 19,970 decimal immediate vectors
from a pinned MIT-licensed SingleStepTests revision.
Read-modify-write regressions execute all 36 ASL/LSR/ROL/ROR accumulator/memory,
memory INC/DEC, TSB/TRB and STZ opcodes at both M widths. They verify selected-width
results, C/N/Z and unrelated-flag isolation, B preservation, original-operand
TSB/TRB Z behavior, Direct Page bank/DL rules, DBR and 24-bit indexed carry, 16-bit
writes, aggregate timing and missing-write-bus diagnostics. Bus-cycle dummy writes
and MLB signaling remain outside the current contract.
Stack/control regressions execute every PHP/PHD/PHA/PHK/PHY/PHB/PHX, PLP/PLD/PLA/
PLY/PLB/PLX, PEA/PEI/PER, JMP/JML, JSR/JSL and RTS/RTL form. They cover emulation
and native modes, M/X-selected 8/16-bit width, high/low byte order, status and N/Z,
ordinary `01xx` wrapping, WDC's linear multi-byte stack exceptions, PC/PBR return
rules, Direct Page pointer wrapping and DL timing, indirect-table bank selection,
aggregate cycles and missing-write-bus stops. A tracing bus verifies the meaningful
operand/stack/pointer callback order and count for JSL and JSR `(absolute,X)`; the
current contract still omits internal dummy reads and signal-level bus cycles.
Final-dispatch regressions cover BRK/COP/RTI/WDM/MVP/MVN/WAI/STP, software and
hardware interrupt vectors, native/emulation stack layouts, signature bytes,
block-move repetition and index wrapping, IRQ/NMI wake/priority behavior, STP reset
release, meaningful bus order and aggregate cycles.
Boundary-visible ABORT regressions cover emulation/native vectors, stack byte order,
PBR and status handling, seven/eight-cycle aggregates, ABORT-over-NMI/IRQ priority,
lower-priority NMI retention, WAI no-wake/restart-address behavior, STP/reset handling,
no opcode fetch before entry and missing-write-bus diagnostics. They intentionally do
not claim mid-instruction register rollback, write suppression or exact signal-cycle
sampling.
The separate `test-cpu-cycles` executable verifies the caller-owned resumable
operation contract. It covers side-effect-free repeated peek, exact-once commit,
stale-descriptor rejection, opcode fetch, missing-write legacy fail-stop preflight,
`NOP`, the
complete `LDA/STA/LDX/LDY/STX/STY` family, `REP`, `SEP`, the conditional/relative branch
family, all five `JMP/JML` forms, absolute and indexed-indirect `JSR`, `RTS`, `JSL`,
`RTL`, `BRK`, `COP`, all sixteen stack push/pull/effective-address opcodes and all
twenty-seven implied status/transfer/register increment-decrement opcodes,
all four accumulator shifts/rotates and all thirty-two memory ASL/LSR/ROL/ROR,
INC/DEC, TSB/TRB and STZ opcodes at both M widths,
all forty-five `LDA/STA/LDX/LDY/STX/STY` opcodes at both selected widths through
their immediate, Direct, stack-relative, indirect, absolute and long paths,
all 101 `ORA/AND/EOR/ADC/SBC/CMP/CPX/CPY/BIT` opcodes through those shared operand
paths, exact `RTI/WDM/MVP/MVN/WAI/STP` sequences, and an exhaustive 256-value run
that rejects `LEGACY_ATOMIC` anywhere on a complete bus,
including taken/not-taken and emulation page-cross branch
paths, direct and indirect jump bank rules, long-subroutine PBR, bank-local pointer
wrapping, stack transfer widths, Direct Page pointer wrapping, ordinary `01xx`
stack wrapping and linear multi-byte exceptions;
8/16-bit M behavior; emulation invariants; program-bank PC wrap; 24-bit linear store
crossing; flag commit position; subroutine and interrupt stack order; boundary
`ABORT`/`NMI`/`IRQ` priority; logical addresses; access order/count; and interrupt
arrival between boundary peek/commit. It does not claim signal-level reset, exact
dummy-cycle placement or complete mid-instruction `ABORT` recovery.
`test-cpu-differential` drives every opcode through both the aggregate instruction
API and the resumable cursor from four deterministic valid states. Its 1,024 cases
compare architectural state, result, cycle count and the exact ordered bus trace.
This guards consistency between the two core entry points; their shared helpers mean
it is not independent hardware-conformance evidence.
The scheduler regression executable covers absolute master-clock events, deterministic
timestamp/priority/insertion ordering, cancellation, capacity, callback insertion,
dispatch budgets, reentrant-run rejection, event horizons, exact rational conversion,
overflow handling, chunked/one-shot drift equivalence and 5A22 6/8/12-clock address
classification. An integration fixture schedules simultaneous IRQ/NMI inputs for a
waiting CPU and verifies explicit event priority followed by the CPU's NMI priority.
The SNES-bus regression executable covers 128 KiB WRAM, low-bank mirrors, WRAM
ports, MMIO callback ranges, cartridge fallthrough, `$420D`, FastROM timing,
CPU-I/O state/fallback routing, 24-bit normalization and counter overflow. The
CPU-I/O regression executable independently covers NMITIMEN masks and transitions,
9-bit H/V compare storage, RDNMI/TIMEUP acknowledgement, HVBJOY inputs, open-bus bit
composition, distinct VBlank/NMI-flag inputs, multiplication/division registers,
old-result visibility, exact 8/16-cycle completion, divide-by-zero and invalid/
unimplemented accesses. It also covers four caller-owned controller inputs,
snapshot isolation, `$4218-$421F`, read-only writes and enable/complete transitions.
Manual-port cases cover live high-strobe B bits, falling-edge snapshots, independent
port clocks, all 16 bits on all four lines, post-word connected padding, open/fixed
return bits and deterministic coexistence with automatic polling. The
programmable-I/O cases cover the `FF` initialization contract, write-only reads,
wired-AND external levels, read-only writes, bit-7 falling edges and one-shot edge
consumption. The
H/V regression executable covers non-interlaced NTSC/PAL scanline/frame totals,
HBlank/VBlank boundaries, overscan, field toggling, the NTSC short line, delayed NMI
assertion, every valid HTIME value, V/HV match positions, short/final-line exceptions,
out-of-range compare values, exact-boundary enabling, TIMEUP acknowledgement, overflow
rejection, exact 4224-clock automatic-joypad timing, normal/overscan start lines,
late enable, disable while busy and one-shot/chunked determinism. Timed CPU fixtures verify a slow NOP,
mixed FastROM-fetch/slow-WRAM LDA, an event timestamped inside an atomic instruction,
emulation-mode NMI entry and visible bridge errors. The master-domain executable
checks scheduler/H/V timestamp agreement, H/V-before-callback tie ordering, dispatch-
budget exhaustion, same-time insertion, invalid/reentrant paths and chunk equivalence.
The interrupt-line executable verifies source independence, CPU-I/O timer and NMI
sampling, one-shot NMI transfer and aggregate CPU-level driving.
The SNES-bus fixture additionally advances repeated NOPs through the H/V-aware bridge,
verifying clock lockstep and timer-latch assertion. Its interrupt-aware cases verify
post-step timer delivery, TIMEUP acknowledgement without erasing cartridge IRQ,
source deassertion, NMI delivery and simultaneous NMI/IRQ priority at the next atomic
CPU boundary. They also verify CPU-bus/internal-cycle math progress, completion-read
ordering, timing-overflow transactionality and an executed multiply sequence. These
fixtures validate meaningful-access timing, but do not cover missing dummy cycles,
complete SNES interrupt generation or complete cartridge mapping. The DMA timing
fixture separately verifies resumable WRAM refresh pauses for CPU and general DMA,
normal-line H=538/H=534 alignment, NTSC short-line phase retention, horizon
boundaries, exact-once byte effects and separate CPU/DMA/refresh clock totals.
The pure HDMA fixture verifies frame initialization, channel priority, direct and
indirect tables, repeat/wait counters, enable-mask pause/resume, pointer and port
wrapping, both directions and termination. Scheduler timing is deliberately not
claimed by that fixture.
The PPU-counter executable enumerates every master-clock position on normal and NTSC
short lines, verifies all valid H/V capture values, independent low/high selectors,
PPU2 open bits, WRIO gating and field/region/version composition. Bus integration
enumerates all 128 system-bank mirrors, checks fallback and ignored writes, consumes
WRIO/external edges and rejects inconsistent H/V-domain wiring. Its executed LDA
fixture explicitly verifies the current pre-instruction snapshot approximation.
The cartridge regression executable covers explicit base LoROM/HiROM and documented
Mode 25/35 ExHiROM ROM windows,
power-of-two mirrors, unambiguous larger-region/smaller-tail repetition, mapper-bank
alignment and padding-dependent rejection, standard SRAM windows, invalid
configurations, ROM write protection, declined slot accesses and an integrated
reset/NOP fetch through base LoROM and ExHiROM. Generated byte arrays are used; it
also verifies revisioned SRAM dirty-state handoff, same-value and mirrored writes,
stale/matching acknowledgement and revision wrap. It does not test file loading,
durable save writes, unofficial ExLoROM or physical board-specific decoding.
The cartridge-image regression executable covers clean and copier-prefixed view
policies, malformed sizes, LoROM/HiROM/ExHiROM header offsets, raw 16-byte
registration fields, normalized
whole-image checksum calculation, unambiguous smaller-tail mirroring, diagnostic
zero-padding, conservative base-map selection, every selection gate, explicit
no-match/ambiguity, candidate bounds, one selected irregular LoROM-to-mapper fixture
and one selected 6 MiB ExHiROM-to-mapper fixture. It does not resolve special board
headers or prove physical cartridge wiring.
The cartridge-preparation regression executable verifies the allocation-free path
from caller-owned clean/prefixed bytes through conservative selection, declared SRAM
capacity and executable LoROM/ExHiROM construction. It checks unchanged outputs on
failure and rejects malformed/tampered inputs. It performs no platform file I/O.
The host smoke script checks the honest banner and rejects ROM/unknown arguments. The
repository checker verifies
required files, local Markdown links, allowed checkbox syntax, license hash, action
pins and forbidden core includes. It is a guardrail, not proof of complete emulation.

CPU expectations are independently authored from the WDC W65C816S March 13, 2024
datasheet sections recorded in [65c816.md](65c816.md). No external test ROM or code
from an emulator is included.
Use the source hierarchy and provenance rules in [references.md](references.md)
before adopting additional hardware documentation, vectors or test ROMs.

Tests use explicit return codes so `-DNDEBUG` cannot remove checks. Host debug,
release and sanitizer builds belong in separate directories. Cross-build success
is not console execution; a foundation binary is not a test-ROM runner.

## Future layers and evidence gates

| Layer | Required evidence |
|---|---|
| CPU | Opcode/mode/flag/addressing/interrupt edge cases with sourced expectations |
| Bus/cartridge | Synthetic mapping/mirror/MMIO/malformed-input fixtures |
| PPU | State/port/priority/window/color and rendering reference vectors |
| SPC700/DSP | Complete opcode/flag/addressing matrices, deterministic cadence coupling, key/source state, BRR/ring/traversal, ADSR/GAIN, noise, all Gaussian coefficients/arithmetic, left-aligned `OUTX`, PMON, ordered dry/echo saturation, FIR/feedback, mute, signed volume, bounded PCM vectors and the fixed 65,536-frame offline hash regression |
| DMA/scheduler | Transfer boundaries, arbitration, event ties, drift and determinism |
| Serialization | Round-trip state, version and malformed-input checks |
| Optimized paths | Differential outputs/state plus real-target before/after timings |
| Integration | Legally sourced test ROMs with provenance and reproducible input |
| Compatibility | Game/region/revision, reproduced scenes, real-device identity |
| CI | Actual successful run, inspected outputs and checksum validation |

Do not commit commercial ROMs or undocumented fixtures. List redistributable ROM
license, author/source URL, revision/hash and expected behavior before inclusion.
No external test-ROM corpus is bundled yet. Fuzz only documented parsers/state
interfaces once they exist, and keep regression inputs legally redistributable.

## Evidence record

Record date, revision or audited tree scope, command, working directory, compiler/
SDK version, flags, exit code, meaningful test output, failures and unavailable
checks. Hardware evidence includes model/settings, test duration and observations.
Never promote `[x]` based on an agent's intention or a file's existence alone.
The authoritative initial record is [bootstrap-audit.md](bootstrap-audit.md). CPU
milestone evidence is recorded in [65c816-foundation-audit.md](65c816-foundation-audit.md)
and [65c816-addressing-audit.md](65c816-addressing-audit.md). Direct Page milestone
evidence is recorded in [65c816-direct-page-audit.md](65c816-direct-page-audit.md).
Direct-indirect milestone evidence is recorded in
[65c816-direct-indirect-audit.md](65c816-direct-indirect-audit.md).
Register-transfer milestone evidence is recorded in
[65c816-register-transfer-audit.md](65c816-register-transfer-audit.md).
Special-transfer completion evidence is recorded in
[65c816-special-transfer-audit.md](65c816-special-transfer-audit.md).
Branch-family completion evidence is recorded in
[65c816-branch-audit.md](65c816-branch-audit.md).
Status-control milestone evidence is recorded in
[65c816-status-control-audit.md](65c816-status-control-audit.md).
Register increment/decrement milestone evidence is recorded in
[65c816-increment-decrement-audit.md](65c816-increment-decrement-audit.md).
Logic/compare foundation evidence is recorded in
[65c816-logic-compare-audit.md](65c816-logic-compare-audit.md).
Extended ORA/AND addressing evidence is recorded in
[65c816-ora-and-audit.md](65c816-ora-and-audit.md).
Extended EOR/CMP addressing evidence is recorded in
[65c816-eor-cmp-audit.md](65c816-eor-cmp-audit.md).
Stack-relative accumulator-ALU evidence is recorded in
[65c816-stack-relative-audit.md](65c816-stack-relative-audit.md).
Stack-relative LDA/STA evidence is recorded in
[65c816-stack-relative-load-store-audit.md](65c816-stack-relative-load-store-audit.md).
Binary immediate ADC/SBC evidence is recorded in
[65c816-binary-arithmetic-audit.md](65c816-binary-arithmetic-audit.md).
Binary Direct Page/absolute ADC/SBC evidence is recorded in
[65c816-binary-direct-absolute-audit.md](65c816-binary-direct-absolute-audit.md).
Binary indexed ADC/SBC evidence is recorded in
[65c816-binary-indexed-audit.md](65c816-binary-indexed-audit.md).
Binary stack-relative ADC/SBC evidence is recorded in
[65c816-binary-stack-relative-audit.md](65c816-binary-stack-relative-audit.md).
Binary regular Direct-indirect ADC/SBC evidence is recorded in
[65c816-binary-direct-indirect-audit.md](65c816-binary-direct-indirect-audit.md).
Binary long ADC/SBC evidence is recorded in
[65c816-binary-long-audit.md](65c816-binary-long-audit.md).
Decimal ADC/SBC evidence is recorded in
[65c816-decimal-arithmetic-audit.md](65c816-decimal-arithmetic-audit.md).
Memory RMW and shift/rotate evidence is recorded in
[65c816-memory-rmw-shifts-audit.md](65c816-memory-rmw-shifts-audit.md).
Stack push/pull and subroutine-control evidence is recorded in
[65c816-stack-control-audit.md](65c816-stack-control-audit.md).
Resumable stack push/pull/effective-address evidence is recorded in
[65c816-stack-cycle-audit.md](65c816-stack-cycle-audit.md).
Resumable implied status/transfer/register evidence is recorded in
[65c816-implied-cycle-audit.md](65c816-implied-cycle-audit.md).
Resumable load/store-family evidence is recorded in
[65c816-load-store-cycle-audit.md](65c816-load-store-cycle-audit.md).
Complete opcode-dispatch and interrupt-boundary evidence is recorded in
[65c816-complete-opcodes-audit.md](65c816-complete-opcodes-audit.md).
Boundary-visible ABORT evidence is recorded in
[65c816-abort-foundation-audit.md](65c816-abort-foundation-audit.md).
Scheduler and 5A22 timing-foundation evidence is recorded in
[scheduler-foundation-audit.md](scheduler-foundation-audit.md).
Partial SNES bus and timed CPU-step evidence is recorded in
[snes-bus-timing-audit.md](snes-bus-timing-audit.md).
Ricoh 5A22 CPU-I/O register/latch evidence is recorded in
[cpu-io-foundation-audit.md](cpu-io-foundation-audit.md).
Non-interlaced H/V timing and blank/NMI transition evidence is recorded in
[hv-timing-foundation-audit.md](hv-timing-foundation-audit.md).
Stable-register H/V timer-match evidence is recorded in
[hv-irq-foundation-audit.md](hv-irq-foundation-audit.md).
Shared scheduler/H/V master-clock fanout evidence is recorded in
[master-timing-fanout-audit.md](master-timing-fanout-audit.md).
Atomic CPU-I/O interrupt handoff evidence is recorded in
[interrupt-handoff-audit.md](interrupt-handoff-audit.md).
Ricoh 5A22 multiplication/division register evidence is recorded in
[5a22-math-register-audit.md](5a22-math-register-audit.md).
Automatic controller-read evidence is recorded in
[auto-joypad-audit.md](auto-joypad-audit.md).
Manual standard-controller serial-port evidence is recorded in
[manual-joypad-audit.md](manual-joypad-audit.md).
Programmable I/O-port evidence is recorded in
[5a22-io-port-audit.md](5a22-io-port-audit.md).
Non-interlaced PPU counter-register evidence is recorded in
[hv-counter-latch-audit.md](hv-counter-latch-audit.md).
The rejected in-callback scheduler experiment and its required successor contract
are recorded in [cpu-access-timing-postmortem.md](cpu-access-timing-postmortem.md).
Base LoROM/HiROM evidence is recorded in
[cartridge-foundation-audit.md](cartridge-foundation-audit.md).
Image/header analysis evidence is recorded in
[cartridge-header-analysis-audit.md](cartridge-header-analysis-audit.md).
Checksum/base-selection evidence is recorded in
[cartridge-mapper-selection-audit.md](cartridge-mapper-selection-audit.md).
Executable irregular-map evidence is recorded in
[cartridge-irregular-mapping-audit.md](cartridge-irregular-mapping-audit.md).
Mode 25/35 ExHiROM evidence is recorded in
[cartridge-exhirom-audit.md](cartridge-exhirom-audit.md).
Cartridge preparation/construction evidence is recorded in
[cartridge-construction-audit.md](cartridge-construction-audit.md).
SRAM persistence-handoff evidence is recorded in
[cartridge-sram-persistence-audit.md](cartridge-sram-persistence-audit.md).

## End-of-task audit

Read STATUS.md; compare to code; run relevant tests/builds; scan TODO/FIXME/placeholders;
check core/platform dependency direction; review docs/links and completion claims;
update status and affected architecture/performance/compatibility records; inspect CI.
Leave unavailable runtime, performance, release and admin work unchecked with reasons.
