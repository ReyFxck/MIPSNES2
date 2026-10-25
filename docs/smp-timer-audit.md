# S-SMP timer audit

Date: 2026-10-25

## Implemented boundary

`core/apu/spc700/timers.[ch]` owns three caller-owned timers without heap or
platform APIs. One free-running 0-127 phase produces the simultaneous timer-0/
timer-1 edge every 128 S-SMP cycles and timer-2 edges every 16 cycles. Disabled
timers ignore those edges without stopping the shared phase.

Each enabled timer has an eight-bit stage-2 counter and a four-bit stage-3
output. Stage 2 increments after its prescaler edge, compares with `$FA-$FC`
and resets on equality while incrementing stage 3 modulo 16. Target zero means
256 edges. Changing a target behind the current stage-2 value waits for
eight-bit wrap rather than creating an immediate match.

`$F1` bits 0-2 control enables. A 0-to-1 transition clears that timer's stages
2 and 3 but preserves stage-1 phase. `$FA-$FC` writes update targets and reads
return zero under the selected independent-research contract. `$FD-$FF` reads
return and clear only stage 3; writes do not affect timer state. As with other
handled S-SMP MMIO, writes still reach the underlying ARAM byte.

`mipsnes2_smp_memory_advance` is an explicit handoff denominated in S-SMP
cycles. It does not infer time from the S-CPU, a host clock or an audio device.
The implementation computes whole prescaler edges arithmetically and accepts a
64-bit cycle delta without iterating once per cycle.

## Verification

`tests/test_smp_timers.c` covers invalid calls, 16/128-cycle boundaries,
simultaneous phase, disabled free-running phase, rising-enable clear, read
clear, target zero, four-bit output wrap, a live target changed behind stage 2
and identical state after one-shot versus 17,637 irregular chunks.

`tests/test_smp_memory.c` covers `$F1/$FA-$FF` routing, target reads, output
read-clear and ignored output writes. `tests/test_machine.c` verifies that the
machine-owned map advances timer 2 and returns to the documented deterministic
power-on state. Host C99 `make clean && make -j2 test` passed on 2026-10-25.
ASan/UBSan timer, memory-map and machine fixtures passed with leak detection
disabled after force-rebuilding an unrelated generated test that had lost its
executable bit. PS2SDK and `psp-config` were unavailable locally; target build
results must come from pinned CI and no target runtime was exercised.

## Deliberate limits

There is no SPC700 interpreter or S-SMP/S-DSP scheduler, so callers must advance
the timers explicitly. The deterministic reset value and zero phase are not a
claim about analog power-on state or the measured phase relative to S-DSP echo
writes. `$F0` TEST effects, exact MMIO-cycle placement, target-write races,
read-clear versus increment races and S-CPU/S-SMP synchronization remain open.
These limits keep the roadmap timer item in progress rather than complete.

## Hardware sources

- [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual), control
  register and timer sections, inspected 2026-10-25. This manufacturer manual
  describes timer start/stop, 8 kHz/64 kHz bases, eight-bit target counters,
  four-bit readable counters and read clear.
- Anomie, [SPC700 Doc](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/spc700.txt),
  revision `eeb98f1e85b7db6574ca0804455819ec81117198`, timer and CONTROL
  sections, inspected 2026-10-25. This independent hardware research supplies
  the free-running first-stage, 0-to-1 clear, post-increment comparison, zero
  target, running-target wrap and read/write edge semantics used here.

No firmware image, commercial ROM, test ROM or source code from another
emulator was used or indexed.
