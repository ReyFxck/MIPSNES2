# Testing and verification

## Host tests

Run `make test`. The contract test checks logical-address masks, all bank/offset
combinations and logging sink semantics. A second executable compiles with logging
disabled and verifies callbacks are absent. `test-cpu-bus` checks the reference bus,
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
The scheduler regression executable covers absolute master-clock events, deterministic
timestamp/priority/insertion ordering, cancellation, capacity, callback insertion,
dispatch budgets, reentrant-run rejection, event horizons, exact rational conversion,
overflow handling, chunked/one-shot drift equivalence and 5A22 6/8/12-clock address
classification. An integration fixture schedules simultaneous IRQ/NMI inputs for a
waiting CPU and verifies explicit event priority followed by the CPU's NMI priority.
The SNES-bus regression executable covers 128 KiB WRAM, low-bank mirrors, WRAM
ports, MMIO callback ranges, cartridge fallthrough, `$420D`, FastROM timing,
24-bit normalization and counter overflow. Timed CPU fixtures verify a slow NOP,
mixed FastROM-fetch/slow-WRAM LDA, an event timestamped inside an atomic instruction,
emulation-mode NMI entry and visible bridge errors. They validate meaningful-access
timing, not missing dummy cycles, mid-instruction pause/resume, WRAM refresh, complete
SNES interrupt generation or complete cartridge mapping.
The cartridge regression executable covers explicit base LoROM/HiROM and documented
Mode 25/35 ExHiROM ROM windows,
power-of-two mirrors, unambiguous larger-region/smaller-tail repetition, mapper-bank
alignment and padding-dependent rejection, standard SRAM windows, invalid
configurations, ROM write protection, declined slot accesses and an integrated
reset/NOP fetch through base LoROM and ExHiROM. Generated byte arrays are used; it
does not test file loading, unofficial ExLoROM or physical board-specific decoding.
The cartridge-image regression executable covers clean and copier-prefixed view
policies, malformed sizes, LoROM/HiROM/ExHiROM header offsets, raw 16-byte
registration fields, normalized
whole-image checksum calculation, unambiguous smaller-tail mirroring, diagnostic
zero-padding, conservative base-map selection, every selection gate, explicit
no-match/ambiguity, candidate bounds, one selected irregular LoROM-to-mapper fixture
and one selected 6 MiB ExHiROM-to-mapper fixture. It does not resolve special board
headers or prove physical cartridge wiring.
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
| SPC700/DSP | Instruction/timer and sample-level independent vectors |
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
Complete opcode-dispatch and interrupt-boundary evidence is recorded in
[65c816-complete-opcodes-audit.md](65c816-complete-opcodes-audit.md).
Scheduler and 5A22 timing-foundation evidence is recorded in
[scheduler-foundation-audit.md](scheduler-foundation-audit.md).
Partial SNES bus and timed CPU-step evidence is recorded in
[snes-bus-timing-audit.md](snes-bus-timing-audit.md).
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

## End-of-task audit

Read STATUS.md; compare to code; run relevant tests/builds; scan TODO/FIXME/placeholders;
check core/platform dependency direction; review docs/links and completion claims;
update status and affected architecture/performance/compatibility records; inspect CI.
Leave unavailable runtime, performance, release and admin work unchecked with reasons.
