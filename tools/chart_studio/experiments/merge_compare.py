#!/usr/bin/env python3
"""Evaluate explicit two-track composition on optional local files; never installs songs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", type=Path, required=True)
parser.add_argument("--tabs", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True, help="New evaluation directory")
parser.add_argument("--break-alignment", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
binary, tabs, out = args.binary.resolve(), args.tabs.resolve(), args.output.resolve()
cases = [
    ("break", "Mindflow - Break Me Out.mid", 2, 1),
    ("ttfaf-acoustic", "DragonForce - Through The Fire And Flames.mid", 6, 4),
    ("magia", "Kalafina - Magia.mid", 1, 2),
    ("honesty", "Mindflow - Honesty.mid", 1, 2),
    ("slow-ride", "Foghat - Slow Ride.mid", 1, 2),
    ("still-into-you", "Paramore - Still Into You.mid", 3, 4),
]
summary = []
def convert(key, midi, primary, secondary, extra=()):
    chart = out / (key + ".chart")
    report = out / (key + ".merge.json")
    cmd = [str(binary), "convert", str(midi), "--part", f"ExpertSingle:{primary}",
           "--merge-source", f"ExpertSingle:{secondary}", "--plus", "auto",
           "--plus-profile", "strategic", "--plus-trials", "8", "--output", str(chart),
           "--report", str(out/(key+".plus.json")), "--merge-report", str(report), *map(str, extra)]
    if key.startswith("break") and args.break_alignment:
        cmd += ["--alignment", str(args.break_alignment.resolve())]
    (out/(key+".command.json")).write_text(json.dumps(cmd, indent=2))
    p = subprocess.run(cmd, text=True, capture_output=True, timeout=180)
    (out/(key+".log")).write_text(p.stderr)
    if p.returncode:
        return {"case": key, "status": "failed", "error": p.stderr}
    original = chart.read_bytes()
    # New process, reopened persistent project/cache, same recipe.
    p = subprocess.run([*cmd, "--replace"], text=True, capture_output=True, timeout=180)
    assert p.returncode == 0, p.stderr
    assert original == chart.read_bytes(), key + ": unstable persistent regeneration"
    data = json.loads(report.read_text())["merge"][0]["value"]
    masks = {}
    lanes = [[] for _ in range(5)]
    for line in chart.read_text().splitlines():
        match = re.match(r"\s*(\d+) = N (\d+) (\d+)", line)
        if match:
            tick, lane, duration = map(int, match.groups())
            masks[tick] = masks.get(tick, 0) | (1 << lane)
            lanes[lane].append((tick, duration))
    for lane in lanes:
        lane.sort()
        assert all(not duration or tick + duration < following
                   for (tick, duration), (following, _) in zip(lane, lane[1:])), key
    sources = [{e["value0"]["value0"]: e for e in data[name]["events"]}
               for name in ("primary", "secondary")]
    kept = [d for d in data["decisions"] if d["copied"]]
    for decision in kept:
        event = sources[decision["source"]][decision["sourceId"]]
        assert masks[decision["tick"]] == event["value0"]["value3"], "Composition remapped lanes"
    assert len(masks) == len(kept)
    return {"case": key, "status": "passed", "primary_track": primary, "secondary_track": secondary,
            "primary_attacks": len(sources[0]), "secondary_attacks": len(sources[1]),
            "copied_primary": sum(d["source"] == 0 for d in kept),
            "copied_secondary": sum(d["source"] == 1 for d in kept),
            "result_attacks": len(masks),
            "transition_tail_cuts": sum(d["duration"] < d["playableDuration"] for d in kept),
            "source_sustain_policy_reductions": sum(d["playableDuration"] < d["originalDuration"] for d in kept),
            "sha256": hashlib.sha256(original).hexdigest(),
            "deterministic_after_reopen": True, "lane_masks_preserved": True,
            "same_lane_collisions": 0}

for key, filename, primary, secondary in cases:
    midi = tabs/filename
    if not midi.exists():
        summary.append({"case": key, "status": "missing"})
        continue
    result = convert(key, midi, primary, secondary)
    summary.append(result)
    if key == "break" and result["status"] == "passed":
        config = json.loads((out/"break.merge.json").read_text())["merge"][0]["value"]["config"]
        config["regions"] = [{"begin": 240000, "end": 276480, "source": 1}]
        recipe = out/"break-manual.recipe.json"
        recipe.write_text(json.dumps({"version": 1, "recipes": [{"key": "[ExpertSingle]", "value": config}]}, indent=2))
        summary.append(convert("break-manual", midi, primary, secondary, ["--merge-recipe", recipe]))
    print(json.dumps(result), flush=True)
(out/"summary.json").write_text(json.dumps(summary, indent=2))
if any(row["status"] == "failed" for row in summary):
    raise SystemExit(1)
