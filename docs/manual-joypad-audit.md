# Ricoh 5A22 manual joypad serial-port audit

Date: 2026-10-20

## Verified scope

This milestone adds a platform-independent standard-controller protocol at `$4016`
and `$4017`. The existing four caller-owned logical input words use the SNES serial
order in bits 15 through 0: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R,
then four zero identification bits.

Writing `$4016` bit 0 high resets both manual port positions and exposes the live B
bit without advancing. Its high-to-low transition snapshots all four words and their
connectivity. While strobe is low, `$4016` clocks JOY1 and JOY3 together and `$4017`
independently clocks JOY2 and JOY4. Each read returns the current bit before advancing.
After the 16-bit word, a line returns one for a connected standard pad and zero when
the caller marks it disconnected. `$4016` retains open-bus bits 2-7; `$4017` retains
open-bus bits 5-7 and returns fixed ones in bits 2-4.

The input and connectivity setters are machine-wiring contracts. They do not call a
PS2, PSP or host controller API. Output bits 1-2 are retained so later peripheral
work does not need to change the register contract, but they have no device wiring.

## Evidence and intentional approximations

Nintendo's *SNES Development Manual*, Book I chapter 13 describes manual controller
access as NES-compatible and defines the standard controller data order. Independent
fullsnes hardware research documents the common strobe, paired data lines, read-clock
behavior, fixed/open return bits and the connected standard pad's post-word value.

The manual shift registers are deliberately independent of the automatic polling
snapshot and countdown. Actual simultaneous manual and automatic access can contend
for physical controller lines, but that interaction is not yet sufficiently verified.
Before the first falling-edge strobe, zero-initialized snapshot data and disconnected
padding provide deterministic behavior; this is not a claim about console power-up
electrical state. No emulator source or implementation was consulted or copied.
References are linked from [references.md](references.md); no external manual or ROM
asset is committed.

## Host verification

`tests/test_cpu_io.c` covers invalid connectivity indices, output retention, live
high-strobe behavior, falling-edge snapshot isolation, independent physical-port
positions, exact reconstruction of all four 16-bit words, connected/disconnected
post-word values, fixed/open return bits and coexistence with automatic polling.
`tests/test_snes_bus.c` covers low/high system-bank mirrors and confirms that handled
port accesses do not leak through the fallback callback.

Local verification before publication:

- `make clean && make -j2 test` — passed with GCC.
- `make BUILD_DIR=build/release-manual-joypad CC=gcc CFLAGS='-O2 -g
  -DNDEBUG' clean test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-manual-joypad
  CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined
  -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test`
  — passed with AddressSanitizer and UndefinedBehaviorSanitizer; leak detection was
  disabled only for the managed runtime's documented `/proc` restriction.
- `python3 tools/check_repository.py` — passed.

Local Clang, PS2SDK and PSPSDK are unavailable. GitHub CI is therefore the Clang and
console cross-build gate.

GitHub Actions passed for implementation commit
`fb25e877b3e5a6b1df527c486bac9aa1bed2c98d`:

- Host CI run `39510314845` — GCC and Clang release builds, full sanitizer suites
  and host artifact uploads passed;
- PS2 CI run `39510314808` — pinned toolchain installation, scaffold cross-build,
  development artifact and SHA-256 staging/upload passed;
- PSP CI run `39510315037` — pinned toolchain installation, scaffold cross-build,
  development artifact and SHA-256 staging/upload passed.

All returned jobs and individual workflow steps were inspected. These are build
results, not emulator execution or real-console validation.

## Explicitly incomplete

- electrical and cycle-level contention with automatic polling;
- wiring `$4016` output bits 1-2 to peripherals;
- multitap, mouse, Super Scope and other nonstandard protocols;
- physical PS2 and PSP controller acquisition/mapping;
- real SNES, PS2 and PSP hardware validation;
- save-state serialization of the new latch/position state;
- complete register-specific open-bus modeling beyond the documented masks.

No PPU rendering, DMA/HDMA, SPC700, S-DSP, RFAuds2 runtime, enhancement chip,
dynarec or platform-specific optimization was added. No compatibility or performance
claim follows from this milestone.
