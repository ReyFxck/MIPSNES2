# Contributing

Read [AGENTS.md](AGENTS.md), [PROJECT_GOALS.md](PROJECT_GOALS.md),
[ARCHITECTURE.md](ARCHITECTURE.md), [STATUS.md](STATUS.md), [ROADMAP.md](ROADMAP.md)
and the relevant subsystem docs first. Repository records supersede chat memory.

## Work process

Use a focused `feature/<topic>` branch from main. Bootstrap uses
`bootstrap/project-foundation`. Submit a PR, wait for CI and owner review; do not
automatically merge. Explain the hardware problem, change, evidence, limitations
and documentation updates. Unchecked verification boxes are honest and welcome.

Implement original code under the existing GPL-3.0 policy. Cite hardware research,
test provenance and SDK revision in docs; do not transplant emulator implementations.
Do not attach or request commercial ROMs. Redistributable fixtures need licenses.

## Verification gates

Run `make test` and relevant target builds. Record compiler and flags. Compare
reference and optimized paths. A build probe is not emulation validation. Do not
claim PCSX2/PPSSPP or real-hardware testing without running it. No benchmark means
TBD, never an estimated result presented as measured. Keep core free of SDK/GUI APIs.

For optimization: correct C, tests, profile, hotspot, baseline, consider improved C,
then target-specific implementation, regression, new benchmark and optimization log.
Assembly and vector units are tools, not goals. Working low-level code must not be
replaced for aesthetic portability. Document every approximation and refinement trigger.

## Hardware bug workflow

1. Reproduce with revision, platform, region and inputs.
2. Identify subsystem and investigate documented/observed hardware behavior.
3. Author a minimal regression test where possible.
4. Fix hardware implementation and run prior regressions.
5. Revalidate the game; update STATUS.md and COMPATIBILITY.md.

Diagnostic game-specific hacks require an explicit TODO, game, underlying issue,
status entry and removal plan. The hardware feature remains incomplete.

## Completion and audit

`[x]` means implemented AND verified under the task's criteria, not merely written.
Use only STATUS.md's four allowed states. Parent milestones require all required
children. At the end inspect status against code, run relevant tests/builds, scan
TODO/FIXME/placeholders, check dependencies and document failures/blockers. Update
architecture/performance/compatibility when affected; inspect actual CI results.
See docs/testing.md for evidence format and docs/github-setup.md for owner actions.
