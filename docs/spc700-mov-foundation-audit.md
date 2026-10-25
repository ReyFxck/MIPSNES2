# SPC700 register/immediate MOV audit

Date: 2026-10-25

## Scope

This block extends the atomic SPC700 execution foundation with three immediate
loads (`MOV A/X/Y,#imm`) and six register transfers (`MOV A,X`, `MOV A,Y`,
`MOV X,A`, `MOV Y,A`, `MOV X,SP`, `MOV SP,X`). It does not add direct-page,
absolute, indirect or memory-write addressing.

## Behavior and evidence

The manufacturer *SFC Sound Manual* SPC700 instruction summary lists opcodes
`$E8/$CD/$8D` for the immediate A/X/Y loads and `$7D/$DD/$5D/$FD/$9D/$BD`
for the six register transfers. Each is two cycles. All except `MOV SP,X`
derive N and Z from the transferred byte; `MOV SP,X` preserves every PSW bit.
The linked manual mirror was consulted on 2026-10-25. No emulator source or
external implementation was used.

## Atomic contract

- Immediate forms fetch the operand through the existing S-SMP map at PC+1,
  including `$FFFF->$0000` wrap.
- Cycle-total overflow is rejected before the operand read.
- If the operand address is unavailable, programmer-visible registers, PC,
  cycle total and timers remain unchanged; the successfully fetched opcode
  diagnostic remains visible.
- A successful instruction commits its register/PSW result and advances the
  timer block by exactly two S-SMP cycles.

## Verification

`tests/test_spc700_cpu.c` covers zero, positive and negative immediate values for
all three targets; all six register transfers; preservation of unrelated registers
and PSW bits; `MOV SP,X` flag preservation; operand wrap; unavailable IPL operands;
overflow before operand commit; all 256 dispatch bytes; and MOV-to-timer coupling.
Host, sanitizer, analyzer and target-CI results are recorded in `STATUS.md`.
Locally, `make clean && make -j2 test`, the focused ASan/UBSan fixture with leak
checking disabled for this environment, and GCC `-fanalyzer` all passed. Clang and
both console SDKs were unavailable.

## Open limits

Execution remains instruction-atomic rather than bus-cycle resumable. Memory
addressing, dummy reads, reset/interrupt entry, SLEEP/STOP, DSP access and the
S-CPU/S-SMP scheduler domain are absent. No target-runtime or audio result is claimed.

Next primary milestone: add SPC700 direct-page address formation and the first
direct-page register MOV load/store forms.
