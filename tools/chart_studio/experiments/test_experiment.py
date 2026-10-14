#!/usr/bin/env python3
"""Optional real-UMAP integration test; no commercial fixtures or reference charts."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
from copy import deepcopy

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fixture import fixture
from umap_projection import features, project
from metrics import measure, stability

cli = Path(sys.argv[1]).resolve()

# Hand-computed melody: up two lanes, repeat, down one lane.
sample = dict(
    ppq=1,
    events=[
        dict(
            id=i,
            tick=i,
            mask=mask,
            removed=False,
            phrase="a",
            signature="a",
            sources=[dict(pitch=pitch)],
        )
        for i, (pitch, mask) in enumerate([(60, 1), (62, 4), (62, 4), (59, 2)])
    ],
)
measured = measure(sample)
assert measured["contour_pairs"] == 2 and measured["contour_agreement"] == 1
assert measured["mean_lane_movement"] == 1 and measured["mean_finger_changes"] == 4 / 3
assert measured["repeated_pitch_consistency"] == 1
assert measured["repeated_phrase_consistency"] is None
changed = deepcopy(sample)
changed["events"][0]["mask"] = 2
assert stability(sample, changed) == 0.75


def run(*args, ok=True):
    p = subprocess.run(
        [str(cli), *map(str, args)], capture_output=True, text=True, timeout=60
    )
    assert (p.returncode == 0) == ok, p.stdout + p.stderr
    return (json.loads(p.stdout) if p.stdout.strip() else None) if ok else p


with tempfile.TemporaryDirectory(prefix="chart-umap-") as directory:
    root = Path(directory)
    midi = root / "music.mid"
    fixture(midi)
    data = run("features", midi, "--track", 0)
    first = project(data, 42, epochs=30)
    assert first == project(data, 42, epochs=30)
    altered = deepcopy(data)
    for event in altered["events"]:
        event["mask"] = (
            31  # A training target must never affect the features/projection.
        )
        for source in event["sources"]:
            source["pitch"] += 7
    assert features(data) == features(altered)
    assert first == project(altered, 42, epochs=30)
    assert measure(data)["repeated_phrase_consistency"] == 1
    assert stability(data, deepcopy(data)) == 1
    artifact = root / "projection.json"
    artifact.write_text(json.dumps(first))
    chart = root / "umap.chart"
    args = [
        "convert",
        midi,
        "--track",
        0,
        "--algorithm",
        "umap",
        "--projection",
        artifact,
        "--output",
        chart,
    ]
    run(*args)
    before = chart.read_bytes()
    run(*args, "--replace")
    assert before == chart.read_bytes()
    project_path = chart.with_suffix(".gppchartproj")
    original_midi = midi.read_bytes()
    run("events", project_path, "--output", midi, "--replace", ok=False)
    assert midi.read_bytes() == original_midi
    run("patterns", project_path, "confirm", "--event", 1)
    confirmed = run("events", project_path)
    other = project(data, 43, epochs=30)
    artifact.write_text(json.dumps(other))
    run(*args, "--replace")
    assert stability(confirmed, run("events", project_path)) == 1
    run("patterns", project_path, "lock", "--event", 1)
    locked = run("events", project_path)
    artifact.write_text(json.dumps(first))
    run(*args, "--replace")
    assert stability(locked, run("events", project_path)) == 1
    run(
        "convert",
        midi,
        "--track",
        0,
        "--algorithm",
        "umap",
        "--output",
        root / "no-artifact.chart",
        ok=False,
    )
    other["input_hash"] = "wrong input"
    artifact.write_text(json.dumps(other))
    run(*args, "--replace", ok=False)
    assert before == chart.read_bytes()
    other = deepcopy(first)
    other["rows"][0]["positions"][0] = 49
    artifact.write_text(json.dumps(other))
    run(*args, "--replace", ok=False)
    other = deepcopy(first)
    other["rows"].append(deepcopy(other["rows"][0]))
    artifact.write_text(json.dumps(other))
    run(*args, "--replace", ok=False)
    assert not (root / "no-artifact.chart").exists()
    print(
        "UMAP determinism, feature invariance, cache/lock priority and invalid artifact protection passed"
    )
