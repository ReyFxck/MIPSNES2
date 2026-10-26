# SPC700 absolute MOV audit

Date: 2026-10-26

## Scope

This block adds six absolute instructions to the atomic dispatcher: `MOV A,!abs`,
`MOV X,!abs`, `MOV Y,!abs`, `MOV !abs,A`, `MOV !abs,X` and `MOV !abs,Y`. Each
instruction fetches a little-endian 16-bit target from the two bytes following the
opcode. The operand fetch and target access use the existing S-SMP map.

## Behavior and evidence

Nintendo's *SNES Development Manual*, appendix C, lists `$E5/$E9/$EC` for the
absolute A/X/Y loads at three bytes and four cycles, and `$C5/$C9/$CC` for the
matching stores at three bytes and five cycles. Loads derive N and Z from the
transferred byte; stores preserve PSW. The manufacturer *SFC Sound Manual* provides
the same instruction summary. Both references were consulted on 2026-10-26. No
emulator source or external implementation was used.

## Atomic contract

- Cycle-total overflow is rejected before either operand byte or the target access.
- Low and high address bytes are fetched in that order through the mapped PC path;
  each increment wraps independently in the 16-bit address space.
- PSW.P does not alter an absolute target.
- An unavailable second operand or absolute target leaves programmer-visible
  registers, PC, instruction time and target backing bytes unchanged.
- Successful mapped target reads and writes retain S-SMP-map side effects. The
  instruction step remains atomic and is not an exact electrical bus trace.
- A successful load advances the timer block by four S-SMP cycles; a store advances
  it by five.

## Verification

`tests/test_spc700_cpu.c` covers all six opcodes; both P states; little-endian
address construction; zero, positive and negative load values; N/Z and unrelated
flag preservation; all three store sources; exact three-byte PC and cycle totals;
`$00F4` communication MMIO versus `$01F4` ARAM; unavailable `$00F0` loads/stores;
an operand spanning `$FFFF-$0000`; rejection when the absolute high operand lies
in an unavailable IPL overlay; rejection before store mutation on cycle overflow;
a four-load timer edge; and deterministic dispatch of all 256 opcode bytes. Host,
sanitizer, static-analysis and target-CI evidence are recorded in `STATUS.md`.

## Open limits

Absolute-indexed, indirect, direct-to-direct and immediate-to-direct MOV forms
remain absent. Exact bus phases and destination dummy reads are not represented by
the current instruction-atomic contract. Reset/interrupt entry, SLEEP/STOP, DSP
access and the synchronized S-CPU/S-SMP clock domain also remain absent. No
target-runtime or audio result is claimed.

Next primary milestone: add the four absolute-indexed A-register MOV load/store
forms, including 16-bit effective-address wrap and mapped-access atomicity.
