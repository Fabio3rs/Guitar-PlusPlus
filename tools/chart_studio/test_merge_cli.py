#!/usr/bin/env python3
"""Two synthetic mapped voices, recipe/preview, persistent IDs, real loader."""
import json
from pathlib import Path
import os
from contextlib import closing
import sqlite3
import struct
import subprocess
import sys
import tempfile
from fixture import merge_fixture

exe = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.pop("DISPLAY", None)


def run(*args, ok=True):
    p = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, env=env, timeout=60)
    assert (p.returncode == 0) == ok, p.stdout + p.stderr
    return p

with tempfile.TemporaryDirectory(dir=root) as directory:
    work = Path(directory)
    midi = work / "two.mid"
    merge_fixture(midi)
    original = midi.read_bytes()
    chart = work / "merged.chart"
    project = chart.with_suffix(".gppchartproj")
    report = work / "merge.json"
    args = ['convert', midi, '--part', 'ExpertSingle:0', '--merge-source', 'ExpertSingle:1',
            '--minimum-sustain', 0, '--output', chart, '--merge-report', report]
    run(*args, '--merge-preview')
    assert not chart.exists() and not project.exists()
    preview = json.loads(report.read_text())["merge"][0]["value"]
    assert sum(d['copied'] for d in preview['decisions']) == 3
    run(*args, '--replace')
    chart_bytes = chart.read_bytes()
    with closing(sqlite3.connect(project)) as db, db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 5
        document = json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])
    ids = document["merges"][0]["value"]["ids"]
    run(*args, '--replace')
    assert chart.read_bytes() == chart_bytes
    with closing(sqlite3.connect(project)) as db, db:
        document = json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])
    assert ids == document["merges"][0]["value"]["ids"]
    config = document["merges"][0]["value"]["config"]
    config["regions"] = [{"begin": 480, "end": 3000, "source": 1}, {"begin": 3000, "end": 3840, "source": 2}]
    recipe = work / "recipe.json"
    recipe.write_text(json.dumps({"version": 1, "recipes": [{"key": "[ExpertSingle]", "value": config}]}))
    manual = work / "manual.chart"
    run('convert', midi, '--part', 'ExpertSingle:0', '--merge-recipe', recipe,
        '--minimum-sustain', 0, '--output', manual)
    assert manual.read_bytes() != chart_bytes
    run('compare', manual, manual)
    before = chart.read_bytes(), project.read_bytes(), report.read_bytes()
    run(*args, '--merge-min-silence', '1/0', '--replace', ok=False)
    assert before == (chart.read_bytes(), project.read_bytes(), report.read_bytes())
    run(*args, '--merge-source', 'ExpertSingle:1', '--replace', ok=False)
    assert midi.read_bytes() == original
print("Merge CLI: preview, recipe, persistence, IDs, deterministic export and protection passed")
