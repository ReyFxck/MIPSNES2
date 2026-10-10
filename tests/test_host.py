"""Black-box host scaffold contract; standard library only."""
import subprocess
import sys

binary = sys.argv[1]
result = subprocess.run([binary], capture_output=True, text=True, check=True)
if "foundation only - no SNES emulation implemented." not in result.stdout:
    raise SystemExit("host must identify itself honestly")
for argument in ("example.sfc", "--unknown"):
    result = subprocess.run([binary, argument], capture_output=True, text=True)
    if result.returncode != 2 or "ROM execution is unavailable" not in result.stderr:
        raise SystemExit("host must reject unsupported execution")
print("PASS: host banner and unsupported argument rejection")
