# SPC700 execution-foundation audit

Date: 2026-10-25

## Scope

This block adds caller-owned SPC700 A, X, Y, SP, PSW and PC state plus one atomic
instruction-step boundary over the existing S-SMP memory map. It implements `NOP`
and the eight one-byte PSW-control instructions CLRP, SETP, CLRC, SETC, EI, DI,
CLRV and NOTC. No hardware reset values are invented.

## Contract

- A successful step fetches at PC, executes one supported instruction, wraps the
  16-bit PC, charges the documented 2 or 3 S-SMP cycles and advances timers once.
- All 256 opcode bytes reach dispatch. The 247 unsupported values return an explicit
  result without changing programmer-visible registers, the cycle total or timers.
- The most recently fetched byte/address is diagnostic state. A mapped fetch may
  still perform the read side effect owned by the S-SMP memory map.
- Missing IPL bytes and cycle-total overflow fail before architectural commit.
- State and memory are caller-owned; there is no allocation or platform API in core.

## Evidence

The mirrored manufacturer *SFC Sound Manual*, SPC700 instruction-summary section,
defines the `NVPBHIZC` PSW order and the opcode, byte length, cycle count and flag
effects used here. It lists `NOP` as `$00`, one byte and two cycles; the PSW-control
group supplies the remaining eight operations and their two/three-cycle costs. The
source is linked in [references](references.md); no manual, firmware or emulator
source was copied into the repository.

## Verification

`tests/test_spc700_cpu.c` covers:

- initialization rejection and exact caller-register preservation;
- deterministic dispatch of every byte value;
- every implemented PSW transition, including both NOTC directions;
- unchanged A/X/Y/SP, correct PC increment and `$FFFF->$0000` wrap;
- IPL-overlay fetch and explicit missing-IPL failure;
- cycle-total overflow without register/timer commit;
- eight two-cycle NOPs producing one enabled timer-2 edge at 16 cycles.

The host command and target CI evidence are recorded in `STATUS.md` after execution.
Locally, the full host suite passed after correcting the recurring generated-file
execute-bit artifact. The focused ASan/UBSan test passed with leak detection disabled
because this environment prevents LeakSanitizer from inspecting `/proc`; GCC
`-fanalyzer` also passed. Clang and both console SDKs were unavailable.

## Open limits

The step is instruction-atomic, not a per-cycle bus cursor. Operand addressing,
loads/stores, arithmetic, branches, stack, interrupts, SLEEP/STOP, reset sequencing,
DSP access and S-CPU/S-SMP scheduling remain absent. Real PS2/PSP runtime behavior
and physical SNES edge timing are not claimed.

Later MOV milestones are recorded separately in the
[register/immediate MOV audit](spc700-mov-foundation-audit.md) and
[direct-page MOV audit](spc700-direct-page-mov-audit.md) and
[indexed-direct-page MOV audit](spc700-indexed-direct-page-mov-audit.md) and
[absolute MOV audit](spc700-absolute-mov-audit.md) and
[absolute-indexed MOV audit](spc700-absolute-indexed-mov-audit.md) and
[indirect and memory MOV audit](spc700-indirect-and-memory-mov-audit.md). The
[100-opcode ALU audit](spc700-100-opcode-alu-audit.md) records the later
arithmetic, logic, shift and byte increment/decrement families.
