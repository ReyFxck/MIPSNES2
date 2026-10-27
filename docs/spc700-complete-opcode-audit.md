# SPC700 complete-opcode audit

Date: 2026-10-26

## Scope

This block adds the remaining 106 SPC700 opcode values to the existing atomic
dispatcher, raising implemented dispatch from 150 to all 256 bytes. The new
coverage consists of:

- 56 patterned bit operations: sixteen `SET1`/`CLR1`, sixteen `BBS`/`BBC`,
  sixteen `TCALL` and eight carry/memory-bit operations;
- eight conditional flag branches plus `BRA`, two `CBNE` forms and two `DBNZ`
  forms;
- eight register/PSW stack transfers, `CALL`, `PCALL`, `RET`, `RETI`, `BRK` and
  both `JMP` forms;
- `MOVW` in both directions, `INCW`, `DECW`, `CMPW`, `ADDW` and `SUBW`;
- six X/Y compare forms, `TSET1`, `TCLR1`, `MUL`, `DIV`, `XCN`, `DAA`, `DAS`,
  `SLEEP` and `STOP`.

Nintendo's *SNES Development Manual*, appendix C tables C-11 through C-20, and
the manufacturer *SFC Sound Manual* supply the opcode, operand length, aggregate
cycle total, operation and documented flag effects. Anomie's independent SPC700
hardware research was used to resolve division corner cases, stack wrapping and
direct-page word wrapping. No source code from another emulator was used.

## Execution contract

- Direct-page word accesses wrap the low byte within the P-selected 256-byte
  page. Stack accesses wrap within page one.
- The 13-bit memory address and three-bit bit number use the SPC700 packed bit
  operand format. Carry/bit logic and `MOV1` preserve all unrelated flags.
- Conditional branch costs include their taken delta. The maximum possible cost
  is checked before operand or mapped-data access, so overflow cannot partially
  mutate state.
- Word arithmetic updates the documented N/Z/C/H/V subset, with N and Z derived
  from the 16-bit result. `CMPW` does not store a result.
- `DIV YA,X` follows the documented hardware algorithm, including the overflow
  path based on `Y >= X` and the `X << 9` term. `MUL`, decimal adjustment and
  nibble exchange use bounded integer operations.
- `SLEEP` and `STOP` commit their instruction cost and enter an explicit wait
  state. Later atomic steps report `HALTED` until the caller reinitializes the
  CPU; interrupt wake-up is not invented before an interrupt model exists.

## Verification

`tests/test_spc700_complete.c` independently counts exactly 106 newly supported
bytes and checks:

- every one of the 256 opcode values dispatches with its documented non-taken
  base length and cycle total;
- all set/clear/branch bit encodings, every `TCALL` vector and all boolean bit
  truth tables;
- taken and non-taken branches, PC wrapping, calls, returns, BRK/RETI and stack
  wrapping;
- word operations and direct-page wrapping, compare forms and TSET/TCLR;
- all 65,536 A/Y input combinations for `MUL`, plus every X/Y pair with six A
  samples in the 393,216-case `DIV` matrix;
- all byte values for `XCN`, and all A/H/C combinations for `DAA` and `DAS`;
- wait-state behavior, mapped-access failure and maximum taken-cycle overflow.

The earlier MOV and 100-opcode ALU fixtures remain independent regression layers.
The clean host suite, focused ASan/UBSan fixtures, repository checks and GCC
`-fanalyzer` are required before publication. Host, PS2 and PSP cross-build CI
evidence is recorded in `STATUS.md`; cross-build success is not target execution.

## Open limits

The dispatcher is instruction-atomic. It represents documented aggregate
instruction costs, not exact internal/dummy bus phases or interrupt sampling
inside an instruction. Signal-level reset and interrupt entry, a synchronized
S-CPU/S-SMP/S-DSP scheduler, DSP register execution, sample generation and target
audio remain absent. No target-runtime or audio result is claimed.

Next primary milestone: connect the complete atomic dispatcher to a deterministic
S-SMP clock domain, then implement and independently verify the S-DSP path.
