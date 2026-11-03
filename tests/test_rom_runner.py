"""Black-box bounded host ROM-runner contract; standard library only."""
from pathlib import Path
import subprocess
import sys
import tempfile


def put16(image, offset, value):
    image[offset] = value & 0xFF
    image[offset + 1] = value >> 8


def make_lorom(path):
    image = bytearray(64 * 1024)
    image[0:2] = bytes((0x80, 0xFE))  # BRA -2 at $00:8000
    header = 0x7FC0
    image[header:header + 21] = b"MIPSNES2 ROM RUNNER ".ljust(21, b" ")
    image[header + 0x15] = 0x20
    image[header + 0x16] = 0x00
    image[header + 0x17] = 6
    image[header + 0x18] = 0
    put16(image, header + 0x3C, 0x8000)
    checksum = (sum(image) + 0x1FE) & 0xFFFF
    put16(image, header + 0x1C, checksum ^ 0xFFFF)
    put16(image, header + 0x1E, checksum)
    path.write_bytes(image)


runner = sys.argv[1]
with tempfile.TemporaryDirectory(prefix="mipsnes2-rom-runner-") as temporary:
    rom = Path(temporary) / "fixture.sfc"
    make_lorom(rom)
    result = subprocess.run(
        [runner, "--frames", "2", "--watch-wram", "0x10",
         "--watch-size", "2", "--dump-vram", "0x20", "4", str(rom)],
        capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("runner failed:\n" + result.stdout + result.stderr)
    required = (
        "rom_size=65536 mapper=lorom",
        "frames=2",
        "run_result=0 timing_result=9 cpu_result=1",
        "watch_wram_offset=0x00010 watch_size=2 watch_value=0x0000",
        "vram_offset=0x0020 vram_size=4 vram_hex=00000000",
    )
    for text in required:
        if text not in result.stdout:
            raise SystemExit("missing runner output: " + text + "\n" + result.stdout)

for arguments in ((), ("--frames", "0", "missing.sfc"),
                  ("--watch-size", "3", "missing.sfc")):
    result = subprocess.run([runner, *arguments], capture_output=True, text=True)
    if result.returncode != 2 or "usage:" not in result.stderr:
        raise SystemExit("runner must reject invalid arguments")

print("PASS: bounded host ROM runner")
