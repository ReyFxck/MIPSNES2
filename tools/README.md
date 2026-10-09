# Foundation tooling

- `check_repository.py`: standard-library consistency guard, invoked by `make test`.
- `setup_toolchain.py`: pinned SHA-256 verified upstream toolchain installation.
- `toolchains.json`: reviewed archive URLs, versions and digests; no floating latest release.

The word latest in upstream asset filenames is part of their name; the release tag
and SHA-256 are pinned. See [GitHub setup](../docs/github-setup.md) for cache, release
and owner/admin instructions. The dependency ecosystem is GitHub Actions only; no
unused package managers are configured in Dependabot.
