# 65C816 foundation audit

Audit date: 2026-10-10. Branch: `feature/65c816-foundation`, based on merged main
revision `dc88d34c2c01b6129162f8ea8324dccc718f1845`.

## Audited scope

This milestone adds caller-owned 65C816 state, sourced reset-visible invariants, a
fourteen-opcode interpreter subset, completed-instruction base cycles, a replaceable
logical bus interface, direct/MMIO reference regions and host regressions. It does not
add a machine, cartridge loader, complete SNES map, PPU, SPC700, S-DSP, scheduler,
DMA/HDMA, platform runtime backend, assembly optimization or ROM execution.

The CPU behavior source is the Western Design Center W65C816S Datasheet dated
March 13, 2024, with exact sections recorded in [65c816.md](65c816.md). Test vectors
were independently authored. No emulator source or external test ROM was used.

## Local verification

Environment:

- Ubuntu host, GCC 13.3.0, GNU Make 4.3, Python 3.12.14;
- pinned PS2 toolchain `v2.0.0`, cross GCC 15.2.0;
- pinned PSP toolchain `v20261001`, cross GCC 15.2.0.

Verified commands and results:

| Command | Result |
|---|---|
| `make clean && make test` | Pass: contracts, CPU/bus tests, host smoke and repository checks |
| `make BUILD_DIR=build/release CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test` | Pass |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-gcc CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Pass with AddressSanitizer and UndefinedBehaviorSanitizer; local LeakSanitizer disabled because this runtime cannot inspect `/proc/*/task` |
| `make -C platform/ps2 clean all` with the pinned PS2 environment | Pass; CPU and bus compile/link into `mipsnes2-ps2.elf` |
| `make -C platform/psp clean all` with the pinned PSP environment | Pass; CPU and bus compile/link into ELF/PRX/EBOOT.PBP |

The first local sanitizer run with LeakSanitizer enabled stopped in the sanitizer
runtime with `Can't open /proc/.../task`; no project assertion or sanitizer finding
was reported. Full LeakSanitizer remains enabled in GitHub CI. Clang was unavailable
in this local environment; the existing Clang CI job remains the verification gate.

No PCSX2, PPSSPP, real PS2 or real PSP runtime test was performed. Cross-compilation
does not establish runtime correctness. No performance measurement was made.

## RFAuds2 decision evidence

The separate ReyFxck/RFAuds2 repository was initially inspected at revision
`f82e0967104fc96f37259e1abc124341eef6a76c`. Its guidance described a fixed 48 kHz
stereo S16 SPU2 boundary, kept emulated timing producer-owned, and required FAT/Slim
plus long-duration validation. That early audit added no RFAuds2 source, dependency
or runtime integration; later PS2-only integration work is recorded separately.

## Audit conclusions

- all implemented opcodes are listed explicitly in [65c816.md](65c816.md);
- an unimplemented opcode enters a sticky, test-covered diagnostic stop;
- CPU-visible addresses are logical 24-bit values;
- no platform include or conditional appears in the shared core;
- the reference region search is not claimed as the final hot path;
- deterministic unmapped behavior is not claimed as full SNES open bus;
- no game-specific branch, fake subsystem, assembly or benchmark claim exists;
- [STATUS.md](../STATUS.md) keeps complete interpreter, SNES mapping, timing and
  runtime tasks incomplete.
