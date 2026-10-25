# HDMA VBlank deactivation audit

Date: 2026-10-25

## Hardware basis and implementation

[Anomie's SNES Timing Doc, HDMA section](https://github.com/gilligan/snesdev/blob/master/docs/timing.txt)
states that all HDMA channels deactivate at the start of VBlank. The shared
machine schedules a callback at H=0 of the first VBlank scanline after HDMA
is enabled. It clears the active and transfer masks without changing `$420C`,
table pointer registers or the frame's known/terminated masks. A following
V=0 frame initialization can select the channels again.

The callback clears a still-pending line request. If an HDMA slot crosses the
VBlank boundary, it requests shutdown; the resumable timing bridge commits
the already-admitted slot, then drops the rest of the cursor. This avoids
mutating the HDMA register/state snapshot during an in-progress slot. Exact
electrical behavior within that slot is unverified. The event is installed
only after a nonzero `$420C` write, so a machine that never enables HDMA has
no HDMA VBlank callback. NTSC and PAL use their selected 224/239-line visible
range, with frame recurrence accounting for the NTSC short scanline.

## Verification

- [x] Pure HDMA state fixture deactivates an initialized channel without
  changing table registers, blocks same-frame restart, and reinitializes on
  the next frame.
- [x] Timed machine fixtures reach H=0 of VBlank in NTSC and PAL overscan;
  zero event budget preserves the active masks, then a retry clears them at
  the event while `$420C` remains enabled.
- [x] Eight mode-4 channels run past VBlank start. The pending shutdown lets
  one admitted slot finish and clears the remaining cursor without a stale
  snapshot error.
- [x] Revision `559eda5` passed
  [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443593),
  [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443514)
  and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443512).
- [ ] Real-console behavior and exact electrical subphases.

Fixtures use generated in-memory cartridge and WRAM data only. No source
code from another emulator was used or indexed.

On 2026-10-25, the full C99 `cc` host suite, optimized `-O3` suite,
ASan/UBSan suite with leak detection disabled, GCC `-fanalyzer` host build,
and repository checker passed.
LeakSanitizer cannot inspect the proc task directory in this execution
environment. One generated `build/host/test-ppu-oam` binary had mode `0644` on
a repeat run, causing `Permission denied`; rebuilding that target restored
the full suite pass. The cause of the artifact mode is unconfirmed. Console
Real-device behavior remains unverified.
