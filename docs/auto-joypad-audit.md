# Ricoh 5A22 automatic joypad foundation audit

Date: 2026-10-20

## Verified scope

This milestone adds a platform-independent automatic controller-read foundation. The
caller owns four logical 16-bit input words. Enabling NMITIMEN bit 0 causes the H/V
component to snapshot those words at H=298 on the first VBlank line. HVBJOY bit 0 is
then busy for 4224 SNES master clocks, after which all result words become visible
through `$4218-$421F` in little-endian register order.

The input setter carries already translated SNES serial-order bits; it is not a host,
PS2 or PSP controller API. Each platform remains responsible for input acquisition
and mapping before machine wiring supplies a value to CPU-I/O. Input changes after a
read begins cannot change that read's pending result.

Reads during the busy period retain the previous completed result. Disabling
NMITIMEN bit 0 after a read begins does not cancel it. Enabling only at or after the
frame's consumed start boundary waits until the next frame. Result-register writes
are accepted as writes to read-only hardware and do not alter state.

## Evidence and intentional approximations

Nintendo's *SNES Development Manual*, Book I chapter 13 and internal pages 2-28-7
through 2-28-9, documents automatic serial-to-parallel reads, four 16-bit results,
HVBJOY busy state, an approximately 18 microsecond delay after VBlank and an
approximately 215 microsecond read interval. The register table defines the standard
controller bit order.

Independent fullsnes hardware research reports a 4224-master-clock busy duration and
a start phase within H=32.5 through H=95.5, with H=74.5 on the first observed frame
and later reads phase-aligned in 256-clock steps. MIPSNES2 therefore uses H=74.5
(298 master clocks) as one deterministic foundation phase. It does **not** claim
that real hardware always starts there. Likewise, research says result reads while
busy are unreliable; preserving the previous complete word is deterministic but not
a hardware-accuracy claim.

No emulator source or implementation was consulted or copied. References are linked
from [references.md](references.md); no external manual or ROM asset is committed.

## Host verification

`tests/test_cpu_io.c` covers initialization, invalid input indices, enable state,
four snapshots, all eight result bytes, old-result visibility, input changes while
busy, disable while busy and read-only writes.

`tests/test_hv_timing.c` covers the exact fixed start, 4224-clock completion,
HVBJOY, previous-result visibility, snapshot isolation, disabled/late enable,
completion after disable, normal/overscan VBlank starts and PAL one-shot/chunked
equivalence. `tests/test_master_timing.c` checks both boundaries in the shared master
domain. `tests/test_snes_bus.c` checks result reads through low/high-bank mirrors.

Local verification so far:

- `make clean && make -j2 test` — passed with GCC.
- `make BUILD_DIR=build/release-joypad CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test` — passed.
- GCC ASan+UBSan full suite — passed with `ASAN_OPTIONS=detect_leaks=0`.

The first sanitizer invocation compiled successfully but LeakSanitizer stopped before
the tests because this managed environment prevents `/proc/<pid>/task` inspection.
The rerun disabled leak detection only; AddressSanitizer and UndefinedBehaviorSanitizer
remained enabled. Local Clang, PS2SDK and PSPSDK are unavailable.

GitHub Actions passed for implementation commit
`3c56b6b1e3687b5eeacffebbab1ef3b1ed8a0f03`:

- Host CI run `39461739034` — GCC and Clang release builds, full sanitizer suites
  and host artifact upload passed;
- PS2 CI run `39461739142` — pinned toolchain installation, scaffold cross-build,
  development artifact and SHA-256 staging/upload passed;
- PSP CI run `39461739185` — pinned toolchain installation, scaffold cross-build,
  development artifact and SHA-256 staging/upload passed.

All returned jobs and individual workflow steps were inspected. These are build
results, not emulator or real-console execution.

## Explicitly incomplete

- measured phase evolution and motherboard/revision differences;
- hardware-faithful partial result contents while busy;
- electrical/cycle-level contention with the later manual `$4016/$4017` foundation;
- multitap, mouse, Super Scope and other peripheral protocols;
- hardware interaction with manual reads, writes and DMA during an automatic read;
- real PS2 and PSP input-backend wiring;
- real SNES hardware validation of edge cases;
- save-state serialization of the new in-flight state.

No PPU rendering, DMA/HDMA, SPC700, S-DSP, RFAuds2 runtime, enhancement chip,
dynarec or platform-specific optimization was added. No compatibility or performance
claim follows from this milestone.
