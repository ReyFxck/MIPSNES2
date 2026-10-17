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
- deterministic replay and CPU IRQ/NMI boundary fixtures.

This is not a complete SNES scheduler. The interpreter does not expose every W65C816
bus/internal cycle, and the partial SNES bus has no complete cartridge map or active
peripherals. Therefore the bridge distinguishes observed address-speed accesses from
the remaining aggregate CPU cycles but does not claim exact cycle placement.

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

Future tests must add simultaneous PPU/IRQ/NMI/DMA cases, CPU pause/resume at actual
bus cycles, scanline/frame boundaries and long regional-clock runs. Same ROM/state/
input must yield the same trace regardless of host speed.

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
- [ ] [IN PROGRESS] Long-run clock drift tests — ratio chunking verified; integrated regional clocks pending

## Research and decisions

Sources:

- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), sections
  `SNES Timing Oscillators` and `CPU Clock Cycles / SNES Memory Speed`, inspected
  2026-10-17;
- [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
  pinned at repository revision `eeb98f1e85b7db6574ca0804455819ec81117198`;
- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  timing control, bus-signal descriptions and operation tables.

Intentional approximation: meaningful callbacks use their actual logical-address
speed, while aggregate CPU cycles with no callback are temporarily treated as
6-clock internal cycles. This omits dummy-access addresses and exact within-instruction
placement. It must be replaced as the CPU exposes complete bus-cycle sequences; it
must not be used to claim cycle-perfect timing or real-console performance.
