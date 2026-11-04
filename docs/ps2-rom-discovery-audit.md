# PS2 mounted-filesystem ROM discovery and continuous loop — 2026-11-04

## Scope

`platform/ps2/rom_discovery.[ch]` adds a bounded platform-only route from an ELF
started without arguments to a caller-owned cartridge image. It does not change
`core/`, mount a filesystem, load a storage driver or embed a ROM.

Discovery first replaces the final extension of `argv[0]` with `.sfc` and `.smc`.
This sidecar convention also covers development-emulator paths such as
`host:content://.../mipsnes2.elf`: placing a same-basename ROM beside the ELF does
not require a launcher-supplied `--rom` argument. If neither sidecar is readable
and nonempty, the launcher scans these already-available roots in order:

1. `.`
2. `mass:/MIPSNES2`
3. `mass:/`
4. `mass0:/MIPSNES2`
5. `mass0:/`
6. `mc0:/MIPSNES2`
7. `mc1:/MIPSNES2`
8. `host:MIPSNES2`
9. `host:`

Each directory scan is shallow and examines at most 256 entries in the launcher;
the reusable adapter rejects more than 16 roots or 512 entries per root. It accepts
case-insensitive `.sfc`/`.smc` names, probes for a readable nonempty file and picks
the bytewise-lexicographically smallest match in the first root containing one.
Paths are limited to 511 bytes plus NUL. The report exposes attempted/open roots,
examined entries, extension/readability matches, overlong matches and limit
truncation. The adapter owns no heap or unbounded list; libc may use internal
storage for its `DIR` handle.

An explicit `--rom PATH` remains the highest-priority override. `--audio-diag` and
`--core-diag` retain their explicit meanings. When automatic discovery finds
nothing, the previous no-argument diagnostic remains the fallback.

The cartridge path no longer stops after one standalone frame or eight RFAuds2
frames. It repeatedly asks the shared H/V state for the exact next frame boundary,
polls port 0, advances the machine to that absolute horizon, pumps already-produced
PCM and submits the captured frame. A machine/timing error ends the loop and runs
the existing bounded cleanup. Pad polling, audio RPC work and GS vertical-sync wait
remain outside machine advancement and cannot set emulated time.

## Evidence and limits

The host regression creates only temporary original bytes and verifies sidecar
priority, root priority, case-insensitive extensions, order-independent lexical
selection, empty/unreadable rejection, path-capacity failure and invalid bounds.
No commercial ROM or external fixture is stored.

The implementation follows PS2SDK's direction to use POSIX file calls from the EE
newlib port and its documented `mass:` filesystem naming. It deliberately does not
call the lower-level `fio`/`fileXio` directory APIs or load USB modules. A launcher
or emulator must already expose the selected root. Host tests cannot establish
Android content-provider enumeration, PS2 USB/memory-card availability, GS output,
pad behavior, audio continuity, frame rate or game compatibility. Those remain
target-runtime validation work.
