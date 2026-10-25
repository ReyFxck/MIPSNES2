# S-CPU/S-SMP communication-port audit

Date: 2026-10-25

## Scope

`core/apu/io_ports.*` adds the platform-neutral mailbox shared by the future
S-SMP core and the existing S-CPU bus. It owns eight independent byte latches:
four written by the S-CPU and read by the S-SMP, and four written by the S-SMP
and read by the S-CPU. CPU addresses `$2140-$217F` mirror the four ports every
four addresses. The S-SMP-facing API uses indices 0-3 for its `$F4-$F7` side.

The machine owns this state when no external APU callback is supplied. Existing
explicit callbacks retain precedence so the public composition contract is not
silently broken. Power-on resets the internal latches to zero as a deterministic
core policy. The implementation allocates no memory, imports no target API and
is compiled identically by host, PS2 and PSP build graphs.

## Deterministic verification

`tests/test_apu_io_ports.c` verifies all four ports in both directions,
directional independence, overwrites, CPU mirrors, the bus-interface adapter,
zero initialization and invalid inputs. `tests/test_machine.c` verifies the
default machine route through mirrored SNES banks, an S-SMP-side response seen
by the CPU, independence of opposite-direction latches and power-on reset.

The block is complete when the default host suite and repository checker pass
and pinned PS2 and PSP cross-build CI accepts the new shared object. No target
runtime or audio-output claim follows from cross-compilation.

Local verification on 2026-10-25 passed the default C99 suite, an
`-O3 -DNDEBUG -fstrict-aliasing` suite, the repository checker and a GCC
`-fanalyzer` build. ASan/UBSan builds completed and both the new port fixture
and machine integration fixture passed with leak detection disabled. The full
sanitized runner was interrupted by pre-existing generated test files losing
their executable bit twice; rebuilding the affected target advanced the suite,
so this is recorded as an environment artifact rather than a clean full-suite
sanitizer pass. Pinned target CI is pending publication.

Implementation revision `64e8f24` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420524),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420538) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420504).
No console, emulator or audio runtime was executed for this block.

## Deliberate limits

This is logical mailbox state only. There is no SPC700 interpreter, S-SMP RAM,
IPL, timer, DSP, audio sample generation or clock-domain scheduler. A write is
visible immediately to the opposite-side API; real asynchronous sampling and
same-time race ordering remain part of the later CPU/APU synchronization task.
Initial hardware latch values are not claimed, and external-callback machines
continue to own their own port behavior.

## Sources

- Nintendo, [*SNES Development Manual, Book I*](https://floating.muncher.se/bot/manual/book1_text.pdf),
  PPU register page 2-27-24 for `$2140-$2143`, inspected 2026-10-25. It states
  that the same addresses access different input and output registers, giving
  eight registers in total.
- [SNESdev S-SMP hardware documentation](https://snes.nesdev.org/wiki/S-SMP),
  communication-port section, inspected 2026-10-25, used to cross-check the
  four one-way latches on each side and the S-SMP `$F4-$F7` mapping.

No source code from another emulator, IPL image, commercial ROM or game-specific
trace was used or indexed.
