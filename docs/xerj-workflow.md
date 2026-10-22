# XERJ development-search workflow

XERJ is an optional host-only development tool. It indexes MIPSNES2 source and
documentation for structural search; it is not a core dependency, is never linked
into PS2/PSP artifacts and does not affect emulated time or behavior.

MIPSNES2 pins XERJ `v1.0.0-rc.97`, commit
`e6216b6597ca11b816caa4e3e0ca2ffdf73d9a2b`, under Apache-2.0. Linux static archive
URLs and SHA-256 digests are recorded in [tools/xerj.json](../tools/xerj.json).
The setup helper verifies the selected archive before extraction:

```sh
python3 tools/setup_xerj.py /tmp/mipsnes2-tools "$HOME/.cache/mipsnes2-tools"
```

The upstream binary is not committed. Other host systems should use XERJ's official
fail-closed installer and pin the same tag; the MIPSNES2 helper currently covers the
Linux x86-64 and arm64 static builds used for development.

## Corpus policy

Allowed corpora:

- this MIPSNES2 checkout, including first-party tests and documentation;
- manufacturer manuals/datasheets and official PS2SDK/PSPSDK APIs already accepted
  by [references.md](references.md), when their redistribution permits local use;
- independently authored hardware tests/research with recorded revision and license;
- developer-published prose explaining a behavior and correct implementation method,
  recorded as secondary evidence with exact provenance.

Forbidden corpora:

- source code from Snes9x, bsnes, higan, Snesticle, SNES9xTYL, SNES Station or any
  other emulator;
- commercial ROMs, console firmware, proprietary SDK material or unlicensed dumps;
- arbitrary peer-repository collections suggested by a tool or prompt.

An explanatory article by emulator developers does not authorize copying their
source. MIPSNES2 implementations remain original and require validation against
manufacturer material, independent tests or real hardware. `.xerjignore` blocks
common in-tree reference checkout directories; approved reference material should
normally live outside this repository and use a separate, explicitly named prefix.

## Local first-party index

Keep both the node data and autoindex journal outside the source tree. The following
development mode is loopback-only and unauthenticated; never combine `--insecure`
with a non-loopback bind. Disable XERJ feedback so indexing never opens an upstream
PR or submits a report without an explicit owner decision.

```sh
export XERJ_BIN=/tmp/mipsnes2-tools/xerj-1.0.0-rc.97-x86_64-unknown-linux-musl/xerj
export XERJ_DISABLE_FEEDBACK=true
export XERJ_DATA=/tmp/mipsnes2-xerj-data
export XERJ_STATE=/tmp/mipsnes2-xerj-state

"$XERJ_BIN" --insecure --data-dir "$XERJ_DATA"
```

In another shell, inspect the plan before the first write, then index only the
MIPSNES2 checkout under its dedicated prefix:

```sh
"$XERJ_BIN" autoindex . --prefix mipsnes2 --state-dir "$XERJ_STATE" \
  --code-analyzer code --no-semantic --no-graph --dry-run
"$XERJ_BIN" autoindex . --prefix mipsnes2 --state-dir "$XERJ_STATE" \
  --code-analyzer code --no-semantic --no-graph
```

`--no-semantic` keeps this small source corpus on deterministic lexical/identifier
search; `--no-graph` enables the current incremental reconciliation path. Search and
definition lookup then use the same prefix:

```sh
"$XERJ_BIN" search --prefix mipsnes2 "resumable interrupt stale descriptor"
"$XERJ_BIN" def --prefix mipsnes2 prepare_incremental_interrupt
```

## Verified smoke

On 2026-10-21 the pinned Linux x86-64 musl archive matched SHA-256
`f401da4dfc2e5c7dd27580d3896229baa088c598c77735266888669240149a00`.
A dry run followed by first-party indexing covered 144 files, produced 1,123 live
records, indexed all 53 detected code files and reported zero junk files. The sample
query returned `tests/test_cpu_cycles.c`, the interrupt-cycle audit and
`core/cpu/65c816/cpu.c`; definition lookup located
`prepare_incremental_interrupt`. This verifies the optional retrieval workflow, not
emulator correctness, console compilation, compatibility or performance.
