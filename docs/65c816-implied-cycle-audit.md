# 65C816 resumable implied-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates twenty-seven implied status, register-transfer and register
increment/decrement opcodes from the legacy-atomic remainder to the caller-owned
peek/commit cursor:

- CLC/SEC/CLI/SEI/CLV/CLD/SED (`18/38/58/78/B8/D8/F8`);
- TCS/TSC/TCD/TDC/TXA/TYA/TXS/TXY/TAY/TAX/TSX/TYX
  (`1B/3B/5B/7B/8A/98/9A/9B/A8/AA/BA/BB`);
- INC A/DEC A/DEY/INY/DEX/INX (`1A/3A/88/C8/CA/E8`);
- XBA/XCE (`EB/FB`).

The two-cycle instructions use one shared internal phase. Three-cycle XBA uses two
distinct internal tokens so a descriptor from the first internal operation cannot be
committed as the second. Architectural effects occur only on the final internal
commit. Effect-only helpers are shared with the existing aggregate implementation;
there is no platform branch and no game-specific behavior.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially its opcode table, programming model, mode rules and timing table. The
pre-existing aggregate implementation and tests supplied the project-local semantic
oracle while this block exposed the already-defined effects at resumable boundaries.

XERJ `v1.0.0-rc.97` indexed only this MIPSNES2 checkout. Its dry run and index
covered 154 files, all 54 detected code files and 1,198 live records with zero junk
files. Search localized the implied-family cursor tests and shared CPU implementation;
definition lookup found `finish_implied_operation`. Feedback remained disabled. No
source from another emulator was consulted, indexed or copied.

## Deterministic coverage

- exact opcode-read/internal sequence and two-cycle total for every two-cycle member;
- exact opcode-read/two-distinct-internal sequence and three-cycle total for XBA;
- all seven isolated status-bit changes with unrelated bits preserved;
- every accumulator, index, Direct and stack transfer opcode;
- M/X-selected transfer widths, accumulator-B preservation and 8/16-bit N/Z rules;
- emulation/native stack-high behavior for TCS/TXS and XCE mode invariants;
- all six accumulator/index increment/decrement opcodes at selected widths;
- XBA byte exchange and low-byte N/Z behavior independent of M;
- stale first-XBA-internal descriptor rejection before any additional register or
  cycle effect;
- unchanged aggregate behavior across all 256 opcode values.

## Deliberate limits

The other 186 opcode remainders and reset remain legacy-atomic. Internal descriptors
do not claim a dummy read, VPA/VDA values or electrical address-bus state. This block
does not add mid-instruction `ABORT`, cursor cancellation, DMA/HDMA ownership or
scheduler event-budget propagation. The central timed-step bridge remains atomic.

## Completion criterion

This block is complete when deterministic host tests prove every property above,
debug/release/sanitizer suites and the static analyzer pass, repository checks pass,
and the unchanged shared core compiles first in the pinned PS2 toolchain and then in
the pinned PSP toolchain. Console compilation is not a runtime or performance claim.

## Verification

Publication identifiers and final command results are recorded after the exact
tested tree is committed and published.
