# 65C816 resumable stack-cycle audit

Date: 2026-10-22

## Scope

This milestone migrates all sixteen stack push, pull and effective-address push
opcodes from the legacy-atomic remainder to the caller-owned peek/commit cursor:

- pushes PHP/PHD/PHA/PHK/PHY/PHB/PHX (`08/0B/48/4B/5A/8B/DA`);
- pulls PLP/PLD/PLA/PLY/PLB/PLX (`28/2B/68/7A/AB/FA`);
- effective-address pushes PER/PEI/PEA (`62/D4/F4`).

Shared phases expose internal operations, M/X-selected widths, high-before-low
writes, low-before-high reads, PER target formation and PEI Direct Page pointer
reads. Ordinary emulation-mode byte transfers wrap within `01xx`; PHD, PLD,
PEA, PEI and PER preserve the documented linear multi-byte exception before the
emulation stack-high invariant is restored. Native-mode transfers use full 16-bit S
wrapping. There is no platform branch and no game-specific behavior.

## Evidence

The implementation is independently authored from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially the stack-relative rules, emulation-mode caveats and Table 5-7 stack
entries 22a-22f. The pre-existing aggregate implementation and tests supplied the
project-local semantic oracle while this block split those effects into resumable
operations.

XERJ `v1.0.0-rc.97` indexed only this MIPSNES2 checkout. Its dry run and index
covered 152 files, all 54 detected code files and 1,179 live records with zero junk
files. Search localized the shared CPU stack helpers, aggregate stack regressions and
new cursor tests; definition lookup found `finish_stack_pull` and the established
stack helpers. Feedback remained disabled. No source from another emulator was
consulted, indexed or copied.

## Deterministic coverage

- exact fetch/internal/write order for every push opcode at its selected width;
- exact fetch/two-internal/read order for every pull opcode at its selected width;
- PHP/PLP status behavior, PLP mode invariants and 8-bit X/Y truncation;
- PLA accumulator-B preservation and PLD/PLA/PLY/PLB/PLX N/Z behavior;
- emulation `01xx` wrapping, linear multi-byte exceptions and native 16-bit wrap;
- PEA operand reads and five-cycle sequence;
- signed PER target from the post-operand PC and six-cycle sequence;
- PEI aligned Direct Page wrapping, unaligned-D carry and the DL penalty cycle;
- stale consecutive-internal descriptor rejection before any additional effect;
- missing-write-bus fallback before operand reads or stack mutation;
- unchanged aggregate behavior across all 256 opcode values.

## Deliberate limits

The other 213 opcode remainders and reset remain legacy-atomic. Internal descriptors
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
