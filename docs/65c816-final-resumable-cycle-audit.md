# 65C816 complete resumable opcode-cycle audit

Date: 2026-10-23

## Scope

This milestone migrates the final 107 opcode values from ordinary execution through
`LEGACY_ATOMIC` to caller-owned peek/commit sequences:

| Family | Opcode count | Cursor path |
|---|---:|---|
| ORA, AND, EOR | 45 | Shared operand/address/pointer/data phases |
| ADC, SBC | 30 | Shared phases plus binary/decimal accumulator effect |
| CMP, CPX, CPY | 21 | Shared phases plus M/X-selected compare effect |
| BIT | 5 | Shared phases plus immediate/memory flag effect |
| RTI, WDM, MVP, MVN, WAI, STP | 6 | Dedicated stack, signature, block-move and control phases |
| **Total** | **107** | **All 256 opcode values resumable on a complete bus** |

The implementation remains entirely in the platform-neutral 65C816 core. PS2, PSP
and host backends receive no opcode logic, and there are no title-specific paths.

## Implementation checklist

- [x] Separate ALU architectural effects from aggregate cycle accounting so the
  final effect can commit exactly once.
- [x] Reuse the existing 16-mode operand sequencer for all 101 read/ALU opcodes.
- [x] Preserve M-selected accumulator width and X-selected CPX/CPY width.
- [x] Preserve Direct Page penalties, conditional indexed cycles, bank wrapping,
  stack-relative addressing and long-address linear carry.
- [x] Add explicit emulation/native RTI stack sequences.
- [x] Add WDM signature consumption and final-cycle WAI/STP transitions.
- [x] Add seven-operation MVP/MVN byte transfers with destination/source bank order,
  DBR update, X/Y direction and PC repetition.
- [x] Retain `LEGACY_ATOMIC` only for missing-write fail-stop preflight, preserving
  the established zero-cycle `INVALID_BUS` contract.
- [x] Keep core/backend separation and avoid game-specific behavior.
- [x] Use no source code from another emulator.

The behavioral reference remains Western Design Center's
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
already adopted by the project for the opcode/cycle table, stack rules, block moves
and WAI/STP behavior. No emulator implementation was consulted or copied.

## Deterministic verification checklist

- [x] Aggregate semantic suite executes all 256 opcode values through
  `mipsnes2_cpu_step`.
- [x] Exhaustive cursor regression executes every opcode from fetch to completion
  and rejects any `LEGACY_ATOMIC` descriptor on a complete bus.
- [x] Exact descriptor-order tests cover emulation/native RTI, WDM, WAI, STP, MVP
  and MVN, including cycles, stack addresses, bus order and architectural results.
- [x] Existing both-width load/store/addressing matrices exercise the same shared
  operand phases now used by the 101 read/ALU opcodes.
- [x] Host GCC release tests pass.
- [ ] Host Clang release tests pass in CI; Clang is unavailable locally.
- [x] GCC AddressSanitizer/UndefinedBehaviorSanitizer tests pass.
- [ ] Clang AddressSanitizer/UndefinedBehaviorSanitizer tests pass in CI.
- [x] Pinned PS2 cross-build passes before PSP validation.
- [x] Pinned PSP cross-build passes after PS2 validation.
- [x] Repository policy checker and link/checkbox validation pass.
- [x] XERJ project-only index/search/update completes with feedback disabled.
- [ ] GitHub Host, PS2 and PSP workflows pass for the implementation commit.

Local evidence:

| Check | Result |
|---|---|
| `make clean && make test` | Passed with host GCC |
| GCC ASan/UBSan suite | Passed with leak detection disabled for this environment |
| PS2 v2.0.0 / GCC 15.2.0 | Passed; ELF SHA-256 `e53be3a0bcc724d2364495ce56211a25309f1cbc9427e0758f3336c8077df448` |
| PSP v20261001 / GCC 15.2.0 | Passed; PBP SHA-256 `e5bc0f7f5874bf888edb7f2bc9eea900aac0b939891913d77dca6f48a51cffb1` |
| XERJ v1.0.0-rc.97 | Dry-run and write indexed 157 first-party files / 1,252 records; search and definition lookup passed |

## Completion criterion

This block is complete when a complete read/write bus can execute every opcode value
through the cursor without producing `LEGACY_ATOMIC`, the deterministic semantic and
descriptor-order suites pass, PS2 then PSP cross-build successfully, XERJ completes
on this checkout only, and all required GitHub workflows are green.

## Honest boundary

Completing all opcode sequences does not make the CPU bus-cycle exact. RESET is not
yet represented by the cursor. Several INTERNAL descriptors represent aggregate
documented cycles rather than verified dummy reads, and MLB, RDY, BE, refresh,
mid-instruction ABORT and DMA arbitration remain outside this milestone. The timed
5A22 scheduler bridge still completes a whole instruction before dispatching events.
