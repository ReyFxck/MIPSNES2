# Scheduler

## Scope and current state

The first central scheduler foundation is implemented in `core/scheduler/`. It owns
no subsystem and uses no heap, platform API or host wall clock. Its time unit is one
SNES master clock stored in an unsigned 64-bit counter.

Implemented and host-verified behavior:

- caller-owned fixed storage for up to 64 pending events;
- absolute master-clock timestamps;
- deterministic ordering by timestamp, caller priority and insertion token;
- cancellation by stable token;
- same-timestamp scheduling from callbacks;
- event-dispatch budgets and reentrant-run rejection;
- next-event/event-horizon queries;
- exact rational clock conversion with a carried integer remainder;
- logical-address classification for 5A22 6/8/12-master-clock accesses;
- partial SNES-bus accounting for every meaningful CPU callback;
- an atomic CPU-step bridge that advances the scheduler by the resulting clocks;
- a caller-owned non-interlaced NTSC/PAL H/V master-clock component;
- deterministic HBlank/VBlank, field, NTSC short-line and delayed NMI transitions;
- stable-register H/V/HV timer-match generation into the CPU-I/O TIMEUP latch;
- VBlank automatic-joypad start, busy countdown and atomic result completion;
- coordinated H/V and scheduler advancement in one checked master-clock domain;
- deterministic H/V-before-callback ordering at equal timestamps;
- independent timer/cartridge/expansion IRQ-source aggregation;
- CPU-I/O NMI-edge and IRQ-level delivery at atomic CPU boundaries;
- deterministic replay and CPU IRQ/NMI boundary fixtures.

This is not a complete SNES scheduler. The interpreter does not expose every W65C816
bus/internal cycle, and the partial SNES bus has no complete cartridge map or active
peripherals. Therefore the bridge distinguishes observed address-speed accesses from
the remaining aggregate CPU cycles but does not claim exact cycle placement.

The caller-owned H/V foundation supplies VBlank, HBlank, the distinct delayed NMI-
flag signal and stable-register H/V timer matches to the 5A22 CPU-I/O state. It models
non-interlaced NTSC/PAL frame geometry and the NTSC short scanline without PPU
rendering. The shared coordinator now drives H/V and `mipsnes2_scheduler.now` from
the same elapsed-clock delta. The optional interrupt-aware CPU bridge connects the
CPU-I/O latches through an independent-source aggregator. Complete interrupt timing
is not claimed because the CPU still exposes only atomic instruction boundaries.

## Non-interlaced H/V phase

`mipsnes2_hv_timing` stores caller-owned elapsed master clocks, frame count, H master-
clock phase, V line and field. Normal lines use 1364 clocks. Non-interlaced NTSC uses
262 lines and removes four clocks from line 240 while field is one; PAL uses 312
normal lines. Field toggles at H=1/V=0. HBlank clears at H=1 and begins at H=274.
VBlank begins at line 225, or 240 with overscan selected, and ends at H=0/V=0.
RDNMI/NMI asserts at H=0.5, two master clocks after VBlank begins.

H-only matches use `14 + HTIME * 4` master clocks after the selected scanline's dot
zero, so HTIME 338/339 spill to clocks 2/6 of the following line. V-only and
HV-with-HTIME-zero use 1374 clocks from the previous scanline's dot zero. HV with a
nonzero H value uses the H formula only on VTIME. HTIME values above 339 and VTIME
values outside the regional frame do not match; H=153 is suppressed on the NTSC
short line and on the last line. These rules cover stable register state only.

The API advances only from explicit master-clock deltas and gives identical state for
one-shot and chunked input. The PPU horizontal projection now accounts for six-clock
dots 323/327 on normal lines and their absence on the NTSC short line; the 5A22 timer
comparison formula is unchanged. Interlace, PAL's interlaced long line, dynamic SETINI, exact within-instruction timer
register writes, repeated reprogrammed matches in one scanline, TIMEUP's narrow
read-at-match exception remain absent. Automatic joypad polling currently begins at
the deterministic H=298 point on the first VBlank line and completes 4224 master
clocks later. Real hardware varies the start phase within a measured window, so the
fixed phase is not claimed as final hardware behavior. See
[H/V timing audit](hv-timing-foundation-audit.md),
[H/V timer IRQ audit](hv-irq-foundation-audit.md) and
[master-timing fanout audit](master-timing-fanout-audit.md), plus the
[automatic joypad audit](auto-joypad-audit.md).

The optional PPU counter adapter reads this H/V snapshot at the bus boundary, not at
individual cycles within an atomic instruction. Its external edge handoff must be
synchronized before advancing the shared domain. Exact capture latency and repeated/
held-low behavior remain research work; [counter audit](hv-counter-latch-audit.md)
records the chosen deterministic foundation policies.

## Shared master-clock domain

