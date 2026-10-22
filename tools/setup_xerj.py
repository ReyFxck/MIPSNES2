"""Fetch and verify the pinned optional XERJ Linux host tool.

Usage: python3 tools/setup_xerj.py INSTALL_ROOT CACHE_ROOT
Supports Linux x86-64 and arm64. Requires curl, GNU tar and Python 3.10+.
The installed binary is a development aid and is never linked into MIPSNES2.
"""
import hashlib
import json
import pathlib
import platform
import subprocess
import sys


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def host_target():
    if sys.platform != "linux":
        raise SystemExit("Pinned MIPSNES2 XERJ setup currently supports Linux only")
    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        return "x86_64-unknown-linux-musl"
    if machine in ("aarch64", "arm64"):
        return "aarch64-unknown-linux-musl"
    raise SystemExit("Unsupported XERJ host architecture: " + machine)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    install_arg, cache_arg = sys.argv[1:]
    manifest = json.loads(
        pathlib.Path(__file__).with_name("xerj.json").read_text()
    )
    target = host_target()
    spec = manifest["assets"][target]
    install = pathlib.Path(install_arg).resolve()
    cache = pathlib.Path(cache_arg).resolve()
    install.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / ("xerj-" + spec["sha256"] + ".tar.gz")
    if not archive.exists():
        partial = archive.with_suffix(".partial")
        partial.unlink(missing_ok=True)
        subprocess.run(
            [
                "curl", "--fail", "--location", "--retry", "3",
                "--connect-timeout", "20", "--max-time", "600",
                spec["url"], "-o", str(partial)
            ],
            check=True,
        )
        if digest(partial) != spec["sha256"]:
            partial.unlink()
            raise SystemExit(
                "Downloaded XERJ SHA-256 mismatch; refusing extraction"
            )
        partial.replace(archive)
        print("Downloaded verified XERJ", manifest["tag"])
    else:
        print("Reusing cached XERJ archive", manifest["tag"])
    if digest(archive) != spec["sha256"]:
        raise SystemExit(
            "Cached XERJ SHA-256 mismatch; remove corrupt archive and retry"
        )
    destination = install / spec["directory"]
    if destination.exists():
        raise SystemExit(
            "Install destination already exists; use a fresh directory"
        )
    subprocess.run(
        [
            "tar", "--no-same-owner", "-xzf", str(archive),
            "-C", str(install)
        ],
        check=True,
    )
    binary = destination / "xerj"
    if not binary.is_file():
        raise SystemExit("Unexpected upstream XERJ archive layout")
    version = subprocess.run(
        [str(binary), "--version"], check=True, text=True,
        stdout=subprocess.PIPE
    ).stdout.strip()
    if version != "xerj v" + manifest["version"]:
        raise SystemExit("Extracted XERJ version does not match the manifest")
    print("Verified and extracted", version, "to", destination)


if __name__ == "__main__":
    main()
