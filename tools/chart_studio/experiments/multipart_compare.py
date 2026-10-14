#!/usr/bin/env python3
"""Evaluate explicit five-lane source assignments without touching songs/originals.

Usage: multipart_compare.py EXE TABS SONGS NEW_OUTPUT_DIRECTORY
Existing alignment evidence is reused for the three installed Studio projects.
Reports contain source hashes and exact argv; no commercial fixtures are copied.
"""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys


CASES = {
    "Foghat - Slow Ride": {"Single": 1, "DoubleRhythm": 2, "DoubleBass": 3},
    "Kalafina - Magia": {"Single": 1, "DoubleRhythm": 2, "DoubleGuitar": 3, "DoubleBass": 4},
    "Kalafina - Magia (r571207)": {"Single": 1, "DoubleRhythm": 2, "DoubleBass": 4},
    "Paramore - Still Into You": {"Single": 3, "DoubleRhythm": 4, "DoubleBass": 5, "Keyboard": 6},
    "DragonForce - Through The Fire And Flames": {"Single": 6, "DoubleGuitar": 7, "DoubleRhythm": 9, "DoubleBass": 10, "Keyboard": 14},
    "Mindflow - Destructive Device": {"Single": 1, "DoubleBass": 2},
    "Mindflow - 01 - Destructive Device": {"Single": 1, "DoubleBass": 2},
    "Mindflow - Honesty": {"Single": 1, "DoubleRhythm": 2, "DoubleGuitar": 3},
    "Mindflow - The Logic Behind Heads Or Tails": {"Single": 1, "Keyboard": 2},
    "Muse - Knights Of Cydonia": {"Single": 4, "DoubleRhythm": 3, "DoubleBass": 5, "Keyboard": 7},
    "Slipknot - Before I Forget": {"Single": 2, "DoubleRhythm": 3, "DoubleBass": 6, "Keyboard": 7},
    "Weezer - My Name Is Jonas": {"Single": 6, "DoubleRhythm": 8, "DoubleBass": 10, "DoubleGuitar": 5},
    "Heart - Barracuda": {"Single": 3, "DoubleRhythm": 4, "DoubleBass": 5, "DoubleGuitar": 2},
}
ALIGNMENTS = {
    "Kalafina - Magia": "Magia",
    "Mindflow - Honesty": "Honesty",
    "Mindflow - The Logic Behind Heads Or Tails": "Mindflow - The Logic Behind Heads Or Tails",
}


def main():
    exe, tabs, songs, output = (Path(p).resolve() for p in sys.argv[1:])
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for name, assignments in CASES.items():
        midi = tabs / (name + ".mid")
        if not midi.is_file():
            results.append({"name": name, "status": "missing MIDI"})
            continue
        chart = output / (name + ".chart")
        report = output / (name + ".json")
        command = [str(exe), "convert", str(midi), "--output", str(chart),
                   "--report", str(report), "--plus", "auto", "--plus-profile", "strategic",
                   "--plus-bars", "2", "--plus-trials", "32"]
        for suffix, track in sorted(assignments.items()):
            command += ["--part", f"Expert{suffix}:{track}"]
        alignment = None
        if name in ALIGNMENTS:
            project = songs / ALIGNMENTS[name] / "notes.gppchartproj"
            if project.is_file():
                with sqlite3.connect(project.as_uri() + "?mode=ro", uri=True) as db:
                    document = json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])
                alignment = document.get("alignment", {}).get("evidence")
                if alignment:
                    artifact = output / (name + ".alignment.json")
                    artifact.write_text(alignment)
                    command += ["--alignment", str(artifact)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        (output / (name + ".log")).write_text(result.stderr + result.stdout)
        row = {"name": name, "argv": command, "input_sha256": hashlib.sha256(midi.read_bytes()).hexdigest(),
               "status": "passed" if result.returncode == 0 else "failed", "exit_code": result.returncode,
               "alignment_reused": bool(alignment), "parts": assignments, "error": result.stderr}
        if result.returncode == 0:
            row["chart_sha256"] = hashlib.sha256(chart.read_bytes()).hexdigest()
            row["report"] = str(report)
            reopened = subprocess.run([str(exe), "inspect", str(chart.with_suffix(".gppchartproj"))],
                                      capture_output=True, text=True, timeout=30)
            row["reopened"] = reopened.returncode == 0
        results.append(row)
        print(name, row["status"], flush=True)
        (output / "summary.json").write_text(json.dumps(results, ensure_ascii=False, indent=2))
    return int(any(r["status"] == "failed" for r in results))


if __name__ == "__main__":
    raise SystemExit(main())
