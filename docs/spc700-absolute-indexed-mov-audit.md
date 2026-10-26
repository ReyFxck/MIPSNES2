# SPC700 absolute-indexed MOV audit

Date: 2026-10-26

## Scope

This block adds four absolute-indexed instructions to the atomic dispatcher:
`MOV A,!abs+X`, `MOV A,!abs+Y`, `MOV !abs+X,A` and `MOV !abs+Y,A`. Each
instruction fetches a little-endian 16-bit base address, adds the selected 8-bit
index in the full 16-bit address space and accesses the result through the existing
S-SMP map.

## Behavior and evidence

Nintendo's *SNES Development Manual*, appendix C, lists `$F5/$F6` for the
absolute-indexed A loads at three bytes and five cycles, and `$D5/$D6` for the
matching stores at three bytes and six cycles. Loads derive N and Z from the
transferred byte; stores preserve PSW. The manufacturer *SFC Sound Manual* provides
the same instruction summary. Both references were consulted on 2026-10-26. No
emulator source or external implementation was used.

## Atomic contract

- Cycle-total overflow is rejected before either operand byte or the target access.
- The effective address is `(absolute + index) & $FFFF`; PSW.P does not alter it.
- Operand bytes retain the established mapped, independently wrapping PC fetch.
- An unavailable indexed target leaves programmer-visible registers, PC,
  instruction time and target backing bytes unchanged.
- Successful target reads and writes retain S-SMP-map side effects after indexing.
  The instruction step remains atomic and is not an exact electrical bus trace.
- A successful load advances the timer block by five S-SMP cycles; a store advances
  it by six.

## Verification

`tests/test_spc700_cpu.c` covers all four opcodes; X and Y selection; both P states;
forced 16-bit effective-address wrap; zero, positive and negative load values; N/Z
and unrelated-flag preservation; store register/flag preservation; exact three-byte
PC and 5/6-cycle totals; indexed `$00F4` communication MMIO versus `$01F4` ARAM;
unavailable indexed `$00F0` loads/stores; rejection before wrapped store mutation
on cycle overflow; a mixed 5+5+6-cycle timer edge; and deterministic dispatch of
all 256 opcode bytes. Host, sanitizer, static-analysis and target-CI evidence are
recorded in `STATUS.md`.

## Open limits

Register-indirect, auto-increment, direct-indirect, direct-to-direct and
immediate-to-direct MOV forms are recorded in the subsequent audit. Exact bus phases and destination dummy
reads are not represented by the current instruction-atomic contract.
Reset/interrupt entry, SLEEP/STOP, DSP access and the synchronized S-CPU/S-SMP clock
domain also remain absent. No target-runtime or audio result is claimed.

Next milestone: the
[indirect and memory MOV audit](spc700-indirect-and-memory-mov-audit.md).
