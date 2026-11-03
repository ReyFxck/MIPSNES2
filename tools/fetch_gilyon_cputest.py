#!/usr/bin/env python3
"""Fetch and prepare the pinned MIT-licensed gilyon 65C816 basic test ROM."""
from argparse import ArgumentParser
import hashlib
import json
from pathlib import Path
import tempfile
from urllib.request import urlopen
from zipfile import ZipFile


REVISION = "5ecdf555da920f0bd7b157542141965a8120186d"
RELEASE = "v1.4"
ARCHIVE_URL = (
    "https://github.com/gilyon/snes-tests/releases/download/v1.4/"
    "snes-tests.zip"
)
ARCHIVE_SHA256 = "c3a39e15a9f3789fe143e54dfa035c988204576eafacf97df8ff3546a917c5e9"
VARIANTS = {
    "basic": {
        "rom_sha256": "d479bde706b9e16d76c0dfe98e2b2d3f5348f9dcb911e6a36b898f4201fc98bb",
        "tests_sha256": "cc5fca504344d771f7b7a5e65c51e4485dbcbaba378edb88df381d6c9819e289",
        "prepared_sha256": "793a4425569649314b200d6bcdc21ea8e69684f9b99ad0a41ba6a4a526d28c3b",
    },
    "full": {
        "rom_sha256": "f61de78d346a68c166d67472a414e5d207368889d37b977d1338f9221ca6400e",
        "tests_sha256": "abbd5b5afdf3a669cf16859bd2e509938e1eff1a1ac4c0deef7217c0ee35f60b",
        "prepared_sha256": "b5d0559118df83c1138550d46154eaa0a810013b4adaaa2ce52374759dca6e98",
    },
}
LICENSE_MEMBER = "LICENSE"
LICENSE_SHA256 = "8d22899b4e22435aefc1865666f889402750a1fffdf8957ce87f03f2d6d40fec"
LOROM_HEADER = 0x7FC0


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require_digest(name, data, expected):
    actual = digest(data)
    if actual != expected:
        raise SystemExit(f"{name} SHA-256 mismatch: {actual}")


def normalize_placeholder_checksum(source, variant):
    image = bytearray(source)
    if len(image) != 256 * 1024:
        raise SystemExit(f"unexpected cputest-{variant} ROM size")
    fields = LOROM_HEADER + 0x1C
    if image[LOROM_HEADER + 0x15] != 0x30:
        raise SystemExit(f"unexpected cputest-{variant} map mode")
    if image[fields:fields + 4] != bytes((0x00, 0x00, 0xFF, 0xFF)):
        raise SystemExit("upstream ROM no longer has the reviewed checksum placeholder")
    checksum = sum(image) & 0xFFFF
    complement = checksum ^ 0xFFFF
    image[fields:fields + 4] = bytes((
        complement & 0xFF, complement >> 8,
        checksum & 0xFF, checksum >> 8,
    ))
    return bytes(image), checksum


def main():
    parser = ArgumentParser()
    parser.add_argument("--variant", choices=sorted(VARIANTS), default="basic")
    parser.add_argument(
        "--output-dir", type=Path,
        default=Path("build/test-roms/gilyon-snes-tests-v1.4"))
    arguments = parser.parse_args()
    variant = arguments.variant
    specification = VARIANTS[variant]
    rom_member = f"cputest/cputest-{variant}.sfc"
    tests_member = f"cputest/tests-{variant}.txt"
    output_dir = arguments.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    with urlopen(ARCHIVE_URL, timeout=60) as response:
        archive = response.read()
    require_digest("release archive", archive, ARCHIVE_SHA256)
    with tempfile.TemporaryDirectory(prefix="mipsnes2-gilyon-") as temporary:
        archive_path = Path(temporary) / "snes-tests.zip"
        archive_path.write_bytes(archive)
        with ZipFile(archive_path) as release:
            rom = release.read(rom_member)
            tests = release.read(tests_member)
            license_text = release.read(LICENSE_MEMBER)

    require_digest("upstream ROM", rom, specification["rom_sha256"])
    require_digest("test descriptions", tests, specification["tests_sha256"])
    require_digest("license", license_text, LICENSE_SHA256)
    prepared_rom, checksum = normalize_placeholder_checksum(rom, variant)
    require_digest("prepared ROM", prepared_rom,
                   specification["prepared_sha256"])

    prepared_path = output_dir / f"cputest-{variant}-checksummed.sfc"
    tests_path = output_dir / f"tests-{variant}.txt"
    license_path = output_dir / "LICENSE.gilyon-snes-tests"
    provenance_path = output_dir / f"PROVENANCE-{variant}.json"
    prepared_path.write_bytes(prepared_rom)
    tests_path.write_bytes(tests)
    license_path.write_bytes(license_text)
    provenance_path.write_text(json.dumps({
        "source": "https://github.com/gilyon/snes-tests",
        "revision": REVISION,
        "release": RELEASE,
        "license": "MIT",
        "archive_url": ARCHIVE_URL,
        "archive_sha256": ARCHIVE_SHA256,
        "variant": variant,
        "upstream_rom_sha256": specification["rom_sha256"],
        "prepared_rom_sha256": specification["prepared_sha256"],
        "normalization": (
            "LoROM checksum/complement placeholder replaced with the calculated "
            "whole-image pair; executable test bytes are unchanged"
        ),
        "checksum": f"0x{checksum:04x}",
    }, indent=2, sort_keys=True) + "\n")
    print(prepared_path)
    print("prepared_sha256=" + digest(prepared_rom))


if __name__ == "__main__":
    main()
