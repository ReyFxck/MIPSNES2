# core/apu/spc700

`memory.[ch]` owns the bounded S-SMP address map over caller-owned 64 KiB ARAM,
an optional caller-supplied 64-byte IPL image, the communication mailbox and
the `$F1-$FF` implemented register routes, including `$F2/$F3` into S-DSP.
`timers.[ch]` owns the three-stage
timer state and an explicit S-SMP-cycle advance API. `cpu.[ch]` owns caller-provided
register state plus bounded instruction-atomic fetch/dispatch for all 256 opcode
values across the architectural addressing modes. `../timing.[ch]` derives a
deterministic nominal S-SMP budget from machine master time and advances CPU,
timers and S-DSP cadence. TEST MMIO, interrupt/reset entry, exact per-cycle bus
phases, electrical mailbox races and full S-DSP audio remain unimplemented.

See the [memory-map audit](../../../docs/smp-memory-map-audit.md),
[timer audit](../../../docs/smp-timer-audit.md) and
[execution audit](../../../docs/spc700-execution-foundation-audit.md), the
[register/immediate MOV audit](../../../docs/spc700-mov-foundation-audit.md), the
[direct-page MOV audit](../../../docs/spc700-direct-page-mov-audit.md), the
[indexed-direct-page MOV audit](../../../docs/spc700-indexed-direct-page-mov-audit.md),
the [absolute MOV audit](../../../docs/spc700-absolute-mov-audit.md), the
[absolute-indexed MOV audit](../../../docs/spc700-absolute-indexed-mov-audit.md),
the [indirect and memory MOV audit](../../../docs/spc700-indirect-and-memory-mov-audit.md),
the [100-opcode ALU audit](../../../docs/spc700-100-opcode-alu-audit.md), the
[complete-opcode audit](../../../docs/spc700-complete-opcode-audit.md), and the
[APU clock/DSP register audit](../../../docs/apu-clock-dsp-register-audit.md), and the
[canonical status](../../../STATUS.md).
