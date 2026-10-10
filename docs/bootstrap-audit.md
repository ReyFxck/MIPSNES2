# Bootstrap audit — 2026-10-09

## Scope and original repository

Inspected the complete initial tracked tree at
`472c5e52427015d4ecb643f7f86849f7c2404568`: only README.md and LICENSE existed.
README's original project identity is retained and expanded. LICENSE remains
byte-for-byte identical, SHA-256
`3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.
The local branch is `bootstrap/project-foundation`. No emulator implementation was
copied or added. No commercial ROM, firmware, secret or benchmark result was added.

## Local verification results

Commands run from the repository root unless otherwise stated. Audited scope is the
bootstrap tree in this PR; no emulator behavior is covered. Host: Linux x86-64,
GCC 13.3.0, GNU Make 4.3; Python standard-library tests. Console cross compilers both
report GCC 15.2.0. Toolchain archive identities are pinned in tools/toolchains.json.

| Command/check | Actual result |
|---|---|
| `make all` | Passed: host probe and static common utility library |
| `make test` | Passed: enabled/disabled logger and address contracts, CLI, consistency |
| `make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` | Passed; test checks remain active |
| `make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Build passed; execution blocked by LeakSanitizer /proc task access |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` | Passed AddressSanitizer + UBSan; no leak-detection claim |
| `make -C platform/ps2` with documented PS2DEV/PS2SDK/PATH | Passed: MIPS ELF |
| `make -C platform/psp` with documented PSPDEV/PATH | Passed: MIPS ELF, PRX, PARAM.SFO and EBOOT.PBP |
| `python3 tools/setup_toolchain.py psp <fresh-root> <cache-root>` | Passed verified cached archive extraction |
| Same installer with truncated PS2 cache | Correctly rejected SHA-256 mismatch before extraction |
| `actionlint -shellcheck= -pyflakes=` version 1.7.12 | Passed after fixing job-level runner context; external shellcheck/pyflakes not installed |
| PyYAML 6.0.3 BaseLoader parse of all nine GitHub YAML files | Passed; parser check distinct from execution |
| `git diff --check` | Passed |
| TODO/FIXME/placeholder/stub scan of core/platform/docs | Documentation warnings only; no pretend emulation functions |
| Required documents/links, identity, checkbox syntax and core dependency guard | Passed |
| Original LICENSE SHA-256 | Identical to initial repository |

Local Clang was not installed; GCC was used. CI is configured to cover both compilers
and full sanitizer settings. The build probes are not intended as usable console
applications and their device lifecycle remains unverified.

Local output identities (not performance evidence):

- PS2 ELF: `992db6ab5c1952fc325a96c87e28c5de1a179efe2dc8ff5e38d7e2c1acea000a`
- PSP PBP: `a75594e1f64d15e32cb92cda71c0293683333fe38e6a49af00de2731c2fb0a35`

Cross-build paths/debug metadata may make binary hashes environment-dependent.
GitHub artifacts must be verified against their own published checksums.

## GitHub execution

[Bootstrap PR #1](https://github.com/ReyFxck/MIPSNES2/pull/1) is open into main,
not merged. Initial source commit: `e70bca99625dde244bae07d7eb84ac3cd97d257f`.
GitHub PR merge-test SHA: `5a3718c2456c7bc9bf91d799668794d95aee687d`.

| Workflow | Actual run | Result |
|---|---|---|
| Host CI | [37995060946](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995060946) | GCC and Clang release + full sanitizer tests passed |
| PS2 CI | [37995060790](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995060790) | Download/checksum/extract/build/stage/upload/cache-save passed |
| PSP CI | [37995060838](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995060838) | Download/checksum/extract/build/stage/upload/cache-save passed |
| Development prerelease | Not dispatched | Main-only gate; intentionally unverified |

Artifacts were confirmed via GitHub artifact metadata: host GCC 11647195192,
host Clang 11646019974, PS2 11646622837 and PSP 11646357988. All have nonzero sizes,
commit-identified names and 14-day retention. CI staged files and ran checksum
verification before upload. The PS2 log explicitly shows ELF/LICENSE/toolchains.json/
BUILD.txt checksum OK. Artifact download API produced references, but fetching their
signed URLs in this local runtime returned HTTP 403. No local post-download archive
inspection is claimed; uploaded outputs are verified through successful job logs
and artifact metadata. Owner can download from the linked runs.

Initial runs warned that v4 Actions' Node 20 runtimes were being forced to Node 24.
The bootstrap follow-up pins current official Node 24 releases after inspecting
actual action.yml inputs: checkout v7.0.1, upload-artifact v7.0.2, download-artifact
v8.0.2, cache v6.1.0. The new build-action pins were then verified in the successful
second round below. download-artifact is used only in the undispatched prerelease
workflow and has static/API validation only. No prerelease was triggered.

### Node 24 and warm-cache verification

Source commit: `21d568f937eca9b2ba0c71877e3d0f43a4fa4e8b`.
PR merge-test SHA: `00460a6e6a53a93fcb7a65fb0f622d32a3b4c0e0`.

| Workflow | Actual run | Result |
|---|---|---|
| Host CI | [37995297757](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995297757) | GCC/Clang release and full ASan/UBSan tests passed; artifacts uploaded |
| PS2 CI | [37995297758](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995297758) | Warm cache restored; archive reverified/extracted; ELF build/checksums/upload passed |
| PSP CI | [37995297798](https://github.com/ReyFxck/MIPSNES2/actions/runs/37995297798) | Warm cache restored; archive reverified/extracted; PBP build/checksums/upload passed |

Console job logs explicitly report `Cache restored successfully`, `Reusing cached
archive` and `Verified and extracted`, followed by all staged-file checksum OK
lines. No Node 20 deprecation warning remains. Fresh artifact metadata was inspected:
host GCC 11646697809, host Clang 11646253463, PS2 11646383147, PSP 11647170460.
Artifact names contain the exact merge-test SHA. The local download restriction
reported above still limits independent post-download inspection.

These successful runs verify all unchanged executable/build content in the final
documentation update. Final documentation is also checked by `make test`, actionlint
and `git diff --check`; subsequent PR checks remain visible on the PR.

Command-line git push lacked credentials and failed. The authenticated GitHub
connector created the branch/tree/commit and opened the PR instead. A subsequent
fetch confirmed remote and locally audited trees are identical:
`be1140105373d5888b498bf177f89adc8ef5d983`. No token was requested or committed.

## Checklist verification scope

Documentation checks mean files were read/generated from the constitution and audited
for scope, links and consistency. Structure checks mean directories contain tracked
content. Contract checks mean the small foundation C tests passed, not a working bus.
Architecture checks mean inspection of this small original tree plus include guards;
they do not predict future correctness. Workflow-file checks mean actionlint/policy
review, not an executed workflow. Issue/PR/Dependabot checks mean configuration files
were inspected; service activation is tracked separately.

## Exact STATUS.md promotions at local audit

All following rows changed from unchecked to `[x]`; no future emulation, runtime or
performance row was promoted:

- `Host build system`
- `Minimal host target compiles`
- `Minimal automated test runs`
- `Logging contract and disabled configuration`
- `Shared logical-address and bus interface contracts`
- `PS2 scaffolding uses verified SDK conventions`
- `PSP scaffolding uses verified SDK conventions`
- `PS2 scaffold cross-compiles`
- `PSP scaffold cross-compiles`
- `GPL-3.0 LICENSE preserved byte-for-byte`
- `README.md`
- `PROJECT_GOALS.md`
- `ARCHITECTURE.md`
- `STATUS.md`
- `ROADMAP.md`
- `PERFORMANCE.md`
- `COMPATIBILITY.md`
- `CONTRIBUTING.md`
- `AGENTS.md`
- `docs/65c816.md`
- `docs/bus.md`
- `docs/ppu.md`
- `docs/spc700.md`
- `docs/dsp.md`
- `docs/apu.md`
- `docs/dma-hdma.md`
- `docs/scheduler.md`
- `docs/cartridge.md`
- `docs/enhancement-chips.md`
- `docs/ps2.md`
- `docs/psp.md`
- `docs/host.md`
- `docs/optimization.md`
- `docs/testing.md`
- `docs/debugging.md`
- `docs/github-setup.md`
- `core/`
- `core/cpu/65c816/`
- `core/bus/`
- `core/ppu/`
- `core/apu/spc700/`
- `core/apu/dsp/`
- `core/dma/`
- `core/scheduler/`
- `core/cartridge/`
- `core/enhancement/`
- `core/debug/`
- `core/common/`
- `platform/ps2/`
- `platform/psp/`
- `platform/host/`
- `tests/`
- `tools/`
- `Core has no PS2 API dependency`
- `Core has no PSP API dependency`
- `Host does not control architecture`
- `Dependency direction documented`
- `PS2 dedicated optimization policy documented`
- `PSP dedicated optimization policy documented`
- `No game-specific hacks`
- `No copied emulator implementation`
- `No fake completed subsystem`
- `No invented SDK APIs`
- `Host CI workflow file audited`
- `PS2 workflow file audited`
- `PSP workflow file audited`
- `Pre-release workflow file audited`
- `Artifact upload configuration audited`
- `SHA-256 workflow support audited`
- `Issue templates`
- `Pull request template`
- `Dependabot configuration`
- `GitHub setup documentation`

## Observed setup failures

- First PS2 archive download was truncated and failed SHA-256 verification. It was
  not used. Another cached copy was also truncated on later inspection and rejected.
  Download, checksum verification, extraction and PS2 build in one operation then
  succeeded with the exact pinned digest. No digest check was bypassed.
- Initial PSP extraction attempted unavailable archive UID/GID ownership and returned
  errors. Re-extraction with `tar --no-same-owner` succeeded. The reusable installer
  uses this option, and SHA-256 verification precedes extraction.
- Initial documentation generation lacked docs/ and failed; directory creation and
  rerun resolved it. No successful test was claimed for that failed command.
- Initial guessed upstream sample paths did not exist. Actual SDK source trees were
  inspected; source links in ps2.md/psp.md identify the verified conventions.

- First repository consistency run misclassified Markdown link bullets as task boxes.
  The matcher was corrected to exclude links; the complete test command then passed.
- Actionlint initially rejected `runner.temp` in job-level env. Moving these values
  into step-level env resolved all three errors; the linter then passed.
- Full local sanitizer execution failed because LeakSanitizer could not inspect
  process tasks; the explicitly documented no-leak-detection run passed. CI retains
  full sanitizer settings.

## Platform limitations

Host validation covers foundation contracts only. PCSX2, real PS2, PPSSPP and real
PSP have not been run. All real-console runtime, compatibility and speed tasks remain
unchecked. Performance measurements: none; PERFORMANCE.md is TBD throughout.

## Outstanding release/admin work

No merge, release, ruleset, label or milestone has been performed. Manual prerelease
end-to-end verification waits for reviewed main and an owner-triggered run. Dependabot
service execution and issue-form activation require default-branch merge/inspection.

## Additional STATUS.md promotions after first GitHub verification

- `GitHub Actions host CI` changed to `[x]`.
- `Automated host tests in GitHub Actions` changed to `[x]`.
- `PS2 CI build` changed to `[x]`.
- `PSP CI build` changed to `[x]`.
- `Build artifacts` changed to `[x]`.
- `SHA-256 generation` changed to `[x]`.
- `First full CI run verified` changed to `[x]`.
- `Bootstrap PR opened` changed to `[x]`.

ROADMAP.md Phase 0 structural/docs/build/test/logging/boundary/scaffold rows now
match verified work. Phase 0 parent/GitHub infrastructure remain unchecked while
release and default-branch/admin validation remains outstanding.

## Final local audit and remaining state

- `Dependency/toolchain caching` changed from unchecked to `[x]` after both warm-cache
  restores and digest revalidation succeeded in real Actions runs.
- The Phase 0 parent is `[IN PROGRESS]`: repository bootstrap is review-ready, while
  release/admin/default-branch activation gates remain unchecked. No child blocker
  is hidden by marking the parent complete.
- Total verified project task rows in STATUS.md: 82, excluding the notation legend.
  No CPU, bus mapping, PPU, APU, chip, runtime, compatibility or performance row is checked.
- STATUS.md was reread against source; required docs, directory contents, core/platform
  boundary, TODO/FIXME/placeholder scan and the unchanged LICENSE were checked.
- The final evidence commit changes documentation only. Host consistency/tests and
  actionlint are rerun; console executable sources/SDK pins are unchanged from the
  successful local and two GitHub build rounds.
- No merge, release or repository administrative setting was changed. Exactly one
  recommended next milestone: **65C816 foundation + minimal SNES bus + automated CPU tests**.
