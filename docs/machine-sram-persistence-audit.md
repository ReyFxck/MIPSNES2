# Machine SRAM persistence audit

Date: 2026-10-29

## Scope

This block connects the existing cartridge dirty/revision contract to the public
machine boundary. `mipsnes2_machine_sram_view_get` returns the live caller-owned
bytes, mapper-visible size, dirty state and revision only for a ready machine.
`mipsnes2_machine_sram_acknowledge` clears dirty state only when a platform reports
durable completion for the exact current revision.

No filesystem API, platform path or target storage policy enters `core/`.

## Executable evidence

`tests/test_machine_sram.c` constructs a valid synthetic LoROM whose 65C816 program
writes two different values to `$70:0003` through long `STA` instructions. The
fixture verifies:

- null and unready machine calls fail;
- the initial 8 KiB view is clean at revision zero;
- the first CPU/bus/mapped SRAM store publishes value `51`, dirty state and
  revision one;
- the second store publishes value `52` and revision two;
- acknowledging revision one after the newer write fails and leaves dirty state;
- acknowledging revision two succeeds; and
- machine power-on retains the SRAM byte, revision and acknowledged clean state.

The test is part of the default host `make test` suite.

## Validation

With Ubuntu GCC 13.3.0 on the host, all of the following passed:

- `make -j2 test` (the complete host suite plus repository checker);
- a focused `test-machine-sram` build and execution with ASan+UBSan using
  `-O1 -fno-omit-frame-pointer -fsanitize=address,undefined` and leak detection
  disabled because this managed runtime cannot inspect `/proc` reliably;
- GCC `-fanalyzer` compilation of `core/machine/machine.c` and
  `tests/test_machine_sram.c` with C99 warnings promoted to errors;
- `git diff --check`; and
- the modified-area TODO/FIXME/placeholder scan, with no new unresolved marker.

Local Clang, PS2SDK and PSPSDK toolchains are unavailable. The pinned GitHub host,
PS2 and PSP workflows must therefore supply the cross-toolchain evidence for the
published revision; no CI result is claimed before those runs finish.

## Claims and limits

The machine-to-cartridge persistence handoff is integrated. The returned view is
live, so an asynchronous backend must copy it or pause/cooperate with emulation.
Save-file loading, file naming, exact-size validation, crash-safe replacement,
debounce, user-facing errors and PS2/PSP storage runtime evidence remain absent.
The Phase 9 persistence item therefore remains in progress.

## Next primary milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
the emulator version/settings and on-device ELF checksum recorded.
