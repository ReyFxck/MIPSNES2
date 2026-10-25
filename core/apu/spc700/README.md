# core/apu/spc700

`memory.[ch]` owns the bounded S-SMP address map over caller-owned 64 KiB ARAM,
an optional caller-supplied 64-byte IPL image and the existing communication
mailbox. It does not execute SPC700 instructions. DSP, timers and other MMIO
are deliberately unimplemented and report unsupported accesses.

See the [memory-map audit](../../../docs/smp-memory-map-audit.md) and
[canonical status](../../../STATUS.md).
