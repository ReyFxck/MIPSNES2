# SPC700 indexed-direct-page MOV audit

Date: 2026-10-26

## Scope

This block adds six indexed direct-page instructions to the atomic dispatcher:
`MOV A,dp+X`, `MOV X,dp+Y`, `MOV Y,dp+X`, `MOV dp+X,A`, `MOV dp+Y,X` and
`MOV dp+X,Y`. The operand and selected index are added modulo 256 before PSW.P
selects page 0 or page 1. Accesses continue through the existing S-SMP map, so
wrapped page-zero addresses retain MMIO behavior.

## Behavior and evidence

The manufacturer *SFC Sound Manual* defines direct-page indexing within the page
selected by PSW.P. Its SPC700 instruction summary and Nintendo's development-manual
instruction tables list `$F4/$F9/$FB` for the indexed A/X/Y loads at two bytes and
four cycles, and `$D4/$D9/$DB` for the matching stores at two bytes and five cycles.
Loads derive N and Z from the transferred byte; stores preserve PSW. Both manuals
were consulted on 2026-10-26. No emulator source or external implementation was
used.

## Atomic contract

- Cycle-total overflow is rejected before operand or target access.
- The effective offset is `(operand + index) & $FF`; no carry escapes the page
  selected by PSW.P.
- An unavailable wrapped page-zero MMIO target leaves programmer-visible state,
  instruction time and the ARAM backing byte unchanged.
- Successful mapped reads and writes retain S-SMP-map side effects. The instruction
  step remains atomic and is not an exact electrical bus trace.
- A successful load advances the timer block by four S-SMP cycles; a store advances
  it by five.

## Verification

`tests/test_spc700_cpu.c` covers all six opcodes, both P-selected pages, forced
eight-bit wrap, zero/positive/negative load values, N/Z and unrelated-flag
preservation, all three store sources, exact PC/cycle totals, wrapped `$F4`
communication-port routing versus `$01F4` ARAM, wrapped unavailable `$F0`
loads/stores, rejection before indexed-store mutation on cycle overflow, a
four-load timer edge and deterministic dispatch of all 256 opcode bytes. Host,
sanitizer, static-analysis and target-CI evidence are recorded in `STATUS.md`.

## Open limits

Absolute, indirect, direct-to-direct and immediate-to-direct MOV forms remain
absent. Exact bus phases and destination dummy reads are not represented by the
current instruction-atomic contract. Reset/interrupt entry, SLEEP/STOP, DSP access
and the synchronized S-CPU/S-SMP clock domain also remain absent. No target-runtime
or audio result is claimed.

Next primary milestone: add absolute A/X/Y MOV load/store forms while preserving
mapped-access failure and cycle-overflow atomicity.
