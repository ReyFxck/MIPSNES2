# Non-interlaced H/V counter-register audit

Date: 2026-10-20

Historical implementation evidence below describes the initial counter milestone.
The subsequent [access-clock milestone](cpu-access-timing-audit.md) supersedes its
start-of-instruction snapshot limit: captures now occur after each emitted access's
clock advance. Missing internal/dummy placement and physical latch latency remain.

## Implemented scope

Caller-owned `core/ppu/counters.c` implements only `$2137`, `$213C`, `$213D` and
`$213F`. It stores captured nine-bit H/V values, independent low/high read selectors,
a latch flag, PPU2 open-bus byte and caller-selected version nibble. Initialization
uses zero captured values and explicit caller-owned open bus; these are deterministic
storage defaults, not hardware power-on/RESET values.

Software reads capture when WRIO bit 7 is high and leave the selectors unchanged.
High counter reads preserve seven PPU2 open bits. Status reads reset both selectors,
compose current field/region/version and clear the latch flag when WRIO bit 7 is
high. The bus adapter consumes CPU-I/O bit-7 falling edges; it handles only these
four addresses and preserves the existing PPU callback for other accesses.

`mipsnes2_hv_ppu_horizontal` projects the current non-interlaced scanline phase into
340 PPU dots. Normal lines assign six clocks to dots 323/327 and four to the rest;
the 1360-clock NTSC short line has no long dots. CPU timer-match calculations are
unchanged: the 5A22 and PPU horizontal representations must not be conflated.

## Sources and unresolved evidence

- Nintendo's *SNES Development Manual*, Book I chapter 11, caution 24.1 and internal
  pages 2-27-20, 2-27-22 and 2-27-23 establish the capture/read/reset/status interface.
- fullsnes, `SNES PPU Timers and Status` and `PPU H-Counter-Latch Quantities`, reports
  the long-dot durations and externally/software-triggered latching.
- Anomie's register research, reproduced by the Super Famicom Development Wiki,
  specifies WRIO-gated latch-flag acknowledgement; fullsnes summarizes it as
  unconditional. The current code chooses the gated policy, not a proven resolution.
- Anomie's timing research reports WRIO capture one dot later than the corresponding
  SLHV access. This relative latency is not implemented because the current CPU bridge
  still lacks access-cycle timestamps.
- SNESdev's PPU register page explicitly marks repeated capture and active latch
  conditions during status acknowledgement as not fully confirmed. MIPSNES2 currently
  permits each explicit capture even with a set flag; this is a documented deterministic
  policy pending hardware tests, not a claim of complete latch-line accuracy.

Sources are linked in [references.md](references.md), inspected 2026-10-20.
The manufacturer PDF initially timed out, then was retrieved successfully. No
emulator implementation, external ROM, manual asset or copied code is included.

## Timing and ownership limits

Optional bus configuration binds initialized counter, H/V and CPU-I/O objects
together. Failed configuration leaves the bus unchanged. The timed CPU bridge rejects
a different or absent H/V object once counters are attached. WRIO bus writes consume
their edge immediately. After external input changes, callers must synchronize via
`mipsnes2_snes_bus_sync_hv_latch` before advancing emulated time. The existing pending
bit has no timestamp and coalesces unsynchronized edges.

Within an atomic CPU step, the component sees the starting H/V snapshot, not each
memory-access timestamp. This can shift raster/gun observations and must be refined
before claiming timing-sensitive compatibility. No PPU rendering, VRAM/CGRAM/OAM,
STAT77, interlace, light-gun device, DMA/HDMA, APU or RFAuds2 runtime was added.
Other PPU2 reads do not yet feed this component's latch; complete open-bus domains
remain incomplete.

## Verification

`tests/test_ppu_counters.c` enumerates every master-clock phase for NTSC/PAL normal
lines and the NTSC short line, all H=0-339 and non-interlaced PAL V=0-311 values,
selectors, masks, repeat/disabled capture policy, status reset and invalid state.
`tests/test_snes_bus.c` enumerates all 128 system-bank mirrors, register access
counts/clocks, ignored writes/fallback, WRIO/external capture, invalid synchronization
without consuming the edge, configuration transactionality and an executed LDA.

Local GCC 13.3.0 commands, all from the repository root:

- `make clean && make -j2 test` — passed, exit 0;
- `make BUILD_DIR=build/release-hv-latch CC=gcc CFLAGS='-O2 -g -DNDEBUG'
  clean test` — passed, exit 0;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-hv-latch CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed, exit 0. ASan and
  UBSan remain enabled; only leak detection is disabled for the previously documented
  managed-runtime `/proc` restriction;
- `make -C platform/ps2` — unavailable, exit 2: PS2SDK is unset;
- `make -C platform/psp` — unavailable, exit 2: psp-config is absent;
- `python3 tools/check_repository.py` and `git diff --check` — passed;
- GPL-3.0 LICENSE SHA-256 remains
  `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.

An intermediate test build used a wrong fixture-helper name and failed compilation;
the helper call was corrected before the successful clean suites. Local Clang and
console cross-compilers are absent. Real SNES/PS2/PSP and PCSX2/PPSSPP are not available.

GitHub Actions passed for implementation commit
`0a4a35c23dd5cb299b0e06471f4eccd20a3a5092`:

- [Host CI 39512827886](https://github.com/ReyFxck/MIPSNES2/actions/runs/39512827886)
  — GCC/Clang release, full sanitizer tests and host artifacts passed;
- [PS2 CI 39512827848](https://github.com/ReyFxck/MIPSNES2/actions/runs/39512827848)
  — pinned toolchain, scaffold cross-build, artifact/checksum staging and upload passed;
- [PSP CI 39512828169](https://github.com/ReyFxck/MIPSNES2/actions/runs/39512828169)
  — pinned toolchain, scaffold cross-build, artifact/checksum staging and upload passed.

All returned jobs and steps were inspected. Artifact names, nonzero sizes, digests
and unexpired state were checked through the listings; archive contents were not
downloaded. These remain development build outputs, not console-runtime or
performance evidence.

## Canonical status changes

Four narrow verified tasks are added: long-dot projection; counter read selectors
and PPU2 open-bus subset; field/region/version status and reset; optional WRIO/
external-edge bus adapter. PPU register model, latching, open bus and tests move to
`[IN PROGRESS]`, not complete. The broader latch-consumption task stays incomplete
because capture timing and held-low hardware cases remain unresolved. No opcode,
compatibility or performance completion is claimed.
