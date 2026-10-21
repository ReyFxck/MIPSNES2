# 65C816 boundary-visible ABORT foundation audit

Audit date: 2026-10-21. Branch: `feature/65c816-abort-foundation`, based on main
revision `4f1f83d857ff72d94f50a55a23206b92e864108e`.

## Audited scope

This change adds a caller-latched ABORT input to the existing C99 65C816 reference
interpreter. At an instruction boundary it implements the documented vector choice,
native/emulation stack layouts, status changes, aggregate cycles and priority over
NMI and IRQ. It also preserves WDC's WAI rule: ABORT does not release WAI, and a
later accepted interrupt stacks the WAI opcode address so RTI can re-execute it.

The CPU remains caller-owned and sees only logical 24-bit addresses. The new input
does not add a platform dependency, scheduler coupling, allocation, emulator source,
external test ROM, game-specific path or RFAuds2 runtime integration.

The evidence source is the Western Design Center W65C816S Datasheet dated March 13,
2024. Sections 2.13, 7.4, 7.13 and 7.19, vector Tables 5-2/5-3 and Table 5-7
entry 22a are recorded in [65c816.md](65c816.md). All new tests were
independently authored from that hardware documentation.

## Local verification

Environment:

- Ubuntu host;
- GCC 13.3.0;
- local Clang unavailable;
- local PS2SDK unavailable;
- local PSPSDK/`psp-config` unavailable.

Verified commands and results:

| Command | Result |
|---|---|
| `make BUILD_DIR=build/abort-dev -j2 build/abort-dev/test-cpu-bus && build/abort-dev/test-cpu-bus` | Pass: logical bus and all 256 opcode values |
| `make BUILD_DIR=build/abort-default -j2 test` | Pass: all 13 C test executables, host smoke and repository checks |
| `make BUILD_DIR=build/abort-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2 test` | Pass: release suite |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/abort-sanitize CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' -j1 test` | Pass: AddressSanitizer and UndefinedBehaviorSanitizer suite after restoring the execute bit on one locally linked test artifact |

The first full default run correctly failed the repository link checker because this
audit file had not yet been created. After the missing evidence document was added,
the complete default suite passed. During the sanitizer build, the local linker left
`build/abort-sanitize/test-contracts` as a valid ELF without its execute bit; applying
`chmod u+x` only to that generated artifact allowed the unchanged sanitizer suite to
pass. No assertion or sanitizer finding was reported. Generated build artifacts are
not committed.

No local PS2/PSP cross-build, PCSX2, PPSSPP, real PS2 or real PSP runtime test was
possible. Cross-build verification is delegated to GitHub Actions. No performance
measurement was made.

## GitHub Actions

Pending publication. This section must be updated with actual workflow runs before
the pull request is merged. Workflow YAML existing is not treated as successful CI.

## Verified behavior

- ABORT is sampled before opcode fetch at the current atomic step boundary;
- priority is ABORT, then NMI, then unmasked IRQ;
- emulation mode uses `00:FFF8-FFF9`, pushes PCH/PCL/P and keeps page-one stack wrap;
- native mode uses `00:FFE8-FFE9` and pushes PBR/PCH/PCL/P;
- the stacked emulation status has bit 4 clear;
- entry clears D, sets I, sets PBR to zero and records seven/eight aggregate cycles;
- a lower-priority pending NMI remains latched after ABORT entry;
- ABORT during WAI neither vectors nor wakes the CPU and preserves the WAI restart
  address for a later accepted interrupt;
- STP continues to accept only RESET, while RESET clears the latched ABORT input;
- an unavailable write path produces the existing sticky invalid-bus diagnostic before
  any bus access;
- an unimplemented opcode still stops diagnostically and is never treated as NOP.

## Explicit limits

The interpreter remains instruction-atomic. It cannot yet sample ABORT during an
instruction, suppress partially completed register changes or model the documented
cycle-sensitive read-modify-write, RTI and nested-interrupt exceptions. It also does
not model a held-low/repeated electrical signal, exact dummy/internal bus cycles or
MLB. Those behaviors require a resumable CPU cycle/micro-operation contract and keep
the broad ABORT, timing and complete-interpreter tasks in progress.

## Audit conclusions

- the implementation and tests match the claimed boundary-visible scope;
- no PS2, PSP or host API appears in the common CPU implementation;
- no RFAuds2 dependency appears outside future PS2 audio documentation;
- no game-specific branch, copied emulator implementation, assembly or benchmark
  claim was introduced;
- [STATUS.md](../STATUS.md) records the verified subset but does not mark broad ABORT
  or complete interpreter integration finished.
