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
from collections import Counter


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


def aligned(left, right, tolerance):
    """Ordered time merge: an insertion does not shift all subsequent notes."""
    i = j = 0
    pairs, removed, added = [], [], []
    while i < len(left) and j < len(right):
        delta = right[j]["time"] - left[i]["time"]
        if abs(delta) <= tolerance:
            pairs.append((left[i], right[j]))
            i += 1
            j += 1
        elif delta > 0:
            removed.append(left[i])
            i += 1
        else:
            added.append(right[j])
            j += 1
    return pairs, removed + left[i:], added + right[j:]


def statistics(part):
    notes = part["notes"]
    return {
        "attacks": len(notes),
        "gems": sum(n["lanes"].bit_count() for n in notes),
        "chords": part["chords"],
        "sustains": sum(n["length"] > 0 for n in notes),
        "hopos": sum(not n["flags"] & 32 and not n["flags"] & 65536 for n in notes),
        "taps": sum(bool(n["flags"] & 65536) for n in notes),
        "plus": len(part["plus"]),
        "tempos": len(part["tempo_map"]),
        "events": dict(Counter(e["kind"] for e in part["events"])),
        "end_seconds": part["duration"],
    }


def difference(left, right, tolerance):
    pairs, removed, added = aligned(left["notes"], right["notes"], tolerance)
    examples = []
    lane_changes = length_changes = flag_changes = 0
    max_time = max_length = 0.0
    for a, b in pairs:
        time_error = abs(a["time"] - b["time"])
        length_error = abs(a["length"] - b["length"])
        max_time = max(max_time, time_error)
        max_length = max(max_length, length_error)
        lanes = a["lanes"] != b["lanes"]
        length = length_error > tolerance
        flags = (a["flags"] ^ b["flags"]) & (32 | 512 | 65536)
        lane_changes += lanes
        length_changes += length
        flag_changes += bool(flags)
        if (lanes or length or flags) and len(examples) < 6:
            examples.append({"left": a, "right": b})
    phrase_pairs, phrase_removed, phrase_added = aligned(left["plus"], right["plus"], tolerance)
    phrase_changes = sum(abs(a["length"] - b["length"]) > tolerance or
                         a["first"] != b["first"] or a["last"] != b["last"]
                         for a, b in phrase_pairs)
    def sections(part):
        return sorted((e for e in part["events"] if e["kind"] == "section"),
                      key=lambda e: e["time"])
    section_pairs, section_removed, section_added = aligned(sections(left), sections(right), tolerance)
    section_changes = sum(a["text"].strip("[] ") != b["text"].strip("[] ")
                          for a, b in section_pairs)
    def tempos(part):
        return [{"time": e[1], "bpm": e[2]} for e in part["tempo_map"]]
    tempo_pairs, tempo_removed, tempo_added = aligned(tempos(left), tempos(right), tolerance)
    tempo_changes = sum(abs(a["bpm"] - b["bpm"]) > 0.001 for a, b in tempo_pairs)
    # Compare original per-lane tails too: gameplay has only one chord duration.
    raw_tail_changes = raw_left = raw_right = 0
    for lane in {g[2] for p in (left, right) for g in p["gems"]}:
        def gems(part):
            return sorted(({"time": g[3], "length": g[4]} for g in part["gems"] if g[2] == lane),
                          key=lambda g: g["time"])
        gem_pairs, gem_removed, gem_added = aligned(gems(left), gems(right), tolerance)
        raw_tail_changes += sum(abs(a["length"] - b["length"]) > tolerance for a, b in gem_pairs)
        raw_left += len(gem_removed)
        raw_right += len(gem_added)
    return {
        "matched_attacks": len(pairs), "only_left": len(removed), "only_right": len(added),
        "lane_changes": lane_changes, "sustain_changes": length_changes,
        "hopo_tap_sustain_flag_changes": flag_changes,
        "maximum_matched_time_delta": max_time, "maximum_sustain_delta": max_length,
        "plus_only_left": len(phrase_removed), "plus_only_right": len(phrase_added),
        "plus_changes": phrase_changes,
        "sections_only_left": len(section_removed), "sections_only_right": len(section_added),
        "section_changes": section_changes,
        "tempo_only_left": len(tempo_removed), "tempo_only_right": len(tempo_added),
        "tempo_value_changes": tempo_changes,
        "raw_gem_sustain_changes": raw_tail_changes,
        "raw_gems_only_left": raw_left, "raw_gems_only_right": raw_right,
        "end_delta": right["duration"] - left["duration"], "examples": examples,
        "unmatched_left_examples": removed[:3], "unmatched_right_examples": added[:3],
    }


