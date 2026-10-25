# SETINI overscan timing audit

Date: 2026-10-25

## Scope

The platform-neutral machine now owns the `$2133` SETINI byte. Bit 2 selects
the non-interlaced visible height used by the shared H/V state: normal mode
starts VBlank at V=225 and overscan starts it at V=240. The configuration
flag remains the deterministic power-on seed. Runtime writes before V=225
update the active frame, while writes from V=225 onward are retained and
applied at V=0 of the following frame.

The register route exists even when no optional VRAM, CGRAM or OAM buffer is
attached. Internal PPU registers are handled first and unhandled accesses
still fall through to the caller's PPU interface, preserving the core/backend
boundary.

OAM reload and HDMA VBlank-deactivation callbacks now probe the normal VBlank
boundary first. If overscan is active, they defer to V=240 and then schedule
the next frame's V=225 probe. A `$420C` enable between V=225 and V=240 also
schedules deactivation at the current frame's V=240 boundary. The machine
seeds an empty, register-bound HDMA state at power-on so this late-enable path
has valid shared state before the first frame-initialization callback.

## Deterministic verification

`tests/test_machine.c` covers:

- a PAL `$2133` write at V=0 moving the OAM reload from V=225 to V=240;
- the OAM pointer remaining unchanged at the rejected V=225 probe and
  reloading at V=240/H=40;
- NTSC normal and dynamically selected PAL overscan HDMA deactivation;
- `$420C` enabled during an overscan-only visible line and deactivated at the
  current frame's V=240 boundary;
- a `$2133` write after V=225 leaving the current VBlank active, then taking
  effect at the next V=0;
- no VBlank at the following frame's V=225 and VBlank beginning at V=240;
- SETINI operation without optional PPU memory attached.

The completion gate is a clean host suite and repository check followed by
passing pinned PS2 and PSP cross-build workflows. Cross-compilation checks
that both target build graphs consume the same shared-core change; it is not
runtime or hardware-timing validation.

Local verification on 2026-10-25 passed the default C99 `make test` suite,
an `-O3 -DNDEBUG -fstrict-aliasing` suite, an ASan/UBSan suite with leak
detection disabled, the repository checker and a GCC `-fanalyzer` build.
Pinned target CI is pending publication of the implementation revision.

## Deliberate limits

Only the overscan bit is behaviorally connected. Interlace, pseudo-hires and
external synchronization remain unimplemented. The renderer's output contract
is still a 256-pixel line and no target backend presents a continuous
overscan-sized frame.

The frame-level cutoff deliberately avoids inventing the documented complex
effects of writes during V=225 through V=240. Sub-scanline writes late on
V=224, exact NMI/HDMA transition behavior, interlace interaction and electrical
sampling still need focused evidence and fixtures. No game-specific behavior
is present.

## Sources

- Nintendo, [*SNES Development Manual, Book I*](https://floating.muncher.se/bot/manual/book1_text.pdf),
  PPU register description for `$2133`, inspected 2026-10-25.
- Anomie, [SNES register documentation](https://github.com/gilligan/snesdev/blob/master/docs/snes_registers.txt),
  `$2133` and `$4200/$420C` notes, inspected 2026-10-25. It documents that
  overscan changes matter near the end of the frame and reports more complex
  late-line effects; those effects are left outside this bounded block.

No source code from another emulator, commercial ROM or game-specific trace
was used or indexed.