`mipsnes2_master_timing_run_until` requires the scheduler and H/V component to begin
at the same master timestamp. It advances H/V state to each pending event horizon
before asking the scheduler to dispatch callbacks there. A callback tied with HBlank,
VBlank, NMI-flag or timer-IRQ transition therefore observes the updated H/V/CPU-I/O
state. This is a deterministic provisional ordering rule, not a claim that all future
SNES subsystem ties share that hardware priority.

The caller-provided event budget covers the entire requested advance. If it is
exhausted by same-time insertion, both clocks remain aligned at that event timestamp
and the unconsumed event remains queued. Clock-domain mismatch, time regression,
invalid H/V state and scheduler failures are distinct visible results. Each H/V
advance is committed transactionally, so an H/V overflow/error cannot leave the H/V
clock partially advanced ahead of the scheduler.

`mipsnes2_cpu_timed_step_hv` applies one atomic CPU instruction or interrupt-entry
delta to this common domain and requires the SNES bus to own CPU-I/O state. It does
not interrupt an instruction at an internal event timestamp. H/V transitions that
occur during that instruction can affect the following CPU boundary only, and the
caller must still transfer CPU-I/O IRQ/NMI state into the interpreter inputs.

`mipsnes2_interrupt_lines` supplies that optional transfer without conflating IRQ
sources. Its source bitmask reserves separate internal-timer, cartridge and expansion
levels. Sampling CPU-I/O updates only the internal-timer bit and consumes one latched
NMI edge; other IRQ sources remain intact. Driving the CPU applies the OR of all IRQ
sources and transfers a pending NMI edge once.

`mipsnes2_cpu_timed_step_hv_interrupts` samples and drives these inputs after current-
timestamp events but before the CPU step, then repeats after advancing H/V/scheduler
to the resulting boundary. Thus an already-pending NMI/IRQ participates in the
current step, while one generated during that atomic instruction is visible in CPU
state on return and executes at the next boundary. NMI-versus-IRQ priority remains
owned and tested by the CPU core. Exact PPU/DMA/timer ties inside an instruction are
still unresolved.

## Event and rollover contract

Events scheduled before `now` are rejected. Equal-time events use the lower numeric
priority first; equal priority uses the monotonic insertion token. The scheduler does
not assign hardware subsystem priorities yet because simultaneous PPU/DMA/timer/CPU
ordering still needs subsystem evidence. A callback may add or cancel events, but may
not recursively run the scheduler. The caller supplies a dispatch budget so a broken
same-time reschedule loop returns a visible error instead of hanging.

Timestamp wrap is not silently accepted. A later machine loop must choose a safe
renormalization/checkpoint policy before a 64-bit master timestamp can overflow.
Insertion-token exhaustion is likewise reported instead of reusing an identifier.
At NTSC/PAL master-clock rates, normal execution has ample range, but save-state and
long-run policies remain explicit future work.

`mipsnes2_scheduler_event_horizon` returns the earlier of the requested time and next
event. It establishes the API boundary needed to bound CPU work. The current CPU
cannot yet stop at every bus-cycle boundary. An event inside one atomic timed step is
dispatched with its exact timestamp after the instruction completes and affects the
next CPU boundary. Complete CPU event-horizon integration remains in progress.

## Rational clocks

`mipsnes2_clock_ratio` converts source-domain ticks by an exact positive rational
`numerator / denominator`. It reduces the ratio and carries the division remainder
between calls. Chunked and one-shot conversion therefore produce identical totals
without floating point. PPU, SPC700 and DSP ratios are not assigned yet.

## Ricoh 5A22 access classes

`mipsnes2_5a22_access_master_clocks` classifies a normalized logical 24-bit address.
Low-bank/mirrored WRAM and ordinary ROM/SRAM regions use 8 master clocks; ordinary
I/O uses 6; `$4000-$41FF` in system banks uses 12; FastROM-capable upper ROM windows
use 6 only when the caller supplies an enabled FastROM state. Internal cycles use 6.

`mipsnes2_snes_bus` now applies that classifier to every meaningful read/write and
owns `$420D` bit 0. `mipsnes2_cpu_timed_step` snapshots the CPU/bus counters, executes
one CPU boundary, and subtracts observed accesses from the WDC aggregate cycle count.
The remainder is charged as 6-clock internal cycles before advancing the scheduler.
Visible mismatches, counter regressions, overflow and scheduler-budget errors are
reported. Reset bus accesses are not yet driven through this bridge.

The CPU-I/O math unit uses the processor-cycle domain: each completed SNES CPU-bus
access advances one cycle and the bridge advances the remaining aggregate internal
cycles. It therefore does not change its 8/16-cycle latency with FastROM or slow-I/O
master-clock classes. Result reads sample before advancing their access cycle and a
trigger write begins only after an older operation has advanced through that write
cycle. Exact internal-cycle positions are still unavailable, so result observation
inside an instruction with earlier internal cycles remains outside the verified
contract.

The mapping expectations come from fullsnes' hardware-research sections **SNES Timing
Oscillators** and **CPU Clock Cycles / SNES Memory Speed**, cross-checked against the
WDC W65C816S cycle/bus tables. fullsnes identifies the SNES master clocks and the
5A22's address-dependent 6/8/12 divisors; WDC defines processor cycles but not the
SNES-specific memory-speed decoder. No emulator implementation was used.

