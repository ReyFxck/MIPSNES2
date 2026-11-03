# Compatibility

**No commercial games have been executed and no game is claimed as working.**
One pinned redistributable CPU test ROM has been executed on the host as a
conformance check; it does not establish game or target-console compatibility.

## Host conformance

| Test | Revision | Scope | Result | Evidence |
|---|---|---|---|---|
| gilyon `cputest-basic` | `snes-tests` v1.4 / `5ecdf555da920f0bd7b157542141965a8120186d` | 65C816 instruction behavior through the shared machine | Pass, 1,107/1,107 cases; host only | [licensed-ROM runner audit](docs/65c816-licensed-rom-runner-audit.md) |

The full variant is deliberately tracked as a failure frontier, not a pass: it
currently reaches test `$0103`, an emulation-mode `COP` stack/vector case.

## PlayStation 2

A ROM-free synthetic RFAuds2 lifecycle diagnostic reported `PASS` in one
NetherSX2 v2.2n-4248 (Patched) screenshot on Motorola fogos after SBV loader
preparation: 1,920 PCM frames accepted, start/statistics/shutdown succeeded.
This is not a game compatibility or audible-output result; there is no game
entry, real-PS2 result or sustained timing claim. See
[the PS2 audio lifecycle audit](docs/ps2-audio-lifecycle-diagnostic-audit.md).

| Game/test | Region | Enhancement chip | Revision | State | Issues | Real hardware/model | Evidence |
|---|---|---|---|---|---|---|---|

## PSP

| Game/test | Region | Enhancement chip | Revision | State | Issues | Real hardware/model | Evidence |
|---|---|---|---|---|---|---|---|

## Reporting

Allowed practical states/tags: Does Not Boot, Boots, Title/Menu, In-Game, Playable,
Full Speed, Graphical Issues, Audio Issues, Timing Issues, Input Issues, Crash,
No Known Issues. Combine a progress state with issue tags when useful. Record
untested as untested rather than Does Not Boot. Title screen != compatibility.

Include tested duration/scenes, reproduction inputs, graphics/audio/input/timing
observations, commit, target, toolchain and settings. Distinguish actual PS2/PSP from
PCSX2/PPSSPP and record emulator versions separately. Full Speed requires sustained
real-hardware measurement under PERFORMANCE.md's protocol. No Known Issues means
none observed in the documented scope, not proof of universal correctness.

Never request or commit commercial ROMs. Identify lawful local ROMs by title/region
and optionally hash. Follow CONTRIBUTING.md's hardware bug workflow and link the
minimal regression test. No game-specific diagnostic hacks currently exist.
