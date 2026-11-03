#!/usr/bin/env python3
"""Run one prepared gilyon 65C816 ROM and decode its documented tile status."""
from argparse import ArgumentParser
from pathlib import Path
import re
import subprocess


def read_tests(path):
    descriptions = {}
    current = None
    for line in path.read_text().splitlines():
        match = re.fullmatch(r"Test ([0-9a-fA-F]{4}): (.*)", line)
        if match:
            current = int(match.group(1), 16)
            descriptions[current] = [line]
        elif current is not None and line.startswith("   "):
            descriptions[current].append(line)
    if not descriptions:
        raise SystemExit("test description file contains no tests")
    return descriptions


def field(output, name):
    match = re.search(rf"(?:^| ){re.escape(name)}=(\S+)", output, re.MULTILINE)
    if not match:
        raise SystemExit(f"runner did not report {name}\n{output}")
    return match.group(1)


def decode_tile_text(hex_text):
    data = bytes.fromhex(hex_text)
    return bytes(data[0::2]).split(b"\0", 1)[0].decode("ascii", "replace")


def main():
    parser = ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--tests", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=140)
    arguments = parser.parse_args()
    descriptions = read_tests(arguments.tests)

    result = subprocess.run([
        str(arguments.runner), "--frames", str(arguments.frames),
        "--watch-wram", "0x10", "--watch-size", "2",
        "--dump-vram", "0x64", "32", str(arguments.rom),
    ], capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("ROM runner failed:\n" + result.stdout + result.stderr)
    test_number = int(field(result.stdout, "watch_value"), 0)
    status = decode_tile_text(field(result.stdout, "vram_hex"))
    last_test = max(descriptions)
    print(result.stdout, end="")
    print(f"suite_status={status!r} test={test_number:04x} "
          f"expected_last={last_test:04x}")
    if status == "Success" and test_number == last_test:
        print(f"PASS: gilyon 65C816 suite completed {len(descriptions)} tests")
        return
    if status == "Failed" and test_number in descriptions:
        print("First deterministic failure:")
        print("\n".join(descriptions[test_number]))
        raise SystemExit(1)
    raise SystemExit(
        f"suite did not complete in {arguments.frames} frames "
        f"(status={status!r}, test={test_number:04x})")


if __name__ == "__main__":
    main()
