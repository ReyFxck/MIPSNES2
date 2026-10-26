# SPC700 indirect and memory MOV audit

Date: 2026-10-26

## Scope

This block adds ten byte-transfer instructions to the atomic dispatcher:

- register indirect: `MOV A,(X)`, `MOV (X),A`, `MOV A,(X)+` and
  `MOV (X)+,A`;
- direct indirect: `MOV A,[dp+X]`, `MOV [dp+X],A`, `MOV A,[dp]+Y` and
  `MOV [dp]+Y,A`;
- direct-page memory transfer: `MOV dp,#imm` and `MOV dp,dp`.

No emulator source or external implementation was used. Nintendo's *SNES
Development Manual*, appendix C tables C-5/C-6, supplies the opcodes, stream
lengths, cycle totals and flag effects. It lists `$E6/$C6` at 3/4 cycles,
`$BF/$AF` at four, `$E7/$F7` at six, `$C7/$D7` at seven and `$8F/$FA` at
five. Only loads into A derive N/Z; all stores and memory-to-memory forms
preserve PSW.

## Addressing contract

- `(X)` addresses offset X in the page selected by PSW.P. `(X)+` performs the
  same mapped access using the old X and increments X modulo 256 only after a
  successful access.
- `[dp+X]` wraps the operand-plus-X sum to eight bits before reading a
  little-endian pointer from the P-selected direct page. `[dp]+Y` reads the
  pointer before adding Y in the full 16-bit address space.
- The pointer high-byte offset wraps modulo 256 within the same direct page.
  Pointer bytes and resolved targets both traverse the S-SMP map.
- `$8F` encodes immediate then destination; `$FA` encodes source then
  destination, opposite the assembler destination-first spelling.
- Cycle overflow is rejected before operand, pointer or target access.
  Unavailable accesses commit neither registers nor instruction time. Exact
  per-cycle bus traffic and destination dummy reads remain outside this
  instruction-atomic contract.

## Verification

`tests/test_spc700_cpu.c` covers all ten opcodes; both P-selected pages;
zero/positive/negative loads and N/Z preservation; direct-page, pointer and
16-bit target wrap; post-increment order and `$FF->$00` wrap; stream operand
order; page-zero `$F4` communication-port routing versus `$01F4` ARAM;
unavailable pointer bytes, sources and targets; cycle-overflow rejection before
mutation; timer delivery; exact one/two/three-byte PC advances and 3/4/5/6/7
cycle totals; and deterministic dispatch of all 256 opcode bytes.

The clean host suite, focused ASan/UBSan fixture, repository checks and GCC
`-fanalyzer` are required before publication. Host, PS2 and PSP cross-build CI
evidence is recorded in `STATUS.md`; no target-runtime or audio result is
claimed.

## Open limits

`MOVW YA,dp` and `MOVW dp,YA` remain absent, as do arithmetic, branches, stack,
bit operations, reset/interrupt entry, SLEEP/STOP, DSP access and a synchronized
S-CPU/S-SMP clock domain. Exact electrical bus phases and destination dummy reads
are not represented.

Next milestone: the [100-opcode ALU audit](spc700-100-opcode-alu-audit.md).
