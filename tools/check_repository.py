"""Conservative foundation consistency guards, not emulation verification."""
import hashlib
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOT_DOCS = "README PROJECT_GOALS ARCHITECTURE STATUS ROADMAP PERFORMANCE COMPATIBILITY CONTRIBUTING AGENTS".split()
DOCS = "65c816 bus ppu spc700 dsp apu dma-hdma scheduler cartridge enhancement-chips ps2 psp host optimization testing debugging github-setup bootstrap-audit 65c816-foundation-audit 65c816-addressing-audit 65c816-direct-page-audit 65c816-direct-indirect-audit 65c816-register-transfer-audit 65c816-special-transfer-audit 65c816-branch-audit 65c816-status-control-audit 65c816-increment-decrement-audit 65c816-logic-compare-audit".split()
REQUIRED_DIRS = "core/cpu/65c816 core/bus core/ppu core/apu/spc700 core/apu/dsp core/dma core/scheduler core/cartridge core/enhancement core/debug core/common platform/ps2 platform/psp platform/host tests tools".split()
errors = []
required = [name + ".md" for name in ROOT_DOCS]
required += ["docs/" + name + ".md" for name in DOCS]
required += [".github/workflows/" + name + ".yml" for name in ("ci", "build-ps2", "build-psp", "prerelease")]
required += [".github/ISSUE_TEMPLATE/" + name + ".yml" for name in ("bug", "compatibility", "performance", "config")]
required += [".github/pull_request_template.md", ".github/dependabot.yml", "LICENSE"]
for relative in required:
    if not (ROOT / relative).is_file():
        errors.append("missing file: " + relative)
for relative in REQUIRED_DIRS:
    path = ROOT / relative
    if not path.is_dir() or not any(path.iterdir()):
        errors.append("missing/empty directory: " + relative)
license_path = ROOT / "LICENSE"
if license_path.exists() and hashlib.sha256(license_path.read_bytes()).hexdigest() != "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986":
    errors.append("original GPL-3.0 LICENSE changed")
for name in ("README.md", "AGENTS.md"):
    content = (ROOT / name).read_text()
    for phrase in ("DO NOT TURN THIS PROJECT INTO A SNES9X PORT.", "Built for the hardware, not ported to it."):
        if phrase not in content:
            errors.append(name + " missing identity: " + phrase)
if not (ROOT / "AGENTS.md").read_text().startswith("# STOP — READ BEFORE MODIFYING MIPSNES2\n"):
    errors.append("AGENTS.md must start with required warning")
markdown = [ROOT / p for p in required if p.endswith(".md")]
markdown += list((ROOT / "core").rglob("*.md")) + list((ROOT / "tests").glob("*.md"))
for path in markdown:
    if not path.exists():
        continue
    content = path.read_text()
    for match in re.finditer(r"\[[^\]\n]+\]\(([^)]+)\)", content):
        link = match.group(1).split("#", 1)[0]
        if not link or "://" in link or link.startswith("mailto:"):
            continue
        if not (path.parent / link).exists():
            errors.append(str(path.relative_to(ROOT)) + ": broken link " + link)
    for line in content.splitlines():
        if re.match(r"^\s*- \[[^]]*\](?!\()", line):
            if not re.match(r"^- \[( |x)\] ", line):
                errors.append(str(path.relative_to(ROOT)) + ": invalid checkbox " + line)
            if "[BLOCKED]" in line and " — " not in line:
                errors.append("blocked entry needs reason: " + line)
for path in (ROOT / "core").rglob("*"):
    if path.suffix not in (".c", ".h", ".S"):
        continue
    content = path.read_text()
    for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', content, re.M):
        if re.search(r"psp|ps2|tamtypes|kernel\.h|sifrpc|libpad|gsKit|SDL|platform/", header, re.I):
            errors.append(str(path.relative_to(ROOT)) + ": platform dependency " + header)
    if re.search(r"#\s*(ifdef|if defined).*\b(PS2|PSP)\b", content):
        errors.append(str(path.relative_to(ROOT)) + ": platform conditional in core")
for path in (ROOT / ".github/workflows").glob("*.yml"):
    content = path.read_text()
    if "continue-on-error: true" in content or "pull_request_target:" in content:
        errors.append(path.name + ": unsafe failure/event policy")
    for action in re.findall(r"uses:\s+([^\s]+)", content):
        if not action.startswith("./") and not re.fullmatch(r"[\w-]+/[\w-]+@[0-9a-f]{40}", action):
            errors.append(path.name + ": unpinned action " + action)
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print("PASS: required files, license, identity, links, checkboxes, core boundaries and action pins")
