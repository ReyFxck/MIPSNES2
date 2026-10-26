# SPC700 100-opcode ALU audit

Date: 2026-10-26

## Scope

This single block adds exactly 100 SPC700 opcodes to the existing atomic
dispatcher, raising implemented dispatch from 50 to 150 bytes:

- 54 accumulator ALU forms: all nine documented addressing modes for each of
  `OR A`, `AND A`, `EOR A`, `CMP A`, `ADC A` and `SBC A`;
- 18 direct-memory ALU forms: `dp,dp`, `dp,#imm` and `(X),(Y)` for the same six
  operations;
- 16 shift/rotate forms: accumulator, direct, direct+X and absolute forms of
  `ASL`, `ROL`, `LSR` and `ROR`;
- 12 byte increment/decrement forms: A, X, Y, direct, direct+X and absolute
  `INC`/`DEC`.

Nintendo's *SNES Development Manual*, appendix C tables C-7 through C-10,
supplies every opcode, instruction length, 2-6-cycle total, operation and flag
effect. No emulator source or external implementation was used.

## Shared execution contract

- ALU opcode families share one operation helper. OR/AND/EOR update N/Z; CMP
  updates N/Z/C without storing; ADC/SBC update N/V/H/Z/C with SPC700 carry/no-
  borrow semantics.
- All nine accumulator address forms use the same mapped direct, absolute,
  indexed, register-indirect and direct-indirect address construction already
  verified by MOV.
- Direct-memory encodings fetch source before destination. `(X),(Y)` uses Y as
  source and X as destination within the P-selected direct page.
- Shifts/rotates share mapped read-modify-write behavior and update N/Z/C.
  INC/DEC update only N/Z and wrap modulo 256.
- Cycle-total overflow is rejected before operands or mapped data accesses.
  Unavailable operands/targets commit neither programmer-visible registers nor
  instruction time. Successful mapped reads/writes retain memory-owned side
  effects; exact per-cycle bus ordering and destination dummy reads are not
  represented.

## Verification

`tests/test_spc700_alu.c` independently counts exactly 100 new opcode bytes and
checks:

- all 54 accumulator ALU opcode/address-mode combinations with exact PC lengths,
  cycle totals, mapped values and unaffected registers;
- all 18 direct-memory ALU forms, source/destination order and CMP no-write;
- every shift/rotate and byte INC/DEC form, carry input/output, wrap and N/Z;
- all 262,144 combinations of accumulator byte, operand byte and carry state for
  both immediate ADC and SBC;
- page-zero `$F4` communication MMIO versus P-selected `$01F4` ARAM;
- unavailable `$F0` reads/read-modify-writes, cycle overflow before mutation and
  exact instruction-to-timer delivery.

`tests/test_spc700_cpu.c` still dispatches every possible opcode byte and proves
that the remaining 106 values return the explicit unimplemented result without
register or time commit. The clean host suite, focused ASan/UBSan fixtures,
repository checks and GCC `-fanalyzer` are required before publication. Host,
PS2 and PSP cross-build CI evidence is recorded in `STATUS.md`; no target-runtime
or audio result is claimed.

## Open limits

Sixteen-bit MOVW/arithmetic, decimal adjust, multiply/divide, bit operations,
branches/calls/returns, stack behavior, reset/interrupt entry, SLEEP/STOP, DSP
access and synchronized S-CPU/S-SMP scheduling remain absent. Exact electrical
bus phases remain outside the instruction-atomic model.

Next primary milestone: add the 16-bit MOVW/arithmetic family, then conditional
branches and stack/control-flow foundations.
