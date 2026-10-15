# Testing and verification

## Host tests

Run `make test`. The contract test checks logical-address masks, all bank/offset
combinations and logging sink semantics. A second executable compiles with logging
disabled and verifies callbacks are absent. `test-cpu-bus` checks the reference bus,
reset/vector foundation, every implemented opcode, widths/modes/flags, logical
addresses, linear data bank crossing, indexed penalties, base cycles and unknown-
opcode diagnostics. Direct Page regressions additionally cover bank-zero selection,
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
Binary immediate arithmetic regressions execute ADC/SBC at both M widths. They cover
carry/borrow input, carry/no-borrow output, signed overflow in both directions,
selected-width N/Z, B and unrelated-status preservation, PC/cycles, and the explicit
sticky D=1 diagnostic stop before operand fetch.
Binary Direct Page/absolute arithmetic regressions execute all four new opcodes at
both M widths. They cover bank-zero Direct word wrap, aligned/nonzero-DL timing,
absolute DBR and linear bank crossing, selected-width flags/B, PC/cycles and the
pre-operand decimal diagnostic stop across all recognized ADC/SBC forms.
Binary indexed arithmetic regressions execute ADC/SBC Direct-X and absolute-X/Y at
both M widths. They cover bank-zero Direct wrap, nonzero-DL timing, absolute page and
bank carry, the shared page-cross-or-16-bit-index cycle, selected-width flags/B,
PC/cycles and the pre-operand decimal diagnostic stop across all twelve recognized
ADC/SBC forms.
Binary stack-relative arithmetic regressions execute ADC/SBC `offset,S` and
`(offset,S),Y` at both M widths. They cover full 16-bit S-based address formation,
bank-zero word/pointer wrap, DBR plus Y carry, native/emulation behavior,
selected-width flags/B, fixed 4/7-cycle bases and the pre-operand decimal diagnostic
stop across all sixteen recognized ADC/SBC forms.
Binary regular Direct-indirect arithmetic regressions execute ADC/SBC `(direct,X)`,
`(direct),Y` and `(direct)` at both M widths. They cover bank-zero pointer wrap, DBR
selection, pre/post-indexing, Y bank carry, DL/M/X/page cycle additions, selected-
width flags/B, PC/cycles and the pre-operand decimal diagnostic stop across all
twenty-two recognized ADC/SBC forms.
The host smoke script checks the honest banner and rejects ROM/unknown arguments. The
repository checker verifies
required files, local Markdown links, allowed checkbox syntax, license hash, action
pins and forbidden core includes. It is a guardrail, not proof of complete emulation.

CPU expectations are independently authored from the WDC W65C816S March 13, 2024
datasheet sections recorded in [65c816.md](65c816.md). No external test ROM or code
from an emulator is included.

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

## End-of-task audit

Read STATUS.md; compare to code; run relevant tests/builds; scan TODO/FIXME/placeholders;
check core/platform dependency direction; review docs/links and completion claims;
update status and affected architecture/performance/compatibility records; inspect CI.
Leave unavailable runtime, performance, release and admin work unchecked with reasons.
