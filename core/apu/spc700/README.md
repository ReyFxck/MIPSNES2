# core/apu/spc700

`memory.[ch]` owns the bounded S-SMP address map over caller-owned 64 KiB ARAM,
an optional caller-supplied 64-byte IPL image, the communication mailbox and
the `$F1/$FA-$FF` timer-register route. `timers.[ch]` owns the three-stage
timer state and an explicit S-SMP-cycle advance API. `cpu.[ch]` owns caller-provided
register state plus bounded atomic fetch/dispatch for 150 PSW/MOV/ALU/shift/
increment/decrement instructions across mapped addressing modes.
It does not derive S-SMP time from the S-CPU. DSP and TEST MMIO, word operations,
bit operations, control flow, stack behavior and the rest of the instruction set remain
deliberately unimplemented.

See the [memory-map audit](../../../docs/smp-memory-map-audit.md),
[timer audit](../../../docs/smp-timer-audit.md) and
[execution audit](../../../docs/spc700-execution-foundation-audit.md), the
[register/immediate MOV audit](../../../docs/spc700-mov-foundation-audit.md), the
[direct-page MOV audit](../../../docs/spc700-direct-page-mov-audit.md), the
[indexed-direct-page MOV audit](../../../docs/spc700-indexed-direct-page-mov-audit.md),
the [absolute MOV audit](../../../docs/spc700-absolute-mov-audit.md), the
[absolute-indexed MOV audit](../../../docs/spc700-absolute-indexed-mov-audit.md),
the [indirect and memory MOV audit](../../../docs/spc700-indirect-and-memory-mov-audit.md),
the [100-opcode ALU audit](../../../docs/spc700-100-opcode-alu-audit.md), and the
[canonical status](../../../STATUS.md).
