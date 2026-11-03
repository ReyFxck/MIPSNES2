# Clean-room S-SMP boot-protocol audit

Date: 2026-11-02

## Scope

The shared core now has an optional behavioral implementation of the S-SMP
power-on upload protocol. It contains no Nintendo IPL ROM bytes and does not
provide a readable replacement firmware image. The PS2 explicit-ROM path enables
this behavior because it already supplies caller-owned 64 KiB ARAM.

At power-on the protocol clears ARAM `$0000-$00EF`, publishes `$AA/$BB` through
the two directional output ports, accepts the `$CC` command, receives one or more
indexed blocks through `$2140-$2143`, acknowledges each accepted index and starts
the existing complete SPC700 at the requested entry point. Transfers use the
S-SMP memory map, so address wrap and implemented MMIO routes remain shared with
normal execution. No title, game code, checksum or ROM identity changes behavior.

## Evidence and implementation policy

The manufacturer SFC Sound Manual documents the 64-byte IPL window at
`$FFC0-$FFFF`, the standard `$0200-$7FFF` transfer area, the four independent
S-CPU/S-SMP port directions and their zero reset state. Nintendo's SNES
Development Manual documents the corresponding `$2140-$2143` S-CPU registers.
The repository-pinned independent SPC700 hardware document and public boot
protocol description establish the observable `$AA/$BB`, `$CC`, byte-index,
multi-block and start-address handshake.

The implementation is an original C state machine over those observable
register transactions. It neither embeds, reconstructs nor executes the 64-byte
manufacturer program. An explicitly caller-supplied IPL and register seed remain
available for research, but are mutually exclusive with this protocol.

## Deterministic boundaries

- Input and acknowledgement latches are immediately visible, matching the
  existing documented communication-port approximation.
- Forward block termination uses the protocol's signed eight-bit ordering;
  stale/backward values wait without modifying ARAM.
- Destination and byte index wrap are explicit and bounded; there is no heap.
- The entry register state is the deterministic result of the documented boot
  sequence: `A=X=Y=0`, `SP=$EF`, `Z=1`, and the requested `PC`.
- The machine catches the APU clock up before attaching the uploaded program,
  so the SPC700 begins at the next explicit machine/APU boundary rather than
  retroactively consuming earlier cycles.
- The firmware overlay remains unavailable after handoff when no external IPL
  was supplied. Exact S-CPU/S-SMP races and IPL instruction-by-instruction timing
  are not claimed.

## Verification

Generated fixtures cover ready/reset state, zero-page clearing, command
acknowledgement, byte upload, stale signals, signed termination, multi-block
transfer, 16-bit address wrap, reset, unsupported-MMIO failure and exact entry
registers. Machine integration uploads and executes an original one-byte `SLEEP`
program, verifies delayed clock-domain attachment and repeats power-on. The PS2
configuration enables the same path without carrying firmware bytes.

Exact local commands and GitHub CI results are recorded in the published
revision's status entry.

The local clean host command
`make BUILD_DIR=build/cartridge-boot-full -j2 test` passed. The full ASan/UBSan
suite also passed with `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot
inspect `/proc` in this managed environment. GCC `-fanalyzer` compiled the changed
core and focused boot/cartridge/machine fixtures; `tools/check_repository.py` and
`git diff --check` passed. PS2SDK and the PSP toolchain are not installed locally,
so console builds require inspected GitHub CI and do not establish target runtime.
