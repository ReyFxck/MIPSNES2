# Base LoROM/HiROM Cartridge Foundation Audit

Date: 2026-10-17

## Scope

This milestone adds an original, caller-owned C99 cartridge foundation:

- slot reads/writes can report whether an address is decoded;
- read-only ROM and writable SRAM buffers;
- explicit base LoROM/HiROM selection;
- documented ROM/SRAM windows and power-of-two mirroring;
- deterministic configuration errors;
- SNES-bus, CPU reset/fetch and scheduler integration;
- host fixtures and PS2/PSP cross-build inclusion.

It does not implement filesystem loading, header analysis, mapper auto-detection,
copier headers, non-power-of-two/multi-chip boards, ExLoROM/ExHiROM, enhancement
chips or SRAM persistence.

## Evidence and implementation boundary

The console bus windows come from [Anomie's SNES memory mapping document](https://github.com/gilligan/snesdev/blob/eeb98f1e85b7db6574ca0804455819ec81117198/docs/memmap.txt),
revision 1126 at pinned repository revision
`eeb98f1e85b7db6574ca0804455819ec81117198`. The 32 KiB LoROM and 64 KiB HiROM
wiring is cross-checked against the [SNESdev Wiki memory map](https://snes.nesdev.org/wiki/Memory_map)
and [The SNES Cartridge, Briefly Explained](https://mousebitelabs.com/2019/05/18/custom-pcb-explanation/),
all inspected 2026-10-17. No emulator source or commercial ROM was used.

Only power-of-two ROM from 32 KiB for LoROM or 64 KiB for HiROM through 4 MiB is
accepted. SRAM may be absent or power-of-two within the base map's addressable
window. This is a stated compatibility limit, not a claim that commercial irregular
boards are invalid.

## Verification

`tests/test_cartridge.c` covers:

- null pointers, unsupported mapper values, ROM sizes and SRAM sizes;
- caller-owned buffer retention and a no-SRAM configuration;
- base LoROM/HiROM ROM windows, boundary bytes and deterministic mirrors;
- base SRAM windows, mirrors and writes;
- read-only ROM writes and declined unmapped addresses;
- 24-bit address normalization;
- preservation of the SNES bus open latch when the cartridge declines a read;
- reset-vector and timed NOP fetch through cartridge, SNES bus, CPU and scheduler.

The following commands passed locally:

```text
make clean && make test
make BUILD_DIR=build/release clean
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize clean
ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
git diff --check
python3 tools/check_repository.py
```

The new suite reported `PASS: base LoROM/HiROM cartridge mapping foundation` in
default, release and ASan+UBSan configurations. Leak detection alone was disabled
for the existing local environment restriction. Clang was unavailable. Local PS2
and PSP commands stopped before compilation because PS2SDK and `psp-config` were
unavailable. GitHub's pinned workflows remain the cross-compiler gate.

## GitHub Actions

All three pull-request workflows passed for implementation commit
`f5be6dec1b4fc9ebdf96564fb1736c6fcdcad032`:

- [Host CI run 39072800480](https://github.com/ReyFxck/MIPSNES2/actions/runs/39072800480)
  — GCC and Clang release/sanitizer jobs succeeded and staged artifacts;
- [PS2 CI run 39072800463](https://github.com/ReyFxck/MIPSNES2/actions/runs/39072800463)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded;
- [PSP CI run 39072800446](https://github.com/ReyFxck/MIPSNES2/actions/runs/39072800446)
  — pinned-toolchain scaffold cross-build, artifact and checksum steps succeeded.

The documentation-evidence commit must also pass all applicable workflows before
merge. Cross-compilation is not console execution.

## Runtime and performance

No PCSX2, real PS2, PPSSPP or real PSP test was performed. No commercial ROM was
loaded. No benchmark or compatibility/full-speed claim is made.
