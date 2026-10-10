# Testing and verification

## Host tests

Run `make test`. The contract test checks logical-address masks, all bank/offset
combinations and logging sink semantics. A second executable compiles with logging
disabled and verifies callbacks are absent. `test-cpu-bus` checks the reference bus,
reset/vector foundation, every implemented opcode, widths/modes/flags, logical
addresses, base cycles and unknown-opcode diagnostics. The host smoke script checks
the honest banner and rejects ROM/unknown arguments. The repository checker verifies
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
The authoritative initial record is [bootstrap-audit.md](bootstrap-audit.md).

## End-of-task audit

Read STATUS.md; compare to code; run relevant tests/builds; scan TODO/FIXME/placeholders;
check core/platform dependency direction; review docs/links and completion claims;
update status and affected architecture/performance/compatibility records; inspect CI.
Leave unavailable runtime, performance, release and admin work unchecked with reasons.
