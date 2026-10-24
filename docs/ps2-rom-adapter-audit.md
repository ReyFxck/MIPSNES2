# Explicit-path PS2 ROM loading and bounded CPU execution — 2026-10-24

`platform/ps2/rom_file.[ch]` reads a file from a path supplied by the launcher
into the caller's fixed ROM buffer. Capacity is 8 MiB plus the 512-byte optional
copier prefix. A read error, empty file and one byte beyond capacity each have a
distinct result; failures expose no valid byte count. `--rom PATH` in the PS2
entry uses that buffer with the shared machine, 128 KiB WRAM and 512 KiB SRAM.
The default no-argument entry still displays the existing five-case diagnostic.
The optional path runs at most one nominal NTSC CPU/H-V frame or 100,000 completed
sequences and reports the stopping reason. It does not show game graphics.

Source for the SDK file API: the PS2SDK
[EE sample](https://github.com/ps2dev/ps2sdk/blob/2c670453980fcc3fe46ead6399b730b12c8556eb/ee/rpc/audsrv/samples/playwav/playwav.c)
uses `sceSifInitRpc`, `fopen` and `fread` for a `host:` path. No SNES emulator
source was inspected or incorporated. Filesystems not already installed by the
launcher are outside this slice. The program never loads USB drivers implicitly.

Host verification: `make test` includes `tests/test_ps2_rom_file.c` (exact-sized,
oversized, empty, invalid and absent files) and `tests/test_machine.c` (bounded
CPU execution, horizon pause and sequence limit on an original synthetic LoROM).
No third-party ROM bytes are committed. At implementation revision
`8261563bb03cb5bff35108c7fbcc9d6577496805`,
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40051034228),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40051034392) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40051034170)
completed successfully, including GCC/Clang sanitizer jobs. The PS2 build
published a nonexpired ELF artifact archive (683,603 bytes). The optional
PS2 command-line path, file
availability, SRAM initialization policy, full reset, controller input, PPU,
DMA, APU and sustained execution remain unverified or unimplemented as named.
