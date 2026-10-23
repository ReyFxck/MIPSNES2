# Mid-instruction ABORT read-only slice — 2026-10-23

## Scope and hardware evidence

The opt-in scheduler/CPU operation bridge now accepts an ABORT pulse after the
opcode fetch and before any write of the active instruction. The pulse can be
latched by a scheduler callback without changing the active CPU descriptor.
The operation sequence finishes with its elapsed clocks and read side effects;
the bridge then restores programmer-visible registers from the pre-fetch
checkpoint and latches ABORT for the following interrupt-entry cursor. The
return PC is the address of the aborted opcode, not its successor. The existing
boundary cursor selects ABORT before NMI and IRQ.

Primary reference: Western Design Center, *W65C816S Datasheet*, March 13,
2024, sections 2.13 (pulse, internal-register preservation, opcode return
address), 7.4 (exceptions and timing limitations), 7.19 (RESET > ABORT > NMI >
IRQ) and Table 5-7 entry 22a (hardware interrupt sequence):
<https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf>.
These W65C816S observations are used as a programmer-visible foundation for
the Ricoh 5A22, not as proof of its electrical timing. No emulator source or
test asset was imported.

This is deliberately **not** full mid-instruction ABORT. A pulse during opcode
fetch, RESET, hardware interrupt entry, BRK/COP, RTI or WAI/STP is rejected by
the new timing API. A pulse after a committed write or during an in-flight
write is rejected. If a later operation would write, resume returns
`UNSUPPORTED_OPERATION` *before* committing it and retains the active cursor;
the caller must reset/reconstruct the machine rather than pretending the
MMIO/stack write was undone. The bridge does not model MLB, VDA/VPA, PHI2 pulse
phase, write qualification, dummy traffic or electrical ABORT suppression.
Its CPU registers are incrementally visible to scheduler callbacks before the
final restoration; this is not a claim of signal-level register inhibition.
No backend-specific or title-specific path was introduced.

## Deterministic tests and validations

`tests/test_snes_bus.c` tests rejection before fetch, a pulse during an
in-flight immediate operand read, restored A/PC and unchanged read accounting,
ABORT priority over retained NMI/IRQ with stacked opcode PC, fail-closed
handling before a WRAM-port write, and two same-timestamp pulse callbacks
across event-budget exhaustion without duplicate reads or events.

On implementation revision `b406689e886067a43cd2c2387c4bbf5057972044`,
the local GCC host passed `make BUILD_DIR=build/abort-slice -j2 test`,
`make BUILD_DIR=build/abort-release CFLAGS='-O3 -DNDEBUG' -j2 test` and
`ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/abort-sanitize
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
LDFLAGS='-fsanitize=address,undefined' -j2 test`. For the local sanitizer runtime
`ASAN_OPTIONS=detect_leaks=0` was required by the existing `/proc` limitation;
this does not claim a LeakSanitizer pass. Pinned PS2SDK/toolchain v2.0.0 and
PSPSDK/toolchain v20261001 cross-builds passed, producing
`platform/ps2/mipsnes2-ps2.elf` and `platform/psp/EBOOT.PBP`.
Pinned XERJ `v1.0.0-rc.97`, with feedback disabled and a loopback-only
temporary node, indexed only this checkout: 160 files, 1,317 live records,
54/54 code files and zero code junk. A first-party search located the new
ABORT API, implementation and deterministic test. XERJ is retrieval tooling,
not behavioral evidence.
Neither executable was run on a console. No performance or game compatibility
claim follows from these builds.

GitHub Actions for implementation revision `b406689` was inspected, rather than
inferred from workflow files:

- [Host CI run 40008423252](https://github.com/ReyFxck/MIPSNES2/actions/runs/40008423252)
  — SUCCESS, GCC and Clang jobs; two nonempty host artifacts;
- [PS2 CI run 40008423202](https://github.com/ReyFxck/MIPSNES2/actions/runs/40008423202)
  — SUCCESS, nonempty target artifact;
- [PSP CI run 40008423180](https://github.com/ReyFxck/MIPSNES2/actions/runs/40008423180)
  — SUCCESS, nonempty target artifact.

## Next primary milestone

Resolve external-write qualification and a recoverable mid-instruction ABORT
path for instructions with writes, including RMW/interrupt exceptions, with
manufacturer-backed timing and deterministic MMIO/stack tests. The broader
ABORT and timing tasks remain unchecked until that work and hardware validation.