def formats(executable, evidence, inputs, tolerance):
    """Manual survey only; never registered in CTest or dependent on local music."""
    paths = set()
    for path in inputs:
        if not path.exists():
            raise RuntimeError(f"Missing input: {path}")
        paths.update(path.rglob("*") if path.is_dir() else [path])
    paths = sorted(p.resolve() for p in paths if p.is_file() and p.suffix.lower() in (".mid", ".chart", ".gpp"))
    evidence.mkdir(parents=True, exist_ok=False)
    summaries, loaded, failures = [], {}, 0
    lines = [f"Guitar++ manual chart survey (time tolerance {tolerance:g} s)",
             "Differences describe the input files and import policies; they are not automatically bugs.", ""]
    for index, path in enumerate(paths):
        destination = evidence / f"{index:03d}.json"
        digest = sha256(path)
        with tempfile.TemporaryDirectory(prefix="gpp-chart-inspect-") as work:
            try:
                process = subprocess.run([str(executable), "inspect", str(path), str(destination)],
                                         cwd=work, capture_output=True, text=True, timeout=60)
                data = json.loads(destination.read_text()) if destination.exists() else {}
                error = data.get("error") or process.stderr.strip()
                success = process.returncode == 0 and data.get("success", False)
            except (subprocess.TimeoutExpired, ValueError) as exception:
                data, error, success = {}, str(exception), False
        if sha256(path) != digest:
            raise RuntimeError(f"Input changed during survey: {path}")
        parts = data.get("instruments", {})
        entry = {"path": str(path), "sha256": digest, "success": success,
                 "dump": destination.name, "error": error,
                 "instruments": {name: statistics(part) for name, part in parts.items()}}
        summaries.append(entry)
        lines.append(f"{'OK' if success else 'FAILED'} {path}")
        if not success:
            failures += 1
            lines.append(f"  {error}")
            continue
        loaded[path] = data
        if not parts:
            lines.append("  Recognized MIDI, no supported playable instrument")
        for name, part in parts.items():
            lines.append(f"  {name}: {json.dumps(statistics(part), ensure_ascii=False)}")
            lines.append(f"    HOPO: {part['hopo_policy']}, threshold={part['hopo_threshold_ticks']} ticks, PPQ={part['ppq']}")
            for warning in part.get("warnings", []):
                lines.append(f"    WARNING: {warning}")
    comparisons = []
    pairs = []
    if len(paths) == 2 and all(p.is_file() for p in inputs):
        pairs.append(tuple(paths))
    else:
        directories = sorted({p.parent for p in loaded})
        for directory in directories:
            charts = [p for p in loaded if p.parent == directory and p.suffix.lower() == ".chart"]
            midis = [p for p in loaded if p.parent == directory and p.name.lower() == "notes.mid"]
            originals = [p for p in loaded if p.parent == directory and p.name.lower() == "original.mid"]
            pairs.extend((a, b) for a in charts for b in midis)
            pairs.extend((a, b) for a in midis for b in originals)
    for a, b in pairs:
        if a not in loaded or b not in loaded:
            continue
        left, right = loaded[a]["instruments"], loaded[b]["instruments"]
        entry = {"left": str(a), "right": str(b), "instruments": {},
                 "only_left": sorted(left.keys() - right.keys()),
                 "only_right": sorted(right.keys() - left.keys())}
        lines.extend(["", f"COMPARE {a} -> {b}",
                      f"  Instruments only left: {entry['only_left']}; only right: {entry['only_right']}"])
        for instrument in sorted(left.keys() & right.keys()):
            delta = difference(left[instrument], right[instrument], tolerance)
            entry["instruments"][instrument] = delta
            lines.append(f"  {instrument}: {json.dumps(delta, ensure_ascii=False)}")
        comparisons.append(entry)
    summary = {"tolerance_seconds": tolerance, "files": summaries, "comparisons": comparisons,
               "failed_files": failures}
    (evidence / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    (evidence / "report.txt").write_text("\n".join(lines) + "\n")
    print(f"Surveyed {len(paths)} files; {failures} failures; {len(comparisons)} pairs. Report: {evidence / 'report.txt'}")
    return failures == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("capture", "compare", "formats"))
    parser.add_argument("executable", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("inputs", type=Path, nargs="*")
    parser.add_argument("--tolerance", type=float, default=0.001)
    args = parser.parse_args()
    executable = args.executable.resolve()
    evidence = args.evidence.resolve()
    if args.action == "formats":
        if not args.inputs or not 0 <= args.tolerance < float("inf"):
            parser.error("formats requires inputs and a finite nonnegative tolerance")
        return 0 if formats(executable, evidence, args.inputs, args.tolerance) else 1
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
