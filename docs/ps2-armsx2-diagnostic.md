# PS2 diagnostic on ARMSX2 (Android) — 2026-10-23

## What the ELF checks

`platform/ps2/mipsnes2-ps2.elf` uses the pinned PS2SDK's `init_scr` and
`scr_printf` to show `MIPSNES2 PS2 DIAGNOSTIC v1`, five `PASS`/`FAIL` lines and
`RESULT: 5 PASS / 0 FAIL`. It prints the same result to stdout where the emulator
exposes a log and stays on screen until the user exits. The fixtures are original,
embedded synthetic instructions, not an imported SNES ROM:

1. CPU LDA/ADC/STA stores `$40` in WRAM.
2. A timed NOP resumes from master-clock horizon 8 to 14 exactly once.
3. A pulse in a pending `$2180` write requires EXTERNAL qualification; its effect
   reaches WRAM.
4. The same pulse with INHIBIT accounts the write but leaves WRAM and the WRAM
   port address untouched.
5. A late INC RMW ABORT preserves already-modified status and inhibits the write.

This checks execution of a bounded shared-core subset on the PS2 EE as emulated
by ARMSX2. It does not run a SNES game, PPU, SPC700, DSP, PS2 audio/pad backend
or Ricoh 5A22 electrical ABORT. A green host suite or PS2 cross-build cannot
stand in for a captured ARMSX2 screen, and ARMSX2 cannot measure physical PS2
performance or establish actual SNES electrical behavior.

## Android setup and evidence

1. Use an ARMSX2 release with `.elf` entries in its library (official release
   [2.5.1](https://armsx2.net/updates/2.5.1) added them). Record the app version.
2. Import/select a valid PS2 BIOS through the app's [BIOS manager](https://armsx2.net/docs/importing-bios)
   if one is not already selected. Do not redistribute the BIOS.
3. Download the diagnostic ELF to local phone storage, add its containing folder
   through the library's **Setup / Change Folders**, then **Scan** or **Rescan**.
   The general folder workflow is in the official
   [import guide](https://armsx2.net/docs/importing-roms); ELF visibility is
   separately confirmed by the 2.5.1 release notes.
4. Start the ELF from the library with default global settings. Keep EE cycle
   rate at 100%, cycle skipping off, and game fixes/cheats/fast-forward off for
   this baseline. Do not load a save state. These are test controls, not a claim
   of perfect PS2 hardware fidelity.
5. Send a screenshot showing the header, five lines and final result. If it
   hangs, crashes or stays black, send the ARMSX2 version, a screenshot, the
   emulator log if exportable and whether the ELF actually appears in the
   library. A black screen is **not** recorded as a core failure by itself.

## Validation and exit criterion

- [x] Host `make BUILD_DIR=build/ps2-diag -j2 test` — all suites including
  `test-ps2-diagnostic` (5/5) passed.
- [x] Host O3 and ASan+UBSan `make test` with separate build directories
  — all suites passed; `ASAN_OPTIONS=detect_leaks=0` for the managed host's
  LeakSanitizer limitation.
- [x] Pinned ps2dev v2.0.0 / PS2SDK clean cross-build — produced a
  nonempty 32-bit MIPS N32 ELF; SHA-256
  `d1d58f1de90b98025ea3533b1d4951829680114f05753fb3d8ef3ad72223b37a`.
- [x] Pinned PSP v20261001 build — unchanged secondary backend compiled.
- [x] Implementation commit
  [`405d0c7587bf8059051e7c1a076a47d77f35b428`](https://github.com/ReyFxck/MIPSNES2/commit/405d0c7587bf8059051e7c1a076a47d77f35b428):
  [Host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40014123841),
  [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40014123914)
  and [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40014123797)
  completed successfully. Their respective nonempty Host GCC/Clang, PS2 and
  PSP artifacts were inspected in the run metadata (PS2 archive: 660,272 bytes).
- [ ] Record ARMSX2 version, baseline settings and visible `5 PASS / 0 FAIL`
  (or precise failure details) from the user's Android run.
- [ ] Run on real PS2 for a hardware boot claim.

The Android diagnostic milestone is complete only when the expected screen is
captured in ARMSX2 and the exact ELF hash/version are recorded. Physical PS2
validation remains a separate gate.

## Independent SNES test ROMs

[gilyon/snes-tests](https://github.com/gilyon/snes-tests/tree/5ecdf555da920f0bd7b157542141965a8120186d)
is an independent MIT-licensed test-ROM project reviewed at pinned revision
`5ecdf555da920f0bd7b157542141965a8120186d` (license and `cputest`/`spctest`
READMEs). `cputest-full.sfc` and `cputest-basic.sfc` cover opcode behavior but
not timing, interrupts or MMIO; its SPC700 suite does not test DSP, S-SMP MMIO
or timing. We imported **none** of its sources or ROMs into this ELF. Running
these `.sfc` ROMs requires MIPSNES2's SNES machine, display and ROM-loading
path, which are still incomplete. Pin/test the fixture and record license and
expected hardware result at that later integration point.
