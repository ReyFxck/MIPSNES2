# core/apu/spc700

`memory.[ch]` owns the bounded S-SMP address map over caller-owned 64 KiB ARAM,
an optional caller-supplied 64-byte IPL image, the communication mailbox and
the `$F1/$FA-$FF` timer-register route. `timers.[ch]` owns the three-stage
timer state and an explicit S-SMP-cycle advance API. It does not execute SPC700
instructions or derive S-SMP time from the S-CPU. DSP and TEST MMIO remain
deliberately unimplemented and report unsupported accesses.

See the [memory-map audit](../../../docs/smp-memory-map-audit.md),
[timer audit](../../../docs/smp-timer-audit.md) and
[canonical status](../../../STATUS.md).
