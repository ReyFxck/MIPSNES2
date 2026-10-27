# S-SMP local memory-map audit

Date: 2026-10-25

The original boundary below predates the timer and DSP blocks. `$F1/$FA-$FF` are
implemented as described by the later [S-SMP timer audit](smp-timer-audit.md), and
`$F2/$F3` by the [APU clock/DSP register audit](apu-clock-dsp-register-audit.md).
TEST remains unsupported.

## Implemented boundary

`core/apu/spc700/memory.[ch]` borrows a 64 KiB ARAM buffer, the existing
directional CPU/APU mailbox and optionally exactly 64 caller-owned IPL bytes.
It allocates nothing. Reads outside `$F0-$FF` normally access ARAM; reads
`$FFC0-$FFFF` use the IPL image while `$F1` bit 7 is set. Writes to this high
range always update ARAM. Without an IPL image, a read while the overlay is
enabled returns failure, not fabricated firmware data. Power-on selects the
overlay and retains ARAM. Tests use a generated synthetic byte pattern, not
the console IPL.

S-SMP reads `$F4-$F7` see the S-CPU-to-S-SMP latches; writes update the
opposite latches. A write to `$F1` with bit 4 or 5 clears the corresponding
pair of incoming latches without altering outgoing latches. `$F8/$F9` are
ordinary ARAM. Handled `$F1/$F4-$F7` writes also update underlying ARAM, while
reads retain their register meaning. The current tree routes `$F2/$F3` to an
attached S-DSP register state and `$FA-$FF` to timers. `$F0` is rejected in both
directions and reads of `$F1` remain rejected.

The shared machine accepts optional ARAM/IPL buffers and rejects inconsistent
sizes or an external APU callback together with internal ARAM, avoiding a
disconnected mailbox. The PS2 ROM path reserves 64 KiB static EE ARAM. PSP
builds the same shared map but still has no ROM or audio runtime.

## Verification and limits

`tests/test_smp_memory.c` covers boundaries at `$00EF/$0100/$FFBF/$FFC0`,
both ends of the IPL overlay, write-through, remapping, all four ports, pair
clear and output retention, `$F8/$F9`, unsupported MMIO and invalid buffers.
`tests/test_machine.c` covers size rejection and a CPU-bus/S-SMP-map exchange
through the shared mailbox, then power-cycle retention and reset.

Host `make test` and `make check` passed with C99 `cc` on 2026-10-25.
Pinned PS2/PSP CI cross-builds are pending publication. No physical console,
PCSX2, ARMSX2, PPSSPP or NetherSX2 run was performed for this block.

This audit originally introduced a strict partial map without an instruction
engine. The current tree now has register state, timers and an instruction-atomic
256-opcode SPC700 engine, nominal synchronized clock and initial DSP register layer,
but still cannot run a complete audio pipeline: there are no voices, PCM samples or
interrupt/reset entry. Input clear
is immediate in this API, whereas the
sound manual describes a next-cycle clear and undefined results when an
S-CPU write races it. Those electrical details remain unverified here.
Exact power-on ARAM contents are not claimed. Real firmware provenance and
user acquisition remain open; the repository includes none.

## Hardware sources

- [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual),
  memory-map, peripheral-register and control-register sections, inspected
  2026-10-25. Manufacturer document for 64 KiB address space, `$F0-$FF`
  register region, four port directions and `$F1` input clear. The linked
  transcription does not by itself settle timing races.
- [SNESdev S-SMP hardware page](https://snes.nesdev.org/wiki/S-SMP),
  memory layout and control-register table, inspected 2026-10-25.
  Community hardware description for the IPL read overlay and underlying
  ARAM writes; the full page was partially inaccessible to the web fetcher,
  so only the indexed excerpts were used.
- [Anomie's SPC700 Doc](https://github.com/gilligan/snesdev/blob/master/docs/spc700.txt),
  register-overlay description, inspected 2026-10-25 as independent hardware
  research for underlying ARAM writes at handled MMIO addresses. No emulator
  source or implementation code was used or indexed.

No firmware image, commercial ROM or other emulator's source was used.
