# ABORT write qualification and resumable recovery — 2026-10-23

## Decision and evidence

WDC, *W65C816S Datasheet*, March 13, 2024, section 2.13 describes an
ABORT pulse preserving internal register results until the current instruction
finishes and stacking its opcode address. Section 7.4 warns of cycle-dependent
exceptions, especially status after the RMW modify cycle and register effects
during RTI/interrupt entry; section 7.19 orders RESET, ABORT, NMI and IRQ.
Table 5-7 entry 22a describes the interrupt bus sequence.
Primary source: <https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf>.
These are W65C816S claims; Ricoh 5A22 electrical phase and external write-gating
on real SNES hardware remain unverified. No source from another emulator was
used. The tests below are original synthetic fixtures, not imported ROMs.

The datasheet does **not** prove that an external MMIO/WRAM write can be undone.
The operation bridge therefore returns `MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED`
at a reached write following an accepted mid-instruction ABORT. Its caller must
explicitly qualify *each byte* as either `EXTERNAL` (the target accepts the
write; its side effect persists) or `INHIBIT` (the modeled external gate rejects
the target effect). A qualified inhibited write still consumes one bus access,
its original latched 5A22 clock cost, one CPU-I/O math processor cycle and the
written value on the current deterministic open-bus latch. It does not call any
WRAM, `$2180`, PPU, APU, cartridge or other device write route. This is a
configurable machine-wiring abstraction, **not** an automatic 5A22 ABORT rule.

The CPU cursor executes the original write phase through a one-operation
suppressed-write bus callback, then immediately restores its ordinary bus
interface. Its register checkpoint restores the programmer-visible CPU state
after completion, keeping elapsed clocks and the chosen external side effects.
The following instruction boundary enters the existing ABORT cursor. The
default remains blocked until the caller resolves that exact descriptor; a
resume after event-budget failure never recharges or commits it twice.

The CPU cursor records whether the memory RMW modify phase has committed.
A pulse *after* that phase retains status as it was at the pulse, since WDC
section 7.4.1.1 permits status to have changed. A pulse before that phase
restores the pre-fetch status; repeated pulses do not change this first
decision. RTI, BRK/COP, WAI/STP, hardware entry, RESET, opcode-fetch pulses,
and pulses after a committed write remain outside this bounded contract.
MLB/VPA/VDA/PHI2 timing, read side effects, held-low ABORT, dummy traffic,
asynchronous pulses and write-gate behavior on real Ricoh 5A22 hardware still
require direct evidence. There is no machine runner yet to select a real gate
policy, and no real-console runtime or performance claim is made.

## Deterministic tests and completion rule

`tests/test_snes_bus.c` verifies both dispositions on `$2180`, a pulse during an
in-flight write, an endpoint-tied pulse across a one-event dispatch budget,
one-shot per-byte qualification, early RMW inhibition with pre-fetch status,
late RMW status retained at the pulse, a qualified PHA stack write, and two separately
inhibited writes in native 16-bit INC followed by native ABORT vector/stack
entry. It checks original clock endpoints, exact-once access accounting,
absence or presence of external effects and restoration of the opcode PC.

This block is complete when host debug/release/sanitizer tests, pinned PS2 and
PSP cross-builds, first-party repository audit and actual GitHub Actions jobs
pass with a clean `main` tree. That criterion verifies the **explicit write
qualification mechanism**, not full electrical ABORT, all RMW exceptions, console
performance or compatibility. The broad ABORT and timing checkboxes stay open.

## Validation

Local GCC host validation on this checkout:

- `make BUILD_DIR=build/abort-qual -j2 test` — PASS, all host C suites,
  smoke and repository checks;
- `make BUILD_DIR=build/abort-qual-release CFLAGS='-O3 -DNDEBUG' -j2 test`
  — PASS;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/abort-qual-sanitize
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — PASS; local
  LeakSanitizer remains disabled for the documented managed `/proc` limitation;
- pinned PS2 v2.0.0 toolchain/PS2SDK cross-build, `make -C platform/ps2 -j2`
  with documented `PS2DEV`, `PS2SDK` and `PATH` — PASS, ELF generated;
- pinned PSP v20261001 toolchain/PSPSDK cross-build,
  `make -C platform/psp -j2` with documented `PSPDEV` and `PATH` — PASS,
  PRX/PBP generated;
- XERJ v1.0.0-rc.97 first-party-only index on a temporary loopback node,
  with feedback disabled and dry-run before indexing — PASS: 161 files,
  1,329 live records, 54/54 code files indexed, zero code junk; search
  returned the new write-resolution API in the core and header.

The implementation tree `51802cf8471e0ec1b5b17b06ca83de3c5158777d`
was published to `main` as commit `383ceb241b751cafa737ebf3f50fcab4d21f286b`.
Its actual GitHub Actions runs completed successfully:

- [Host CI run 40010651572](https://github.com/ReyFxck/MIPSNES2/actions/runs/40010651572)
  — success; nonempty `mipsnes2-host-gcc-383ceb241b751cafa737ebf3f50fcab4d21f286b`
  artifact (18,301 bytes);
- [PS2 CI run 40010651577](https://github.com/ReyFxck/MIPSNES2/actions/runs/40010651577)
  — success; nonempty `mipsnes2-ps2-383ceb241b751cafa737ebf3f50fcab4d21f286b`
  artifact (615,719 bytes);
- [PSP CI run 40010651609](https://github.com/ReyFxck/MIPSNES2/actions/runs/40010651609)
  — success; nonempty `mipsnes2-psp-383ceb241b751cafa737ebf3f50fcab4d21f286b`
  artifact (126,281 bytes).

No real PS2/PSP hardware, PCSX2/PPSSPP runtime, profiling or SNES ABORT
electrical measurement was performed. Cross-build and CI are not console execution.

## Next primary milestone

Add a real machine bus-ownership/qualification contract, with identified
Ricoh 5A22 hardware evidence or test-ROM measurements for ABORT write timing,
RMW electrical phase and RTI/interrupt exceptions; retain explicit unknowns
until that evidence exists.
