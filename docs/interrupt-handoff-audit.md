# CPU-I/O Interrupt Handoff Audit

Date: 2026-10-19

## Scope

This milestone connects the existing Ricoh 5A22 CPU-I/O NMI/IRQ latches to the
existing 65C816 input contract at explicit atomic CPU boundaries. It introduces a
small caller-owned aggregation object rather than allowing the internal timer to
overwrite future cartridge or expansion IRQ sources.

Implemented behavior:

- independent level bits for the internal H/V timer, cartridge and expansion IRQ
  sources;
- source assertion/deassertion changes only the selected bits;
- CPU-I/O sampling updates only the internal-timer source;
- reading/acknowledging TIMEUP can therefore clear the timer source without erasing
  an asserted cartridge/expansion IRQ;
- one CPU-I/O NMI edge transfers into an aggregation latch and then into the existing
  CPU pending-NMI latch;
- the aggregate OR of all IRQ sources drives the existing level-sensitive CPU input;
- the interrupt-aware H/V timing bridge samples/drives inputs both before and after
  one atomic CPU step;
- an input pending before the step can enter the CPU at that boundary, while an input
  generated during the step is visible for the following boundary;
- simultaneous delivered NMI and unmasked IRQ follows the CPU core's already tested
  NMI priority.

All state is caller-owned and uses no heap, host clock, platform API or physical
target pointer. This layer is emulator wiring, not a fabricated SNES hardware block.

## Deliberate limits

- The CPU interpreter still exposes one whole instruction or interrupt entry as its
  smallest schedulable unit.
- Exact H/V timer/NMI sampling within an instruction is therefore absent.
- The source names establish aggregation capacity; cartridge and expansion devices
  do not yet exist or drive those sources in an integrated machine.
- ABORT, RESET signal sequencing, DMA/HDMA arbitration, PPU event priority and
  enhancement-chip IRQ behavior remain outside this milestone.
- The CPU-I/O timer latch still has the stable-register and TIMEUP match-window limits
  documented in the H/V IRQ audit.
- No real-console timing, compatibility or performance claim follows from this
  host-tested handoff.

## Tests

`tests/test_interrupt_lines.c` verifies initialization and invalid arguments,
independent source set/clear, aggregate CPU level, one-shot NMI transfer, CPU-I/O
timer/NMI sampling and TIMEUP acknowledgement while a cartridge IRQ remains asserted.

`tests/test_snes_bus.c` additionally verifies:

- four timed NOPs cross the configured H-timer match and deliver its IRQ level only
  after the matching atomic step;
- TIMEUP acknowledgement clears the timer source while a cartridge source keeps the
  CPU IRQ level asserted;
- clearing the final external source lowers the CPU IRQ input;
- null interrupt/H/V integration arguments fail visibly;
- advancing to the non-interlaced VBlank NMI boundary then executing an interrupt-
  aware step transfers NMI and enters the NMI vector;
- a simultaneous unmasked cartridge IRQ remains asserted while NMI wins that CPU
  boundary.

Verified locally on 2026-10-19 with GCC 13.3.0:

- `make clean && make test` — passed;
- `make BUILD_DIR=build/release-interrupts CC=gcc
  CFLAGS='-O2 -g -DNDEBUG' clean test` — passed;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-interrupts CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' test` — passed after the recovery described
  below;
- `python3 tools/check_repository.py` and `git diff --check` are included in the
  pre-publication audit;
- `sha256sum LICENSE` remained
  `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

The first clean sanitizer invocation compiled successfully but stopped when one
generated `test-scheduler` ELF unexpectedly had mode `0644`. A forced rebuild of that
target restored mode `0755`; its isolated run and then the complete sanitizer suite
passed. No source or build-rule change was required. This is recorded as a transient
local filesystem artifact, not hidden or counted as a first-pass success.

LeakSanitizer is disabled locally because this environment cannot inspect the
required `/proc` state. Local Clang, PS2SDK and PSPSDK are unavailable. GitHub Actions
evidence is pending, so the narrow handoff checkbox remains in progress. No emulator
or real PS2/PSP executed the code.

## Evidence basis

No new hardware timing formula is introduced. The handoff composes the already
sourced CPU interrupt-boundary behavior, CPU-I/O latch behavior and H/V timing:

- [WDC W65C816S Datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
  interrupt input and vector behavior;
- [fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm), sections
  `SNES CPU Interrupts` and `SNES Timing H/V Events`, inspected 2026-10-19;
- [Super Famicom Development Wiki: Timing](https://wiki.superfamicom.org/timing),
  `Interrupts`, inspected 2026-10-19.

These are manufacturer or independent hardware-research sources, not emulator
implementations. No emulator source or commercial ROM was consulted or copied.
