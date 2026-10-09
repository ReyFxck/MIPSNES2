"""Fetch a pinned upstream Linux x86-64 SDK archive; verify before extracting.

Usage: python3 tools/setup_toolchain.py ps2|psp INSTALL_ROOT CACHE_ROOT
Requires curl, GNU tar and Python 3.10+. Cache archives, not installed executables.
"""
import hashlib
import json
import pathlib
import subprocess
import sys


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("ps2", "psp"):
        raise SystemExit(__doc__)
    target, install_arg, cache_arg = sys.argv[1:]
    spec = json.loads(pathlib.Path(__file__).with_name("toolchains.json").read_text())[target]
    install = pathlib.Path(install_arg).resolve()
    cache = pathlib.Path(cache_arg).resolve()
    install.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / (target + "-" + spec["sha256"] + ".tar.gz")
    if not archive.exists():
        partial = archive.with_suffix(".partial")
        subprocess.run(["curl", "--fail", "--location", "--retry", "3",
                        "--connect-timeout", "20", "--max-time", "600",
                        spec["url"], "-o", str(partial)], check=True)
        if digest(partial) != spec["sha256"]:
            partial.unlink()
            raise SystemExit("Downloaded toolchain SHA-256 mismatch; refusing extraction")
        partial.replace(archive)
        print("Downloaded verified archive", spec["version"])
    else:
        print("Reusing cached archive", spec["version"])
    if digest(archive) != spec["sha256"]:
        raise SystemExit("Cached toolchain SHA-256 mismatch; remove corrupt archive and retry")
    destination = install / spec["directory"]
    if destination.exists():
        raise SystemExit("Install destination already exists; use a fresh directory to avoid mixing SDK versions")
    subprocess.run(["tar", "--no-same-owner", "-xzf", str(archive), "-C", str(install)], check=True)
    if not destination.is_dir():
        raise SystemExit("Unexpected upstream archive layout")
    print("Verified and extracted", target, spec["version"], "to", destination)


if __name__ == "__main__":
    main()
