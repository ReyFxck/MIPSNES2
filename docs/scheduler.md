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
- a resumable CPU-operation/5A22 bridge plus the atomic compatibility bridge;
- a caller-owned non-interlaced NTSC/PAL H/V master-clock component;
- deterministic HBlank/VBlank, field, NTSC short-line and delayed NMI transitions;
- stable-register H/V/HV timer-match generation into the CPU-I/O TIMEUP latch;
- VBlank automatic-joypad start, busy countdown and atomic result completion;
- coordinated H/V and scheduler advancement in one checked master-clock domain;
- deterministic H/V-before-callback ordering at equal timestamps;
- independent timer/cartridge/expansion IRQ-source aggregation;
- CPU-I/O NMI-edge and IRQ-level delivery at instruction boundaries;
- deterministic replay, event-budget retry and CPU IRQ/NMI boundary fixtures.

This is not a complete SNES scheduler. The cursor does not expose every W65C816
dummy/electrical bus phase, and the partial SNES bus has no complete cartridge map
or active peripherals. Its operation-level timing does not claim electrical accuracy.

The caller-owned H/V foundation supplies VBlank, HBlank, the distinct delayed NMI-
flag signal and stable-register H/V timer matches to the 5A22 CPU-I/O state. It models
non-interlaced NTSC/PAL frame geometry and the NTSC short scanline without PPU
rendering. The shared coordinator now drives H/V and `mipsnes2_scheduler.now` from
the same elapsed-clock delta. The optional interrupt-aware CPU bridge connects the
CPU-I/O latches through an independent-source aggregator. Complete interrupt timing
is not claimed because CPU input delivery and electrical sampling remain distinct.

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

`mipsnes2_hv_clocks_until_frame_boundary` reports the exact distance from the
current valid H/V phase to the next frame boundary without mutating state. It
accounts for the field toggle at H=1/V=0 and the NTSC field-one short line, so a
platform can request an absolute machine horizon without assuming every frame is
`262 × 1364` clocks. It does not pace execution or consult a display/audio device.

The optional PPU counter adapter reads this H/V snapshot at the bus boundary, not at
individual electrical cycles within an operation. Its external edge handoff must be
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

`mipsnes2_cpu_timed_step_hv` remains an atomic compatibility API. The opt-in
`mipsnes2_cpu_timed_resume_hv` instead advances one in-flight CPU operation through
the common domain, stopping at requested horizons and retaining its original endpoint
after an event-budget failure. Both require the SNES bus to own CPU-I/O state.

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

The resumable interrupt-aware variant samples CPU-I/O edges during an in-flight
operation, without mutating CPU inputs until the instruction completes. Events due
at the current boundary are dispatched before the cursor begins; a mid-operation NMI
cannot retroactively replace an already selected instruction. The following boundary
retains the CPU's NMI-over-IRQ priority. This is a logical handoff, not an assertion
about exact electrical sampling.

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

`mipsnes2_scheduler_event_horizon` returns the earlier of requested time and the next
event. The caller-owned timed state can hold a READ, WRITE or INTERNAL operation in
flight across that horizon: scheduler/H/V advance to each event timestamp, while its
CPU/bus effect remains uncommitted until the original operation endpoint. An event
exactly at the endpoint is dispatched first. This avoids recharging an 8-clock access
as another 8 clocks after an event at clock 4. Requested horizon returns do not
discard the operation; the next call resumes the remaining clocks.

## Rational clocks

`mipsnes2_clock_ratio` converts source-domain ticks by an exact positive rational
`numerator / denominator`. It reduces the ratio and carries the division remainder
between calls. Chunked and one-shot conversion therefore produce identical totals
without floating point. The APU now applies it to nominal 1.024 MHz S-SMP time
against regional SNES master clocks; PPU subdivision remains separate.

## Ricoh 5A22 access classes

`mipsnes2_5a22_access_master_clocks` classifies a normalized logical 24-bit address.
Low-bank/mirrored WRAM and ordinary ROM/SRAM regions use 8 master clocks; ordinary
I/O uses 6; `$4000-$41FF` in system banks uses 12; FastROM-capable upper ROM windows
use 6 only when the caller supplies an enabled FastROM state. Internal cycles use 6.

`mipsnes2_snes_bus` applies that classifier to meaningful reads/writes and owns
`$420D` bit 0. The new bridge peeks without bus effects, latches the access cost and
FastROM state at operation start, advances time, then commits one descriptor. INTERNAL
always costs one processor cycle and six master clocks; its `value` is a stale-operation
token, not a cycle count. RESET is explicitly selected and advances its two vector
reads without incrementing `instruction_cycles`. The atomic compatibility functions
still compute remaining internal cycles from whole-instruction counters.

