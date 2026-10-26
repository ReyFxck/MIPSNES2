# SPC700 direct-page MOV audit

Date: 2026-10-26

## Scope

This block adds the first memory-addressed SPC700 instructions to the atomic
dispatcher: `MOV A,dp`, `MOV X,dp`, `MOV Y,dp`, `MOV dp,A`, `MOV dp,X` and
`MOV dp,Y`. The effective address is the unsigned operand byte in page 0 when
PSW.P is clear and in page 1 when PSW.P is set. Accesses use the existing S-SMP
map rather than bypassing its communication-port and timer-register behavior.

## Behavior and evidence

The manufacturer *SFC Sound Manual* identifies PSW.P as the direct-page selector:
P=0 selects `$0000-$00FF`, while P=1 selects `$0100-$01FF`. Its SPC700
instruction summary lists `$E4/$F8/$EB` for the A/X/Y loads at two bytes and
three cycles, and `$C4/$D8/$CB` for the corresponding stores at two bytes and
four cycles. Loads derive N and Z from the transferred byte; stores preserve all
PSW bits. The linked manual mirror was consulted on 2026-10-26. No emulator
source or external implementation was used.

## Atomic contract

- Cycle-total overflow is rejected before operand or direct-page access.
- The operand fetch uses the mapped PC path and retains 16-bit PC wrap.
- An unavailable direct-page MMIO target leaves programmer-visible registers,
  PC, cycle total and timers unchanged.
- Successful memory-mapped reads and writes retain the side effects owned by the
  S-SMP map. The instruction step is not an exact per-cycle bus trace.
- A successful instruction advances the existing timer block by exactly three
  cycles for a load or four cycles for a store.

## Verification

`tests/test_spc700_cpu.c` covers both P-selected pages; zero, positive and
negative load values for A/X/Y; N/Z and unrelated-flag preservation; A/X/Y
stores and complete PSW/register preservation; page-zero `$F4` communication
MMIO in both directions; page-one `$01F4` ARAM isolation for the same operand;
unavailable `$F0` loads/stores; store rejection before memory mutation on cycle
overflow; exact PC/cycle results; and deterministic dispatch of all 256 opcode
bytes. The clean host suite, focused ASan/UBSan run,
GCC `-fanalyzer` and target CI evidence are recorded in `STATUS.md`.

## Open limits

Indexed direct-page, indirect, absolute, direct-to-direct and immediate-to-direct
MOV forms remain absent. Exact bus phases, dummy reads, reset/interrupt entry,
SLEEP/STOP, DSP access and the S-CPU/S-SMP scheduler domain remain absent. No
target-runtime or audio result is claimed.

Next primary milestone: add indexed direct-page A/X/Y MOV load/store forms,
including eight-bit index wrap within the P-selected page.