Still required before total instruction timing is valid:

- every opcode's program/data/dummy/internal cycle sequence;
- WRAM refresh stalls;
- DMA/HDMA bus ownership;
- interrupt sampling at hardware-observable boundaries;
- PPU, SPC700/DSP and enhancement clock domains.

## Verification criteria

`tests/test_scheduler.c` covers timestamp/priority/insertion ordering, cancellation,
capacity, same-time callback insertion, dispatch budgets, time regression, token and
clock overflow, exact fractional carry, chunked/one-shot drift equivalence, timing-map
boundaries, identical replay traces and scheduled IRQ/NMI delivery to a waiting CPU.
`tests/test_snes_bus.c` additionally covers WRAM/MMIO mapping and access timing,
FastROM transitions, slow and mixed-speed CPU instructions, an event inside an atomic
instruction, NMI-entry elapsed clocks and timing-bridge error paths.
`tests/test_hv_timing.c` covers non-interlaced NTSC/PAL line and frame geometry,
HBlank/VBlank boundaries, delayed NMI assertion, overscan, field/short-line behavior,
overflow handling, every valid HTIME value, V/HV match positions, short/final-line
exceptions, out-of-range compares and one-shot/chunked determinism with IRQ enabled.
It also covers exact automatic-joypad start/completion boundaries, normal/overscan
VBlank selection, late enable, disable while busy, snapshots and chunk equivalence.
`tests/test_master_timing.c` covers domain validation, plain and chunked advancement,
H/V-before-callback observation at an equal timestamp, event-budget exhaustion,
same-time insertion, reentrant-run rejection, failure-state alignment and exact
automatic-joypad boundaries. The SNES-
bus fixture also drives repeated NOPs through the H/V-aware CPU bridge and verifies
clock lockstep and timer-latch assertion without pretending that the CPU IRQ line is
connected automatically.
`tests/test_interrupt_lines.c` covers source set/clear independence, CPU-I/O timer
sampling, NMI edge transfer, TIMEUP acknowledgement with cartridge IRQ retained and
CPU input driving. The SNES-bus integration fixture covers timer assertion after an
atomic step, external-source preservation/clear, NMI delivery and simultaneous
NMI-plus-IRQ priority into the existing NMI vector path.
CPU-I/O and SNES-bus fixtures additionally cover 8/16-cycle math completion,
old-result boundaries, division by zero, bus/internal-cycle advancement and an
executed STA/NOP/LDA multiply sequence. They cover the four automatic-joypad result
words, read-only behavior, snapshot isolation and mirrored SNES-bus reads.

Future tests must add simultaneous PPU/IRQ/NMI/DMA cases, CPU pause/resume at actual
bus cycles, interlaced frame boundaries, exact mid-instruction timer-register writes,
the TIMEUP read-at-match exception and long integrated regional-clock runs. Same
ROM/state/input must yield the same trace regardless of host speed.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Unchecked items
remain incomplete or unverified; document implementation evidence before promotion.

- [x] Master timestamp units
- [x] Clock ratio representation
- [x] Event tie ordering
- [ ] [IN PROGRESS] CPU event horizon — atomic timed steps exist; mid-instruction bus-cycle bounding pending
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [x] Determinism tests — scheduler foundation replay
- [x] Shared CPU/scheduler/H/V domain — atomic fanout and failure alignment verified
- [x] Atomic CPU-I/O interrupt-line handoff — independent IRQ sources and NMI transfer verified
- [x] Deterministic automatic-joypad foundation — fixed VBlank start, 4224-clock busy window and atomic results verified
- [ ] [IN PROGRESS] Long-run clock drift tests — ratio, H/V chunking and short shared-domain runs verified; regional soak tests pending

## Research and decisions

Sources:

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf),
  chapter 15 and internal pages 2-28-1 through 2-28-2 for 8/16-machine-cycle
  math-unit completion, inspected 2026-10-20;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), sections
  `SNES Timing Oscillators`, `CPU Clock Cycles / SNES Memory Speed`, `SNES Timing H/V
  Counters` and `SNES Timing H/V Events`, inspected 2026-10-19;
- [Super Famicom Development Wiki: Timing](https://wiki.superfamicom.org/timing),
  `Interrupts`, inspected 2026-10-19, for H/V timer formulas and exceptions;
- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
  pinned at repository revision `eeb98f1e85b7db6574ca0804455819ec81117198`;
- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  timing control, bus-signal descriptions and operation tables.

Intentional approximation: meaningful callbacks use their actual logical-address
speed, while aggregate CPU cycles with no callback are temporarily treated as
6-clock internal cycles. This omits dummy-access addresses and exact within-instruction
placement. It must be replaced as the CPU exposes complete bus-cycle sequences; it
must not be used to claim cycle-perfect timing or real-console performance.
