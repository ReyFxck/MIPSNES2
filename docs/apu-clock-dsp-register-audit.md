# Deterministic APU clock and S-DSP register audit

Date: 2026-10-27

## Implemented boundary

This block connects the existing 256-opcode instruction-atomic SPC700 to a
deterministic APU clock domain and starts an independently testable S-DSP path.
It adds no game-specific behavior, firmware, commercial ROM data or source code
from another emulator.

`core/apu/timing.[ch]` converts elapsed SNES master clocks to nominal S-SMP
cycles with the existing reduced rational converter and carried remainder. The
policy uses 1,024,000 S-SMP cycles per second against 21,477,273 NTSC or
21,281,370 PAL master clocks per second. One-shot and arbitrarily chunked calls
therefore produce identical integer totals without host time or floating point.
These are deterministic nominal frequencies, not a claim that two physical
oscillators have zero drift or a fixed electrical phase.

The bridge admits complete atomic SPC700 instructions only after their maximum
documented cost is available. Conditional instructions reserve the taken cost;
an untaken branch leaves the two unused cycles as credit for later work. The new
prepare/clocked-step interface caches the opcode so a side-effectful MMIO fetch
is not repeated, while the historical standalone step still advances timers.
Timers and S-DSP time continue while the SPC700 is sleeping or stopped. A caller
may bound instruction work; remaining time becomes explicit credit rather than
changing the elapsed device-clock total.

`mipsnes2_machine_resume` now catches the APU domain up to the scheduler's
absolute master-clock timestamp. Machine configuration may supply explicit
initial SPC700 registers, which are copied. No undocumented A/X/Y/SP/PSW reset
values are invented. Without that seed, ARAM, DSP registers, timers and sample
cadence still exist, but the machine does not silently choose CPU reset state.

`core/apu/dsp/dsp.[ch]` currently owns:

- a 128-byte register file and full eight-bit `$F2` address latch;
- `$F3` access through the low seven address bits;
- address-bit-7 write inhibition;
- write-to-clear behavior for `ENDX` (`$7C`);
- an internal `FLG` reset/mute/echo-disable value of `$E0`, kept distinct from
  the deliberately unclaimed readable power-on shadow;
- a fixed one-sample-event-per-32-S-SMP-cycle cadence.

The S-SMP map routes `$F2/$F3` to that state and preserves its existing
write-through-to-ARAM contract. This establishes an interconnect and timebase;
it does not synthesize PCM.

## Verification

`tests/test_dsp.c` covers address/data access, low-seven-bit reads, inhibited
writes, `FLG`, `ENDX`, 32-cycle boundaries, chunk equivalence and overflow.
`tests/test_apu_timing.c` covers a full nominal NTSC second (1,024,000 S-SMP
cycles and 32,000 sample events), irregular chunk equivalence, instruction
credit, bounded catch-up, timer coordination, SLEEP and overflow/regression
rejection. `tests/test_smp_memory.c` covers `$F2/$F3`. The machine fixture boots
an independently authored synthetic IPL sequence which writes an S-DSP register
through real SPC700 instructions while ordinary 5A22 resume calls advance time.

Host `make clean && make -j2 test`, focused ASan/UBSan, GCC `-fanalyzer` and
repository consistency checks are required for this block. PS2/PSP CI proves
cross-compilation only; no physical-console, emulator-runtime or audible-output
result is claimed here.

## Deliberate limits

Instruction effects still commit atomically rather than at exact SPC700 bus
phases. Conservative admission can delay an untaken conditional instruction by
up to its two-cycle taken penalty. Same-time S-CPU/S-SMP mailbox races, measured
oscillator drift, SPC700 reset/interrupt entry and TEST-register behavior remain
open.

S-DSP voice state, KON/KOFF polling cadence, BRR, pitch, interpolation,
envelopes, noise, modulation, mixing, echo/FIR, clipping and sample values remain
unimplemented. Apart from `ENDX` and `FLG`, register side effects and within-
sample-loop write visibility are not claimed. `sample_count` is a cadence event
counter, not PCM output or a golden-audio result.

## Hardware sources

- [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual), sound-CPU
  peripheral registers and sound-DSP register-map sections, consulted
  2026-10-27. Manufacturer documentation for `$F2/$F3` and the 128-register
  address space.
- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
  sound-DSP register-map appendix, consulted 2026-10-27 as a first-party
  cross-check. No manual bytes are committed.
- [SNESdev Wiki: S-DSP registers](https://snes.nesdev.org/wiki/S-DSP_registers),
  consulted 2026-10-27 for independently researched address-bit-7 write
  inhibition, `ENDX` acknowledgement, internal reset flags and the 32-cycle
  sample-loop timing caveat.
- [Super Famicom Development Wiki: SPC700 reference](https://wiki.superfamicom.org/spc700-reference),
  consulted 2026-10-27 for the nominal 1.024 MHz S-SMP rate and interface
  cross-check.

No firmware image, test ROM, commercial ROM or implementation source from
another emulator was used.
