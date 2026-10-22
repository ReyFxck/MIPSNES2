"""Conservative foundation consistency guards, not emulation verification."""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOT_DOCS = "README PROJECT_GOALS ARCHITECTURE STATUS ROADMAP PERFORMANCE COMPATIBILITY CONTRIBUTING AGENTS".split()
DOCS = "65c816 bus ppu spc700 dsp apu dma-hdma scheduler cartridge enhancement-chips ps2 psp host optimization testing debugging github-setup bootstrap-audit 65c816-foundation-audit 65c816-addressing-audit 65c816-direct-page-audit 65c816-direct-indirect-audit 65c816-register-transfer-audit 65c816-special-transfer-audit 65c816-branch-audit 65c816-status-control-audit 65c816-increment-decrement-audit 65c816-logic-compare-audit scheduler-foundation-audit snes-bus-timing-audit cpu-io-foundation-audit hv-timing-foundation-audit hv-irq-foundation-audit master-timing-fanout-audit interrupt-handoff-audit 5a22-math-register-audit auto-joypad-audit manual-joypad-audit 5a22-io-port-audit cartridge-foundation-audit cartridge-header-analysis-audit cartridge-mapper-selection-audit cartridge-irregular-mapping-audit cartridge-exhirom-audit cartridge-construction-audit cartridge-sram-persistence-audit".split()
DOCS += ["hv-counter-latch-audit"]
DOCS += ["65c816-interrupt-cycle-audit", "65c816-subroutine-cycle-audit",
         "65c816-long-subroutine-cycle-audit",
         "65c816-indexed-subroutine-cycle-audit", "65c816-jump-cycle-audit",
         "65c816-resumable-branch-cycle-audit",
         "xerj-workflow"]
REQUIRED_DIRS = "core/cpu/65c816 core/bus core/ppu core/apu/spc700 core/apu/dsp core/dma core/scheduler core/cartridge core/enhancement core/debug core/common platform/ps2 platform/psp platform/host tests tools".split()
errors = []
required = [name + ".md" for name in ROOT_DOCS]
required += ["docs/" + name + ".md" for name in DOCS]
required += [".github/workflows/" + name + ".yml" for name in ("ci", "build-ps2", "build-psp", "prerelease")]
required += [".github/ISSUE_TEMPLATE/" + name + ".yml" for name in ("bug", "compatibility", "performance", "config")]
required += [".github/pull_request_template.md", ".github/dependabot.yml", "LICENSE"]
required += [".xerjignore", "tools/setup_xerj.py", "tools/xerj.json"]
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
xerj_manifest_path = ROOT / "tools/xerj.json"
if xerj_manifest_path.is_file():
    try:
        xerj_manifest = json.loads(xerj_manifest_path.read_text())
    except (json.JSONDecodeError, OSError) as error:
        errors.append("tools/xerj.json is invalid: " + str(error))
    else:
        if not isinstance(xerj_manifest, dict):
            errors.append("tools/xerj.json root must be an object")
        else:
            version_value = xerj_manifest.get("version", "")
            tag_value = xerj_manifest.get("tag", "")
            version = version_value if isinstance(version_value, str) else ""
            tag = tag_value if isinstance(tag_value, str) else ""
            if not version or tag != "v" + version:
                errors.append("tools/xerj.json tag must exactly pin v<version>")
            commit = xerj_manifest.get("commit", "")
            if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
                errors.append("tools/xerj.json commit must be a full lowercase SHA")
            if xerj_manifest.get("license") != "Apache-2.0":
                errors.append("tools/xerj.json must record the reviewed Apache-2.0 license")
            if xerj_manifest.get("source_url") != "https://github.com/xerj-org/xerj":
                errors.append("tools/xerj.json must use the reviewed upstream repository")
            expected_targets = {
                "x86_64-unknown-linux-musl", "aarch64-unknown-linux-musl"
            }
            assets = xerj_manifest.get("assets", {})
            if not isinstance(assets, dict):
                errors.append("tools/xerj.json assets must be an object")
                assets = {}
            if set(assets) != expected_targets:
                errors.append("tools/xerj.json must pin both reviewed Linux musl assets")
            for target, spec in assets.items():
                sha256_value = spec.get("sha256", "") if isinstance(spec, dict) else ""
                url_value = spec.get("url", "") if isinstance(spec, dict) else ""
                directory_value = spec.get("directory", "") if isinstance(spec, dict) else ""
                sha256 = sha256_value if isinstance(sha256_value, str) else ""
                url = url_value if isinstance(url_value, str) else ""
                directory = directory_value if isinstance(directory_value, str) else ""
                if not re.fullmatch(r"[0-9a-f]{64}", sha256):
                    errors.append("tools/xerj.json invalid SHA-256 for " + target)
                release_path = "/releases/download/" + tag + "/"
                if release_path not in url or "/latest" in url:
                    errors.append("tools/xerj.json unpinned asset URL for " + target)
                if directory != "xerj-" + version + "-" + target:
                    errors.append("tools/xerj.json unexpected directory for " + target)
xerj_ignore_path = ROOT / ".xerjignore"
if xerj_ignore_path.is_file():
    xerj_ignored = set(xerj_ignore_path.read_text().splitlines())
    for ignored in ("/ref/", "/reference/", "/references/"):
        if ignored not in xerj_ignored:
            errors.append(".xerjignore must exclude " + ignored)
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
