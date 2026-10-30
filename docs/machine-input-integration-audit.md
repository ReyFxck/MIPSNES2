# Machine input integration audit

Date: 2026-10-29

## Scope

This block closes the narrow Phase 9 connection between a platform-translated
standard-pad word and the existing CPU-I/O controller model. The public
`mipsnes2_machine_set_joypad` call accepts one of four SNES serial-order words plus
its connectivity state. It deliberately contains no PS2, PSP or host input API.

## Executable evidence

`tests/test_machine_input.c` builds a valid synthetic LoROM and verifies through a
ready `mipsnes2_machine` that:

- null, uninitialized and out-of-range calls fail without changing state;
- all four injected words are reconstructed through the manual `$4016/$4017`
  strobe-and-shift path;
- post-word bits reflect each injected connectivity flag;
- the H/V event at H=298 on the first normal VBlank line snapshots the same words;
- changing a live word during the 4224-master-clock busy interval does not alter
  the in-flight automatic result;
- `$4218-$421F` commit the original four-word snapshot at completion; and
- `mipsnes2_machine_power_on` clears live controller input so a backend must refresh
  physical state afterward.

The fixture is part of the default host `make test` suite.

## Validation

With Ubuntu GCC 13.3.0 on the host, all of the following passed:

- `make -j2 test` (the complete host suite plus repository checker);
- a focused `test-machine-input` build and execution with ASan+UBSan using
  `-O1 -fno-omit-frame-pointer -fsanitize=address,undefined` and leak detection
  disabled because this managed runtime cannot inspect `/proc` reliably;
- GCC `-fanalyzer` compilation of `core/machine/machine.c` and
  `tests/test_machine_input.c` with C99 warnings promoted to errors;
- `git diff --check`; and
- the modified-area TODO/FIXME/placeholder scan, with no new unresolved marker.

Local Clang, PS2SDK and PSPSDK toolchains are unavailable. The pinned GitHub host,
PS2 and PSP workflows must therefore supply the cross-toolchain evidence for the
published revision; no CI result is claimed before those runs finish.

## Claims and limits

The machine/core wiring for standard pads is complete and deterministic. This does
not implement physical controller polling, platform button maps, hotplug behavior,
multitap or nonstandard peripherals. In particular, the PS2 backend `Pad input`
roadmap item remains open until target code and runtime evidence exist.

## Next primary milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
the emulator version/settings and on-device ELF checksum recorded.
