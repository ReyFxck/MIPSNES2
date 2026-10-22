# Foundation tooling

- `check_repository.py`: standard-library consistency guard, invoked by `make test`.
- `setup_toolchain.py`: pinned SHA-256 verified upstream toolchain installation.
- `toolchains.json`: reviewed archive URLs, versions and digests; no floating latest release.
- `setup_xerj.py`: optional pinned, SHA-256-verified XERJ host-tool installation.
- `xerj.json`: reviewed XERJ tag, commit, Linux static archives and digests.

The word latest in upstream asset filenames is part of their name; the release tag
and SHA-256 are pinned. See [GitHub setup](../docs/github-setup.md) for cache, release
and owner/admin instructions. The dependency ecosystem is GitHub Actions only; no
unused package managers are configured in Dependabot.

XERJ is a host development aid only. It is not linked into the emulator and does not
enter PS2/PSP artifacts. Follow [the XERJ workflow](../docs/xerj-workflow.md); it
indexes MIPSNES2 first-party source/documentation and must not ingest another
emulator's source tree.
