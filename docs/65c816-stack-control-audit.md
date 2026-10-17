# 65C816 stack and subroutine-control audit

Date: 2026-10-17

## Scope

This milestone adds the 25 stack push/pull, effective-address push and subroutine-
control opcodes that were absent from the 223-opcode reference interpreter:

- stack/status/register: PHP `08`, PHD `0B`, PLP `28`, PLD `2B`, PHA `48`, PHK
  `4B`, PHY `5A`, PLA `68`, PLY `7A`, PHB `8B`, PLB `AB`, PHX `DA`, PLX `FA`;
- effective-address pushes: PER `62`, PEI `D4`, PEA `F4`;
- jump/call/return: JSR absolute `20`, JSL absolute long `22`, JMP absolute long
  `5C`, RTS `60`, RTL `6B`, JMP absolute indirect `6C`, JMP absolute-indexed-
  indirect `7C`, JMP absolute-indirect-long `DC`, JSR absolute-indexed-indirect
  `FC`.

The interpreter now dispatches 248 opcodes. BRK, COP, RTI, WDM, MVP, MVN, WAI and
STP remain unimplemented and still enter the existing sticky diagnostic stop. This
milestone does not add interrupt handling, block moves, scheduler timing, PPU/APU,
platform optimization or target audio integration.

## Sources and independent implementation

The behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.9-2.11 and 3.2/3.4 for PBR, PC and bank-zero stack ownership;
- sections 3.5.5, 3.5.19-3.5.22 for indirect, relative-long and stack addressing;
- opcode matrix 5-4, operation table 5-5 and bus-cycle table 5-7 for instruction
  length, aggregate cycles, data order and meaningful bus accesses;
- caveats 7.1 and 7.9 for emulation-mode multi-byte stack exceptions and indirect-
  table bank selection;
- caveats 7.10 and 7.20-7.21 for E/M/X invariants and fixed/selected transfer widths.

No emulator implementation or commercial ROM was used or copied. Expectations are
encoded as original deterministic host tests.

## Verified behavior

The host regressions cover:

- logical bank-zero stack addresses, high-then-low pushes and low-then-high pulls;
- ordinary emulation-mode `01xx` wrapping and native 16-bit S wrapping;
- WDC's linear multi-byte exceptions for JSL, JSR `(a,X)`, PEA, PEI, PER, PHD,
  PLD and RTL, including accesses outside `01xx` before `SH=01` is restored;
- M-selected PHA/PLA and X-selected PHX/PLX/PHY/PLY at native 8/16-bit widths,
  plus emulation width enforcement and accumulator-B preservation;
- PLP mode invariants, PLB/PLD/PLA/PLX/PLY N/Z effects and unrelated-state
  preservation;
- PER signed target formation from the advanced PC, PEI Direct Page wrapping and
  nonzero-DL cycle addition, and PEA immediate word order;
- absolute, long, bank-zero indirect and PBR-indexed-indirect target formation;
- JSR/JSL saved return addresses, RTS/RTL increment, PBR preservation/restoration,
  instruction lengths and aggregate cycle totals;
- meaningful callback order/count for JSL and JSR `(a,X)` operand, stack and pointer
  accesses; internal dummy reads and signal-level cycles remain outside the current
  bus contract;
- missing write callbacks stop push/call instructions after opcode fetch and before
  operand fetch, without changing S or cycles.

## Local verification

Working directory: repository root. Compiler: `cc (Ubuntu
13.3.0-6ubuntu2~24.04) 13.3.0`; Python `3.12.14`.

- `make clean && make test` — passed after implementation; `test-cpu-bus` reported
  `PASS: logical bus and 65C816 248-opcode subset`.
- `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release
  CFLAGS='-O3 -DNDEBUG' test` — initial run compiled successfully, then the repository
  checker correctly failed because this audit file had not yet been created.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' test` — initial run compiled successfully,
  then stopped at the same missing audit-link check. This was a documentation-order
  failure, not a sanitizer finding.

Final release, sanitizer, repository-consistency and cross-build results are recorded
below:

- `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release
  CFLAGS='-O3 -DNDEBUG' test` — final rerun passed every compile, repository check,
  contract test, 248-opcode CPU/bus test and host smoke test.
- `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make
  BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined
  -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test` — final
  rerun passed with no ASan or UBSan diagnostic. LeakSanitizer was disabled because
  the local runtime's documented `/proc` restriction remains unchanged.
- `python3 tools/check_repository.py` — passed after all documentation changes.
- `git diff --check` — passed.

Local console cross-build attempts were not possible: `make -C platform/ps2` stopped
with `PS2SDK is unset`, and `make -C platform/psp` stopped because `psp-config` is
unavailable. No successful local PS2/PSP build is claimed; pinned GitHub workflows
remain the cross-build gate.

## GitHub Actions

Pending publication and execution for the milestone branch. Workflow YAML existing
does not count as verification.

## Limits

Aggregate `instruction_cycles` are processor-cycle totals, not SNES master cycles.
The callback trace represents meaningful reads/writes performed by the reference
interpreter; it does not claim pin-level VPA/VDA/MLB behavior, internal cycles or
5A22 wait-state accuracy. No PS2, PSP, PCSX2, PPSSPP or real-hardware runtime test
has been performed by this milestone.
