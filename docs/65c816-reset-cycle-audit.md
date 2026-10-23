# 65C816 resumable RESET-vector audit

Date: 2026-10-23

## Scope

This milestone moves the existing programmer-visible RESET state transition and
the two reset-vector reads into the caller-owned CPU operation cursor. It does not
claim the W65C816's complete electrical or internal reset-cycle sequence.

The implementation remains entirely in the platform-neutral 65C816 core. PS2, PSP
and host backends receive no reset logic, and there are no title-specific paths.

## Implementation checklist

- [x] Add `mipsnes2_cpu_cycle_begin_reset` without changing instruction-cursor
  startup or exposing private phases.
- [x] Expose a side-effect-free control descriptor before any reset mutation.
- [x] Reject a missing read bus with `INVALID_BUS` before changing CPU state.
- [x] Commit the established emulation-mode, P, D, DBR, PBR, S, X and Y invariants
  before the vector callbacks, preserving the previous callback-visible ordering.
- [x] Expose low and high vector reads at logical `00:FFFC` and `00:FFFD`.
- [x] Install PC and clear WAI/STP, pending edge inputs and diagnostic stop state
  only when the high-vector read commits.
- [x] Retain `mipsnes2_cpu_reset` as a compatibility wrapper that drives the same
  cursor to completion.
- [x] Preserve the established zero `instruction_cycles` RESET accounting instead
  of inventing unsourced dummy/internal cycles.
- [x] Keep core/backend separation and avoid game-specific behavior.
- [x] Use no source code from another emulator.

The programmer-visible reference remains Western Design Center's
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
already adopted by the project for reset state and vector behavior. No emulator
implementation was consulted or copied.

## Deterministic verification checklist

- [x] Repeated control and vector peeks produce no CPU or bus effect.
- [x] A changed control descriptor is rejected as stale before mutation.
- [x] The control commit applies only the pre-vector programmer state.
- [x] Low-vector commit reads `00:FFFC` exactly once without changing PC.
- [x] High-vector commit reads `00:FFFD` exactly once, installs the little-endian
  vector and completes the reset state transition.
- [x] Missing-read-bus completion performs no bus access and leaves CPU state
  unchanged.
- [x] Existing atomic-reset, SNES-bus, cartridge and scheduler tests remain green.
- [x] Host GCC default and release suites pass.
- [x] GCC AddressSanitizer/UndefinedBehaviorSanitizer suite passes.
- [x] Pinned PS2 cross-build passes before PSP validation.
- [x] Pinned PSP cross-build passes after PS2 validation.
- [x] Repository policy checker and link/checkbox validation pass.
- [x] XERJ project-only dry-run/index/search/definition lookup completes with
  feedback disabled.
- [x] GitHub Host, PS2 and PSP workflows pass for the published implementation.

Local evidence:

| Check | Result |
|---|---|
| `make BUILD_DIR=build/reset-dev -j2 test` | Passed all 14 C test executables and the host Python test |
| GCC release suite | Passed with `-O2 -g -DNDEBUG` |
| GCC ASan/UBSan suite | Passed with leak detection disabled for this managed environment |
| PS2 v2.0.0 / GCC 15.2.0 | Passed; 1,811,972-byte MIPS N32 ELF, SHA-256 `e66b047d132daa5764895bcfa6b32a51155ce9616f0fb95cb6f081d14bf9b0d2` |
| PSP v20261001 / GCC 15.2.0 | Passed; 253,690-byte PBP, SHA-256 `70f932e82f619ca9bad4798fa608f6d7c863cab2ef6bbc992f03bdb65c9f1edd` |
| XERJ v1.0.0-rc.97 | Indexed 157 first-party files / 1,257 records and all 54 detected code files; zero junk files; RESET search and definition lookup passed |

Local Clang is unavailable. The GitHub Host workflow remains the required Clang
release and sanitizer gate. These console results are cross-builds, not emulator or
real-hardware runtime validation.

Remote implementation commit
[`365dd640e98e4baa5efcd5318850c48eff90ee7a`](https://github.com/ReyFxck/MIPSNES2/commit/365dd640e98e4baa5efcd5318850c48eff90ee7a)
passed all configured workflows:

- [Host CI run 39969422406](https://github.com/ReyFxck/MIPSNES2/actions/runs/39969422406) — GCC and Clang release, sanitizer, repository-policy and deterministic test jobs passed; both artifacts were uploaded;
- [PS2 CI run 39969422449](https://github.com/ReyFxck/MIPSNES2/actions/runs/39969422449) — pinned toolchain install, cross-build, artifact staging, checksum verification and upload passed;
- [PSP CI run 39969422272](https://github.com/ReyFxck/MIPSNES2/actions/runs/39969422272) — pinned toolchain install, cross-build, artifact staging, checksum verification and upload passed.

## Completion criterion

This block is complete when RESET can be paused between its control transition and
either vector read, each committed descriptor has exactly one documented effect,
the atomic wrapper remains behaviorally compatible, deterministic host tests pass,
PS2 then PSP cross-build successfully, XERJ verifies the first-party index, and all
required GitHub workflows are green.

## Honest boundary

The cursor exposes the meaningful programmer-state transition and vector accesses;
it does not model reset-pin timing, internal/dummy accesses, VPA/VDA activity or a
hardware-verified processor-cycle count. The timed 5A22 scheduler bridge still does
not retain and resume this cursor.

## Next milestone

Wire the existing operation cursor into the platform-neutral 5A22 timing bridge so
it can pause before a CPU operation that would cross the scheduler event horizon,
with transactional error handling and deterministic chunked-versus-one-shot tests.
