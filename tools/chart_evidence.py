#!/usr/bin/env python3
"""Capture runtime chart dumps before a migration, or compare a frozen capture.

Build gpp_dump_chart with BUILD_CHART_TOOLS=ON. No GL context is required.
Snapshots belong in build/, not in version control with third-party song data.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_dump(executable, case, destination):
    # Legacy logging uses the working directory; isolate it from song assets.
    with tempfile.TemporaryDirectory(prefix="gpp-chart-dump-") as work:
        subprocess.run(
            [str(executable), case["mode"], case["path"],
             case["instrument"], str(destination)],
            cwd=work, check=True, capture_output=True, text=True, timeout=60,
        )


def capture(executable, evidence, inputs):
    paths = set()
    for path in inputs:
        if not path.exists():
            raise RuntimeError(f"Missing input: {path}")
        paths.update(path.rglob("*") if path.is_dir() else [path])
    cases = []
    for path in sorted(paths):
        extension = path.suffix.lower()
        if not path.is_file() or extension not in (".chart", ".gpp"):
            continue
        sections = ["[ExpertSingle]"]
        if extension == ".chart":
            sections = sorted(set(re.findall(
                r"^\s*(\[(?:Easy|Medium|Hard|Expert)[^\]\r\n]+\])",
                path.read_text(errors="replace"), re.MULTILINE,
            ))) or sections
        for section in sections:
            cases.append({
                "path": str(path.resolve()), "instrument": section,
                "mode": "feedback" if extension == ".chart" else "compiled",
                "input_sha256": sha256(path), "dump": f"{len(cases):03d}.txt",
            })
    if not cases:
        raise RuntimeError("No .chart/.gpp inputs found")
    # Never replace an earlier baseline, even when capture is interrupted.
    evidence.mkdir(parents=True, exist_ok=False)
    before = evidence / "before"
    before.mkdir()
    for case in cases:
        destination = before / case["dump"]
        run_dump(executable, case, destination)
        case["dump_sha256"] = sha256(destination)
    manifest = {"executable_sha256": sha256(executable), "cases": cases}
    (evidence / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"Captured {len(cases)} cases in {evidence}")


def compare(executable, evidence):
    cases = json.loads((evidence / "manifest.json").read_text())["cases"]
    after = evidence / "after"
    after.mkdir(exist_ok=True)
    differences = 0
    for case in cases:
        source = Path(case["path"])
        before = evidence / "before" / case["dump"]
        if sha256(source) != case["input_sha256"]:
            raise RuntimeError(f"Input changed since capture: {source}")
        if sha256(before) != case["dump_sha256"]:
            raise RuntimeError(f"Baseline changed since capture: {before}")
        destination = after / case["dump"]
        run_dump(executable, case, destination)
        if destination.read_bytes() != before.read_bytes():
            differences += 1
            print(f"DIFFERENT: {source} {case['instrument']} ({case['dump']})")
    print(f"{len(cases)} cases compared; {differences} differences")
    return differences == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("capture", "compare"))
    parser.add_argument("executable", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("inputs", type=Path, nargs="*")
    args = parser.parse_args()
    executable = args.executable.resolve()
    evidence = args.evidence.resolve()
    if args.action == "capture":
        if not args.inputs:
            parser.error("capture requires input files or directories")
        capture(executable, evidence, args.inputs)
    else:
        if args.inputs:
            parser.error("compare uses the inputs recorded in manifest.json")
        return 0 if compare(executable, evidence) else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
