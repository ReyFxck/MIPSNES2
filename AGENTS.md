# STOP — READ BEFORE MODIFYING MIPSNES2

**Built for the hardware, not ported to it.**

**DO NOT TURN THIS PROJECT INTO A SNES9X PORT.**

> DO NOT rewrite working low-level code merely to make it cleaner, more modern, or more portable.
>
> Performance-critical platform-specific code may be intentional.
>
> Measure before replacing it.

Before modifying this repository read:

1. PROJECT_GOALS.md
2. ARCHITECTURE.md
3. STATUS.md
4. ROADMAP.md
5. PERFORMANCE.md if touching performance-sensitive code
6. Relevant docs/ subsystem documentation
7. docs/references.md before researching hardware behavior or SDK APIs

## Binding rules

- Repository documentation is the source of truth; chat history is not.
- PS2 is primary, PSP secondary, host development/testing only.
- Check STATUS.md first. Never assume unfinished work is complete.
- Only mark implemented AND verified tasks `[x]`. Leave uncertain tasks unchecked.
- Preserve GPL-3.0 and valid existing work. Explain any removal.
- Write original implementations; do not turn MIPSNES2 into another emulator's port.
- Do not use or XERJ-index source code from other emulators. A developer's separately
  published technical explanation may be consulted as secondary evidence only when
  it documents behavior and implementation method; cite it, write original code and
  validate against hardware documentation, independent tests or real hardware.
- Fix hardware behavior, not individual games. Diagnostic hacks require TODO, status entry, hardware issue and removal plan.
- Do not mix platform APIs/physical addresses into core. Isolate optimizations.
- Do not casually remove MIPS assembly, MMI, VU microcode, VFPU, specialized DMA or cache-sensitive layout.
- Profile PS2 and PSP independently. Keep target-specific paths when justified.
- Do not invent SDK APIs, SDK variables, successful builds, executed tests or real-hardware results.
- Do not hide blockers or silently remove TODO/FIXME items.
- Update STATUS.md after meaningful work; architecture after architecture changes; PERFORMANCE.md after performance work; COMPATIBILITY.md after compatibility changes.
- Do not mark CI complete merely because YAML exists. Record run URLs and conclusions.
- Keep Actions read-only except the release job. Never commit or print secrets.
- No commercial ROMs. Document redistributable fixture provenance.
- Do not implement dynarec or enhancement chips during foundation work.

## Status notation

Only these states are allowed:

```text
- [ ] Not implemented or not verified
- [x] Implemented AND verified
- [ ] [IN PROGRESS] Work started but incomplete
- [ ] [BLOCKED] Blocked — include the reason
```

Evidence must identify commands, compiler/toolchain, revision or scope, results and unavailable checks. Completion of a structural document does not complete the behavior described by it. Parent tasks require their required children. Never inflate progress.

## End-of-task audit (mandatory)

1. Read STATUS.md and compare with actual source.
2. Run relevant tests and available builds; record precise results/failures.
3. Search modified areas for TODO/FIXME/placeholders; leave unresolved behavior unchecked.
4. Check core/platform boundaries and documentation consistency.
5. Update status, architecture, performance and compatibility as applicable.
6. Inspect relevant GitHub Actions results; do not infer green CI.
7. Report outstanding blockers and exactly one primary next milestone.

For bootstrap use `bootstrap/project-foundation`, open a PR into `main`, and **do not merge automatically**. Future substantial work should use focused feature branches and reviewed PRs. Owner controls administrative settings and release policy.
