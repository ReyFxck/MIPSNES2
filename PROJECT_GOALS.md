# MIPSNES2 Project Constitution

**Built for the hardware, not ported to it.**

**DO NOT TURN THIS PROJECT INTO A SNES9X PORT.**

| Principle | Binding project direction |
|---|---|
| PRIMARY PLATFORM | PlayStation 2 |
| SECONDARY PLATFORM | PlayStation Portable |
| HOST | Development/testing/tooling only |
| PRIMARY OBJECTIVE | Run the practical SNES software library with strong compatibility and sustained full-speed performance wherever feasible on real target hardware. |
| CORE PRINCIPLE | Implement hardware behavior rather than game hacks. |
| PERFORMANCE PRINCIPLE | Measure before optimizing. |
| PLATFORM PRINCIPLE | Optimize each console independently; neither is a generic PC. |
| PS2 PRINCIPLE | Aggressive EE/MMI/scratchpad/DMA/GS/VU optimizations are allowed when measured and correct. |
| PSP PRINCIPLE | Independent Allegrex/MIPS/VFPU/GU optimizations are allowed when measured and correct. |
| PORTABILITY PRINCIPLE | Portable core behavior must not prohibit worthwhile platform acceleration. |

## Purpose and priorities

The question is: **What is the best SNES emulator we can build specifically for PlayStation 2 and PSP?** It is not how to port a desktop emulator. High practical commercial-library compatibility, predictable execution, low CPU/memory overhead, maintainable low-level architecture and real-console performance drive decisions. Full speed is a goal, never an unmeasured promise.

Accuracy where required. Performance everywhere practical. Correct behavior is mandatory wherever software, hardware tests, timing or architectural consistency depends on it. Absolute theoretical cycle perfection is not the final objective. Approximation proposals must record the behavior, rationale, measured cost/benefit, known compatibility impact, evidence and refinement trigger in subsystem docs and STATUS.md. No silent approximation of important hardware behavior.

PS2 wins architectural tradeoffs when a materially faster implementation preserves correctness and testability, isolates platform details, has measurements and is documented. PSP must not blindly inherit PS2 choices. Host is a laboratory, not a desktop product. No SDL dependency is planned.

## Independent implementation and licensing

Preserve the existing [GPL-3.0 license](LICENSE) unless the owner explicitly decides otherwise. The original LICENSE is not to be reformatted or replaced. Source implementations must be original. Prefer hardware documentation, public research, independently authored tests and real-hardware observations; record source provenance. Source code from other emulators is prohibited as implementation material or a search corpus. A developer's separately published technical explanation may be consulted as secondary evidence only when it explains behavior and a correct implementation method; cite it precisely, independently author the MIPSNES2 implementation and validate it against stronger evidence. This is not a Snes9x, SNES9xTYL, bsnes, higan or Snesticle derivative, port or wrapper.

Do not commit commercial ROMs, copyrighted firmware or credentials. Test ROM redistribution needs a documented license and origin; otherwise allow local user-supplied fixtures only.

## Compatibility policy

A game should work because its hardware is implemented. ROM-name/CRC-specific compatibility branches are forbidden in normal architecture. Last-resort temporary diagnostics must name the game and unresolved hardware issue, carry a TODO, be listed in STATUS.md, specify a removal plan and keep the underlying feature unchecked. No diagnostic exceptions currently exist.

## Engineering constraints

Start with an interpreter, not a dynarec. Base SNES precedes enhancement chips. Save states wait for stable state ownership. Keep logical 24-bit SNES addresses distinct from target physical addresses. Shared core depends on no console SDK or GUI; platform backends call the core. Inject optimized kernels at explicit interfaces. Reference behavior and regression tests must remain available where useful.

Avoid per-frame allocation, unnecessary dispatch, pointer chasing, branching, synchronization and memory traffic. Preallocate bounded buffers when their sizes are known. Use fast common paths and slow rare paths while preserving behavior. No performance claim is accepted without measurements; assembly, MMI, VU or VFPU are not intrinsically faster.

Working low-level code is protected: do not replace it merely for cleanliness, modernity or portability. Measure before replacement. PS2 and PSP need not share low-level implementations.

## Project memory and change control

Repository documentation is authoritative; chat memory is not. [STATUS.md](STATUS.md) owns task state; [ROADMAP.md](ROADMAP.md) owns sequencing. A checked box requires explicit evidence satisfying that task's verification criteria. Parent milestones remain unchecked while required children are unfinished. Documentation existence is not subsystem completion. Workflow existence is not verified CI.

Every substantial change must audit status against source, run applicable tests/builds, scan TODO/FIXME/placeholders, review boundaries and documentation, and record failures and unavailable validation. Update architecture, performance and compatibility records when affected. Real PS2/PSP hardware is authoritative for speed; PCSX2/PPSSPP are supplementary tools.

Use focused branches and reviewed PRs; no automatic bootstrap merge. Keep Actions reproducible, pinned, minimally privileged and honest. Ordinary commits produce artifacts, not permanent releases. Owner/admin steps remain documented and unchecked until verified.

## Definition of done

Written != done. Compiled != correct. Boots != compatible. Menu != playable.
60 FPS in PCSX2/PPSSPP != full speed on real hardware.
Assembly != optimized. Optimized != correct.
Workflow YAML != working CI. Artifact configuration != produced artifact.
Pre-release YAML != verified release system.
