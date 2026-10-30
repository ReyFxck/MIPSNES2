# PS2 pad input audit

Date: 2026-10-30

## Scope

This block adds the first PS2 physical-controller path to the bounded `--rom`
diagnostic. A PS2-only adapter loads `rom0:SIO2MAN` and `rom0:PADMAN`, initializes
`libpad`, opens connector 1/slot 0 over a fixed 256-byte aligned buffer and performs
one nonblocking sample before each fixed emulated frame horizon. Successful samples
feed `mipsnes2_machine_set_joypad`; unavailable states inject a disconnected zero
word so stale buttons are not retained.

The shared core contains no PS2SDK header, type, address or lifecycle policy.

## Mapping contract

The PS2 digital buttons map to the standard SNES word as follows:

| PS2 | SNES |
|---|---|
| Cross | B |
| Square | Y |
| Circle | A |
| Triangle | X |
| D-pad | Matching direction |
| Start / Select | Start / Select |
| L1 / R1 | L / R |

L2/R2, L3/R3 and analog axes are ignored. The pure translator accepts `libpad`'s
active-low 16-bit field. The target adapter has a compile-time contract that its
locally exposed mask values equal the pinned PS2SDK definitions.

## Evidence

`tests/test_ps2_pad_mapping.c` verifies every mapped button separately, all mapped
buttons together, the ignored digital inputs and one mixed word. The test is part of
the default host suite. `tests/test_ps2_pad_input.c` supplies minimal documented SDK
stubs and verifies module/init/open failures, 64-byte DMA alignment, disconnected and
transient states, read failure, a successful sample and close/end cleanup. The PS2
build compiles and links the same module/open/state/read adapter against `-lpad`;
GitHub CI is the available real-PS2SDK gate.

The SDK contract was checked against `libpad.h`, its sample and sample Makefile at
PS2SDK source revision `4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5`, linked in
`docs/references.md`. No emulator source or proprietary SDK material was used.

## Validation

With Ubuntu GCC 13.3.0 on the host, all of the following passed:

- `make -j2 test` (the complete host suite plus repository checker);
- focused `test-ps2-pad-input` and `test-ps2-pad-mapping` builds and executions
  with ASan+UBSan using `-O1 -fno-omit-frame-pointer
  -fsanitize=address,undefined`, with leak detection disabled because this managed
  runtime cannot inspect `/proc` reliably;
- GCC `-fanalyzer` compilation of the two adapter sources and both focused tests
  with C99 warnings promoted to errors;
- `git diff --check`; and
- the modified-area TODO/FIXME/placeholder scan, with no new unresolved marker.

Local Clang, PS2SDK and PSPSDK toolchains are unavailable. The pinned GitHub host,
PS2 and PSP workflows must therefore supply the cross-toolchain evidence for the
published revision; no CI result is claimed before those runs finish.

## Limits

No target execution is claimed. The current ROM diagnostic remains bounded to one
frame without RFAuds2 or eight frames with it, so this is not continuous gameplay
input. Hotplug recovery, module-conflict recovery, port 1, multitap, pressure/analog
policies, rumble, UI remapping, PSP input and physical-console validation remain
open. The PS2 backend pad item stays in progress.

## Next primary milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
the emulator version/settings and on-device ELF checksum recorded.