Scheduler dispatch from inside the instruction-atomic CPU was attempted and rejected.
The replacement advances time outside the CPU, leaves an interrupted operation
uncommitted, and resumes without duplicating already executed callbacks or CPU
effects. A callback already dispatched before budget exhaustion cannot be rolled
back. Callbacks must not mutate CPU/cursor/FastROM/bus counters during an in-flight
operation; stale descriptors/counters are rejected before its commit. Active-cursor
cancellation/checkpoint recovery is not implemented. The same resumable path now
arbitrates CPU-triggered general DMA and WRAM refresh; HDMA remains absent.
The timing-state ABORT pulse is a narrow exception: after opcode fetch it latches
without mutating the active CPU descriptor. A pending write blocks at its original
timed endpoint; a future write blocks before admission until the caller qualifies
that byte as externally accepted or inhibited;
the latter clocks the bus without changing its target. This is a caller-supplied
external policy, not claimed 5A22 write-gate behavior. See the
[mid-instruction ABORT audit](65c816-mid-instruction-abort-slice-audit.md).
The [write-qualification audit](65c816-abort-write-qualification-audit.md)
records recovery, tests and outstanding RMW electrical/RTI exceptions.
See the
[access-timing post-mortem](cpu-access-timing-postmortem.md),
[cycle-contract audit](65c816-cycle-contract-audit.md) and
[interrupt-cycle audit](65c816-interrupt-cycle-audit.md), plus the
[subroutine-cycle audit](65c816-subroutine-cycle-audit.md) and the
[long-subroutine-cycle audit](65c816-long-subroutine-cycle-audit.md), then the
[indexed-subroutine-cycle audit](65c816-indexed-subroutine-cycle-audit.md) and the
[jump-cycle audit](65c816-jump-cycle-audit.md), then the
[resumable branch-cycle audit](65c816-resumable-branch-cycle-audit.md) and the
[stack-cycle audit](65c816-stack-cycle-audit.md), followed by the
[implied-cycle audit](65c816-implied-cycle-audit.md) and the
[read/modify/write-cycle audit](65c816-rmw-cycle-audit.md), then the
[load/store-cycle audit](65c816-load-store-cycle-audit.md) and the
[complete opcode-cycle audit](65c816-final-resumable-cycle-audit.md), the
[RESET-cycle audit](65c816-reset-cycle-audit.md) and the
[operation-timing audit](cpu-operation-timing-audit.md).

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
- HDMA bus ownership and overlap with general DMA/refresh;
- interrupt sampling at hardware-observable boundaries;
- PPU, SPC700/DSP and enhancement clock domains.

## Verification criteria

`tests/test_scheduler.c` covers timestamp/priority/insertion ordering, cancellation,
capacity, same-time callback insertion, dispatch budgets, time regression, token and
clock overflow, exact fractional carry, chunked/one-shot drift equivalence, timing-map
boundaries, identical replay traces and scheduled IRQ/NMI delivery to a waiting CPU.
`tests/test_snes_bus.c` additionally covers WRAM/MMIO mapping and access timing,
FastROM transitions, slow/mixed-speed instructions, atomic compatibility, per-clock
resume, event at/inside an operation endpoint, event-budget retry before a write,
RESET vector timing, overflow/stale rejection and boundary IRQ/NMI priority.
`tests/test_dma_timing.c` covers shared CPU/general-DMA refresh stalls, H=538/H=534
alignment, the NTSC short line, horizon resume and distinct clock accounting.
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

Future tests must add simultaneous PPU/IRQ/NMI/DMA cases, electrical bus-phase
placement, interlaced frame boundaries, exact mid-instruction timer-register writes,
the TIMEUP read-at-match exception and long integrated regional-clock runs. Same
ROM/state/input must yield the same trace regardless of host speed.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Unchecked items
remain incomplete or unverified; document implementation evidence before promotion.

- [x] Master timestamp units
- [x] Clock ratio representation
- [x] Event tie ordering
- [x] CPU event horizon — caller-owned in-flight operation state advances through events, pauses on requested horizons/errors and commits exactly once
- [ ] PPU event integration
- [ ] [IN PROGRESS] SPC700 synchronization — rational machine-time budget and atomic instruction admission integrated; exact bus phases/races pending
- [ ] [IN PROGRESS] DSP synchronization — common S-SMP cycle domain and 32-cycle event cadence integrated; pipeline phases pending
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
