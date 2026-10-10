# Host development target

The host is for compilation, unit/regression tests, future test-ROM execution,
fuzzing, debugging and tooling only. No GUI, SDL, desktop emulator UX or desktop-led
architecture is introduced. The binary says foundation only and rejects arguments;
it does not load ROMs.

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

Current coverage: 24-bit address construction/masking across all banks and offset
values, logging callback/context/disabled behavior, host banner/argument rejection,
repository structure/boundaries/checklists. These do not validate 65C816 behavior.
Future CPU/bus fixtures must remain independent of platform devices.
