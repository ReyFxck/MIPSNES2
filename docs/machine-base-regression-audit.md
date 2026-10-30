# Phase 9 base machine regression audit

Date: 2026-10-30

## Scope

This milestone adds a focused `make machine-test` gate over six
platform-neutral machine fixtures:

- `test-machine`: lifecycle, constructed cartridge ownership and bounded CPU run;
- `test-machine-apu-pcm`: integrated SPC700/DSP cadence and deterministic PCM hash;
- `test-machine-ppu-frame`: two recurring PPU frames and chunk-independent pixels;
- `test-machine-input`: manual and automatic standard-pad paths through the machine;
- `test-machine-sram`: live dirty/revision handoff and exact acknowledgement;
- `test-machine-wait`: deterministic WAI NMI/IRQ/horizon advancement and STP stop.

The target is intentionally smaller than `make test`. It provides one repeatable
Phase 9 base-machine gate while preserving the full suite as the repository-wide
verification command.

## Claims and limits

The six constructed fixtures pass together and remain deterministic on the host.
The gate does not execute a licensed
test ROM, a commercial game, a PS2 or PSP binary, a durable save backend, or a
physical controller/audio/video device. It therefore does not close the Phase 9 exit
gate or either real-target smoke item.

There is no core/platform dependency change, compatibility claim or performance
claim. Existing platform boundaries and their target-runtime limitations remain
unchanged.

## Verification

- `make -j2 machine-test`
- `make -j2 test`
- `python3 tools/check_repository.py`
- `git diff --check`
- repository TODO/FIXME/placeholder scan reviewed against status claims
- GitHub Host, PS2 and PSP workflows inspected after publication

## Primary next milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
emulator version/settings and on-device ELF checksum recorded.
