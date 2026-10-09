# GitHub infrastructure and owner setup

Repository: [ReyFxck/MIPSNES2](https://github.com/ReyFxck/MIPSNES2).
Repository files are proposed on `bootstrap/project-foundation`; `main` changes only
after owner review and merge. File existence is distinct from executed automation.
[STATUS.md](../STATUS.md) and [bootstrap audit](bootstrap-audit.md) record real results.

## Repository configuration implemented as files

| File | Purpose | Execution gate |
|---|---|---|
| .github/workflows/ci.yml | GCC/Clang host tests, sanitizer/release builds, checksums/artifacts | Successful real run |
| .github/workflows/build-ps2.yml | Pinned SDK cross-build and ELF artifact | Successful real run and inspected ELF/checksums |
| .github/workflows/build-psp.yml | Pinned SDK cross-build and PBP artifact | Successful real run and inspected PBP/checksums |
| .github/workflows/prerelease.yml | Manual same-revision gated development release | Owner merges, manually dispatches and verifies release |
| .github/ISSUE_TEMPLATE/ | Bug, compatibility and performance forms | File/schema audit; UI activation after default-branch merge |
| .github/pull_request_template.md | Honest verification checklist | File/content audit |
| .github/dependabot.yml | Weekly GitHub Actions updates | Config audit; service activation after merge |

Builds run on PRs, pushes to main, manual dispatch and reusable workflow calls.
No unconditional continue-on-error, no pull_request_target, no default write token.
Actions use verified Node 24 releases pinned to commit SHAs: checkout v7.0.1,
upload-artifact v7.0.2, download-artifact v8.0.2 and cache v6.1.0. Their official
action.yml inputs/runtime were inspected; Dependabot proposes future updates.
`ubuntu-24.04` is the selected runner family; its system packages can still evolve,
so record compiler versions in evidence. Failures must fail jobs.

## Toolchain pinning and reuse

`tools/toolchains.json` pins official ps2dev v2.0.0 and pspdev v20261001 Linux x86-64
archives by SHA-256. Upstream names contain "latest" but release tags/digests are
fixed. `setup_toolchain.py` validates both fresh and cached downloads, rejects digest
mismatches, extracts with `--no-same-owner`, and refuses to overlay existing installs.

Actions cache only archives under `~/.cache/mipsnes2-toolchains` using target, runner
family and lock-file hash. No broad fallback keys and no cached project object files.
Every job extracts into a fresh runner temporary directory. Cold downloads occur
when no matching cache is available; warm reuse must be observed before claiming it
verified. Local install commands are in ps2.md/psp.md. No custom Docker/GHCR image is
needed yet. Reconsider containers only if measured setup cost or reproducibility
needs justify the maintenance. SDK updates require source convention review,
new upstream digest, clean cross-builds, CI and documented compiler changes.

## Artifacts versus releases

Host GCC/Clang and PS2/PSP jobs upload distinct `mipsnes2-<platform>-<sha>` artifacts
(host names include compiler). Each contains output, LICENSE, BUILD.txt and SHA256SUMS;
console packages also include toolchain pins. Artifact retention is 14 days. PR SHA
may be the GitHub merge-test commit; BUILD.txt records the actual checkout identity.
These are development outputs, not formal releases or proof of working emulation.

Chosen prerelease strategy: **manual dispatch from reviewed main only**. A guard
rejects other refs. Reusable host/PS2/PSP jobs build and test that same revision. Only
the publish job, after all succeed, receives contents:write. It downloads console
artifacts from the same workflow run, verifies checksums, packages corresponding
source via git archive, records SHA/build date and publishes a GitHub Pre-release
with unique `dev-<sha>-<run>-<attempt>` tag. No release per normal commit, no moving
release overwrite. Manual releases remain deliberate owner actions. No bootstrap
release is published by this task. Versioned alpha/beta/stable releases (for example
v0.1.0-alpha) are a separate future policy.

Do not declare prerelease support end-to-end verified before a real successful run
and inspection of tag, prerelease flag, matching source/binaries and checksums.
Built-in GITHUB_TOKEN authentication is sufficient where repository settings allow;
never add personal tokens or print secrets. Read-only workflows do not persist checkout
credentials. Do not expose trusted self-hosted runners to untrusted PR code.

## Manual owner/admin checklist

These are recommendations, not claims of configured settings. No admin settings are
modified by this bootstrap. Check the actual setting before updating STATUS.md.

- [ ] Review and merge the bootstrap PR when satisfied
- [ ] Confirm Actions are enabled and pinned official actions permitted
- [ ] Confirm read-only default workflow permissions and release-job write policy
- [ ] Inspect first host/PS2/PSP runs, artifacts and checksums
- [ ] Configure main ruleset preventing force pushes and accidental deletion
- [ ] Require PR workflow for substantial changes
- [ ] Add required CI checks only after their actual names appear in successful runs
- [ ] Review bypass permissions and avoid requirements the single owner cannot satisfy
- [ ] Verify issue forms and Dependabot activation after default-branch merge
- [ ] Create recommended labels if useful
- [ ] Create roadmap milestones if useful
- [ ] Manually dispatch and inspect a prerelease when appropriate

Recommended check jobs are Host (gcc), Host (clang), PS2 scaffold build and PSP
scaffold build; select the actual contexts GitHub reports, not guessed strings.
Do not configure impossible required checks before workflows exist.

## Recommended label taxonomy

`cpu`, `bus`, `ppu`, `apu`, `spc700`, `dsp`, `dma-hdma`, `cartridge`, `superfx`,
`sa1`, `enhancement-chip`, `ps2`, `psp`, `performance`, `compatibility`, `regression`,
`testing`, `documentation`, `build`, `github-actions`, `blocked`.

No labels are claimed created. Issue templates deliberately do not reference labels
that may not exist. Milestones should map to ROADMAP.md's Phase 0 through Phase 14;
none are claimed created. Creating labels/milestones and rulesets is separate admin
work, not accomplished by committing this document.
