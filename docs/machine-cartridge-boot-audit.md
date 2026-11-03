# Synthetic cartridge boot integration audit

Date: 2026-11-03

## Scope

`tests/test_machine_cartridge_boot.c` generates an original 64 KiB LoROM in
memory and executes it through the public machine boundary. The fixture contains
no commercial ROM, protected firmware or code imported from another emulator.
Its internal header, checksum, reset vector, 65C816 program and three-byte SPC700
program are all constructed locally by the test.

The ROM performs the complete integration sequence itself:

- boots from the LoROM RESET vector through the timed 65C816 path;
- observes the clean-room `$AA/$BB` S-SMP ready signature;
- sends `$CC`, uploads `MOV A,#$42; SLEEP` to ARAM `$0200`, waits for every
  acknowledgement and starts the SPC700 at `$0200`;
- writes Mode 1, BG1, VRAM and CGRAM registers through 65C816 `$21xx` MMIO;
- releases forced blank and writes a WRAM completion marker;
- remains in a cartridge loop while the shared scheduler reaches a complete
  reference frame.

The test does not preseed VRAM, CGRAM, APU ports or SPC700 registers after machine
initialization. Caller-owned memories start at zero, and the synthetic cartridge
program is solely responsible for the observed scene and upload.

## Deterministic evidence

The fixture verifies LoROM selection, NTSC destination propagation, the WRAM
completion marker, the exact three uploaded ARAM bytes, completed boot-protocol
state, SPC700 `A=$42` and `SLEEP`, Mode 1/BG1 register state, every written tile
row and the green CGRAM entry. It aligns to the next shared H/V frame boundary,
then captures exactly 224 ordered 256-pixel lines.

Every captured pixel is BGR555 `$03E0`; the stream hashes to
`0c68c1b754439e25`. A second independent machine uses irregular one-clock,
event-edge and large execution chunks. Frame hash, line and pixel counts, master
time, H/V frame count, CPU state and SPC700 state match the regular-chunk run.

This proves that the current shared components can complete one original
cartridge-driven CPU/APU/PPU startup path. It does not prove compatibility with a
licensed test ROM or commercial game, full PPU behavior, continuous platform
presentation, real PS2/PSP execution, hardware timing or performance.

## Local verification

- `make machine-test`
- `make BUILD_DIR=build/cartridge-e2e-full -j2 test`
- focused `-O2 -DNDEBUG` execution
- complete ASan/UBSan suite with `ASAN_OPTIONS=detect_leaks=0`
- GCC `-fanalyzer` build of the core and new fixture
- `python3 tools/check_repository.py`
- `git diff --check`

Two generated host ELFs unexpectedly lacked their execute bit in the managed
workspace. Their file type was confirmed, only the generated artifact permission
was restored, and each affected suite was resumed successfully. No tracked file
permission changed.

Local PS2SDK and PSP toolchains are unavailable. Cross-build and artifact evidence
comes from published implementation `a0c3d36`:

- [Host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41570282278) passed
  GCC and Clang release/sanitizer builds; each job log records the new fixture
  passing twice, and both host artifacts are nonempty;
- [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41570282288) installed
  the verified pinned toolchain, built pinned RFAuds2, cross-compiled the diagnostic,
  verified its internal `SHA256SUMS` and uploaded a 952,543-byte artifact with
  archive digest `sha256:6e327daf2c52ef6acf1a5e0673ec1c20632729a99a9955db71eedbeb15a055c2`;
- [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41570282248) installed
  the verified pinned toolchain, cross-compiled `EBOOT.PBP`, verified its internal
  `SHA256SUMS` and uploaded a 167,640-byte artifact with archive digest
  `sha256:d72c5094ee00c5d9b6ef7d56cdea7b3f3fe892ef7276f5719321398baea290a3`.

These cross-builds establish compiler/link portability only. No emulator or physical
target runtime result is claimed here.

## Primary next milestone

Add a host-side runner for one pinned, redistributable 65C816 test ROM already
listed in `docs/references.md`, execute it through the same shared machine boundary
and record the first deterministic hardware-behavior failure without ROM-specific
logic.
