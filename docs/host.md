# Host development target

The host is for compilation, unit/regression tests, future test-ROM execution,
fuzzing, debugging and tooling only. No GUI, SDL, desktop emulator UX or desktop-led
architecture is introduced. The binary identifies the CPU/bus foundation and rejects
arguments; it does not load ROMs.

Future host audio work is for deterministic offline sample tests and diagnostics
behind the common PCM contract. It remains independent of RFAuds2 and must be able to
validate S-DSP output without any physical PS2/SPU2 device.

Requirements: C99 GCC/Clang, GNU Make, Python 3.10+. No third-party C test framework.

```sh
make
make test
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test
make BUILD_DIR=build/clang CC=clang test
```

Use separate BUILD_DIR for changed compilers/flags; Make does not track flag changes.
The compiler's sanitizer runtime must be installed for sanitizer builds. Failures
must be investigated, not hidden. `make clean` removes the selected build directory.
`make check` runs only repository consistency. Tests are headless and fail with
nonzero exit status; they deliberately avoid NDEBUG-sensitive assertions.

Current coverage includes 24-bit address construction/masking across all banks and
offsets; logging callback/context/disabled behavior; host banner/argument rejection;
repository structure/boundaries/checklists; the reference bus; and the 27-opcode CPU
subset described in [65c816.md](65c816.md). CPU regressions include implemented
8/16-bit widths, flags, cycles, absolute indexing and linear 24-bit data crossing.
They verify only the implemented CPU/bus scope and remain independent of platform
devices.
