# Deterministic machine WAI idle/wake audit

Date: 2026-10-30

## Problem

The 65C816 cursor already implemented WAI semantics, but the machine runner reached
the following waiting boundary without advancing shared emulated time. Treating that
status as a terminal CPU stop prevented a bounded frame runner from reaching the NMI
or timer IRQ that should release WAI. Jumping directly to the frame endpoint would
instead deliver an interrupt late.

## Implementation

`mipsnes2_hv_clocks_until_next_boundary` exposes the exact positive distance to the
next transition already owned by the H/V timing engine: timer/NMI, HBlank, line and
automatic-joypad state. While an inactive CPU cursor is waiting, the resumable timing
bridge advances to the earliest of that boundary, the next scheduler event and the
caller horizon. It samples and drives interrupt lines after every slice, preserves
the dispatch budget and returns to the existing timed HDMA path when requested.
Intermediate idle boundaries after HDMA do not consume the machine runner's CPU
sequence limit.

No CPU operation or processor cycle is synthesized during idle. NMI begins the
existing boundary interrupt sequence at the transition timestamp. A masked IRQ still
releases WAI and fetches the following opcode, as defined by the CPU core. A horizon
with no wake leaves the cursor inactive and the CPU waiting. STP remains a terminal
CPU stop. Platform video/audio/device pacing is not consulted and cannot advance the
machine.

## Verification

The constructed LoROM regression `tests/test_machine_wait.c` verifies:

- no-interrupt WAI progress to two exact caller horizons;
- no-channel HDMA handoff without consuming a CPU sequence slot;
- NMI delivery at normal NTSC VBlank `V=225,H=2`, with no early wake at `H=1`;
- masked H-timer IRQ wake at `H=414` without IRQ-vector entry;
- NMI handler/RTI continuation after WAI;
- unchanged STP stop behavior.

The H/V regression verifies invalid/output-preserving queries plus HBlank, timer,
NMI and auto-joypad next-boundary distances. `make machine-test`, `make test`, focused
GCC `-fanalyzer`, ASan+UBSan and repository consistency checks are the local gates.
GitHub Host, PS2 and PSP workflows must be inspected after publication.

## Limits

This closes deterministic logical idle advancement only. Exact electrical interrupt
sampling, timer-register write-at-match behavior, interlace, complete DMA/HDMA
arbitration, continuous PS2 presentation and target runtime remain open. The idle
loop's PS2/PSP cost is unmeasured. No game compatibility, audible output, visual
correctness or real-hardware performance result is claimed; `COMPATIBILITY.md`
therefore remains unchanged.

## Primary next milestone

Capture the existing RFAuds2 lifecycle diagnostic in the Android PS2 emulator with
emulator version/settings and on-device ELF checksum recorded.
