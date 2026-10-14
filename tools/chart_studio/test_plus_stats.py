#!/usr/bin/env python3
"""Synthetic fixtures only. Optional --dumper exercises the actual runtime loader."""

import argparse
import copy
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest

from plus_analysis import Analysis, Timeline, aggregate, fingerprint, spearman
from plus_report import render
from plus_stats import atomic_write, csv_text, discover, select

DUMPER = None


def part():
    notes = [
        {
            "time": i * 0.25,
            "length": 0.1 if i % 3 else 0.5,
            "lanes": 1 << (i % 5),
            "flags": (1 << (i % 5)) | 32,
        }
        for i in range(64)
    ]
    return {
        "ppq": 480,
        "offset": 0,
        "notes": notes,
        "plus": [
            {"time": 2.0, "length": 2.0, "first": 8, "last": 15},
            {"time": 10.0, "length": 2.0, "first": 40, "last": 47},
        ],
        "bpm": [[0, 120]],
        "tempo_map": [[0, 0, 120]],
        "warnings": [],
        "events": [
            {"kind": "time-signature", "tick": 0, "time": 0, "text": "4/2^2"},
            {"kind": "section", "tick": 1920, "time": 2, "text": "verse"},
        ],
    }


def variable(value):
    result = [value & 127]
    while value > 127:
        value >>= 7
        result.insert(0, (value & 127) | 128)
    return bytes(result)


def midi(path, marker=116):
    # MIDI type 1: conventional gameplay track; marker 103 is also present.
    tempo = (
        b"\x00\xff\x51\x03\x07\xa1\x20\x00\xff\x58\x04\x04\x02\x18\x08\x00\xff\x2f\x00"
    )
    events = [(0, b"\xff\x03\x0bPART GUITAR"), (0, bytes([0x90, marker, 100]))]
    if marker != 103:
        events += [(0, b"\x90\x67\x64"), (240, b"\x80\x67\x00")]
    for tick, pitch in [(0, 96), (120, 97), (240, 98), (480, 99)]:
        events += [
            (tick, bytes([0x90, pitch, 100])),
            (tick + 60, bytes([0x90, pitch, 0])),
        ]
    events += [(360, bytes([0x80, marker, 0]))]
    events.sort(key=lambda e: e[0])
    track, previous = bytearray(), 0
    for tick, event in events:
        track.extend(variable(tick - previous) + event)
        previous = tick
    track.extend(b"\x00\xff\x2f\x00")

    def chunk(data):
        return b"MTrk" + struct.pack(">I", len(data)) + data

    path.write_bytes(
        b"MThd" + struct.pack(">IHHH", 6, 1, 2, 480) + chunk(tempo) + chunk(track)
    )


CHART = """[Song]
{
 Name = "Synthetic"
 Artist = "Test"
 Resolution = 480
 Offset = 0
}
[SyncTrack]
{
 0 = B 120000
 0 = TS 4
 1920 = B 60000
 1920 = TS 3 2
}
[Events]
{
 0 = E "section Intro"
}
[ExpertSingle]
{
 0 = N 0 0
 480 = N 1 240
 960 = N 2 0
 1440 = N 3 960
 1920 = N 4 0
 0 = S 2 1920
}
[EasySingle]
{
 0 = N 0 0
 1920 = N 1 0
}
"""


class Statistics(unittest.TestCase):
    def test_tempo_offset_meter(self):
        p = part()
        p["tempo_map"] = [[0, 3, 120], [1920, 5, 60]]
        p["events"].append(
            {"kind": "time-signature", "tick": 1920, "time": 5, "text": "3/2^2"}
        )
        t = Timeline(p)
        self.assertEqual(t.beat(5), 4)
        self.assertEqual(t.seconds(6), 7)
        self.assertAlmostEqual(t.bars(0, 7), 2)
        self.assertEqual(t.bar_distance(7), 0)
        self.assertAlmostEqual(t.seconds(t.beat(6.75)), 6.75)

    def test_missing_meter_is_not_measured_as_four_four(self):
        p = part()
        p["events"] = []
        t = Timeline(p)
        self.assertIsNone(t.bar_distance(4))
        self.assertIsNone(t.bars(0, 4))
        p["tempo_map"], p["ppq"] = [], 0
        t = Timeline(p)
        self.assertEqual(t.beat(2), 4)
        self.assertTrue(t.warnings)

    def test_solo_is_context_and_not_an_extra_plus(self):
        p = part()
        p["events"].append(
            {"kind": "solo", "tick": 1920, "time": 2, "text": "103 end_tick=2880"}
        )
        a = Analysis(p, (0, 16))
        self.assertEqual(a.phrases[0]["solo_overlap_fraction"], 0.5)
        self.assertEqual(a.phrases[1]["solo_overlap_fraction"], 0)
        self.assertEqual(a.observed()["statistics"]["plus_count"], 2)
        self.assertIn("mean_movement", a.metrics(a.intervals))
        self.assertEqual(a.observed()["statistics"]["peak_attacks_per_second"], 4)

    def test_compound_meter_and_partial_bar_diagnostics(self):
        p = part()
        p["events"] = [
            {"kind": "time-signature", "tick": 0, "time": 0, "text": "6/2^3"},
            {"kind": "time-signature", "tick": 2400, "time": 2.5, "text": "4/2^2"},
        ]
        t = Timeline(p)
        self.assertEqual(t.bars(0, 3), 1)
        self.assertEqual(t.bar_distance(3), 0)
        self.assertEqual(t.bar_distance(5), 0)
        self.assertTrue(any("truncates" in w for w in t.warnings))

    def test_equal_song_weight_in_aggregate(self):
        a = Analysis(part(), (0, 16)).observed()
        b = copy.deepcopy(a)
        b["statistics"]["plus_per_minute"] = 100
        rows = [
            dict(r, selected=True, game="test", instrument="[ExpertSingle]")
            for r in (a, b)
        ]
        group = aggregate(rows)[0]
        self.assertEqual(group["songs"], 2)
        self.assertEqual(group["statistics"]["plus_per_minute"]["median"], 53.75)

    def test_statistics_and_runtime_index_diagnostic(self):
        p = part()
        a = Analysis(p, (0, 16))
        result = a.observed()
        self.assertEqual(result["statistics"]["attacks"], 64)
        self.assertEqual(result["statistics"]["plus_count"], 2)
        self.assertEqual(result["statistics"]["attack_fraction"], 0.25)
        self.assertEqual(result["statistics"]["coverage_fraction"], 0.25)
        self.assertEqual(result["phrases"][0]["attacks"], 8)
        self.assertEqual(result["phrases"][0]["duration_beats"], 4)
        p["plus"][0]["first"] = 999999
        self.assertTrue(any("indices" in w for w in Analysis(p, (0, 16)).warnings))

    def test_overlap_coverage_is_union(self):
        p = part()
        p["plus"][1] = {"time": 3, "length": 2, "first": 12, "last": 19}
        a = Analysis(p, (0, 16))
        self.assertEqual(a.observed()["statistics"]["coverage_fraction"], 3 / 16)
        self.assertEqual(a.random_comparison(10, 42, "a")["completed"], 0)

    def test_zero_plus_and_degenerate_schedule(self):
        p = part()
        p["plus"] = []
        a = Analysis(p, (0, 16))
        self.assertEqual(a.observed()["statistics"]["plus_count"], 0)
        self.assertEqual(a.random_comparison(10, 42, "a")["completed"], 0)
        self.assertIsNone(a.random_schedule(random.Random(1), [1000]))

    def test_zero_span_and_open_transitions(self):
        p = part()
        p["plus"] = []
        p["notes"] = p["notes"][:1]
        p["notes"][0]["length"] = 0
        self.assertIsNone(Analysis(p, (0, 0)).observed()["statistics"]["density"])
        p = part()
        p["notes"] = p["notes"][:4]
        p["plus"] = []
        p["notes"][1]["lanes"] = 131072
        a = Analysis(p, (0, 2))
        window = a.window(0, 3)
        self.assertEqual(window["movement_pairs"], 1)
        self.assertEqual(window["mean_movement"], 1)
        self.assertIsNone(a.phrase(0, 1)["before_density"])

    def test_seed_count_duration_no_overlap(self):
        a = Analysis(part(), (0, 16))
        for seed in range(30):
            schedule = a.random_schedule(random.Random(seed), [2, 4, 6])
            self.assertIsNotNone(schedule)
            self.assertEqual(sorted(b - s for s, b in schedule), [2, 4, 6])
            self.assertTrue(all(b[0] >= x[1] for x, b in zip(schedule, schedule[1:])))
            self.assertTrue(all(s in a.beats and b <= a.end for s, b in schedule))
        self.assertEqual(
            a.random_comparison(20, 42, "a"), a.random_comparison(20, 42, "a")
        )
        first = a.random_comparison(20, 42, "a")
        a.random_comparison(20, 42, "other")
        self.assertEqual(first, a.random_comparison(20, 42, "a"))

    def test_offset_preserves_musical_statistics(self):
        original = part()
        shifted = copy.deepcopy(original)
        for n in shifted["notes"] + shifted["plus"]:
            n["time"] += 3
        shifted["tempo_map"][0][1] += 3
        shifted["bpm"][0][0] += 3
        for event in shifted["events"]:
            event["time"] += 3
        left = Analysis(original, (0, 16)).observed()
        right = Analysis(shifted, (3, 19)).observed()
        self.assertEqual(left["observed"], right["observed"])

    def test_fingerprint_keeps_plus_and_ignores_transient_flags(self):
        p = part()
        h = fingerprint(p)
        p["notes"][0]["flags"] |= 256
        self.assertEqual(h, fingerprint(p))
        p["plus"] = []
        self.assertNotEqual(h, fingerprint(p))

    def test_selection_does_not_prefer_more_plus(self):
        base = dict(
            game="gh3",
            song_id="a",
            instrument="[ExpertSingle]",
            excluded=False,
            preferred=False,
        )
        rows = [
            dict(base, id="a", path="a", priority=0, fingerprint="0"),
            dict(base, id="b", path="b", priority=1, fingerprint="1"),
        ]
        select(rows)
        self.assertTrue(rows[0]["selected"])
        self.assertEqual(rows[1]["selection_reason"], "Alternative chart version")
        rows[1]["preferred"] = True
        select(rows)
        self.assertTrue(rows[1]["selected"])

    def test_correlation_ties_and_undefined(self):
        self.assertEqual(spearman([(1, 2), (2, 4), (3, 6)])["rho"], 1)
        self.assertIsNone(spearman([(1, 2), (1, 4), (1, 6)])["rho"])
        self.assertEqual(spearman([(1, 2), (1, 2), (3, 6)])["rho"], 1)

    def test_safe_outputs_and_metadata(self):
        report = {"title": "</script><script>alert(1)</script>"}
        html = render(report)
        self.assertNotIn(report["title"], html)
        self.assertIn("\\u003c/script", html)
        self.assertIn("'=1", csv_text([{"name": "=1"}]))
        with tempfile.TemporaryDirectory() as work:
            p = Path(work) / "report.json"
            atomic_write(p, "first", False)
            with self.assertRaises(FileExistsError):
                atomic_write(p, "second", False)
            self.assertEqual(p.read_text(), "first")
            atomic_write(p, "second", True)
            self.assertEqual(p.read_text(), "second")
            (Path(work) / "notes.chart").write_text("invalid")
            (Path(work) / "notes.chart.bak").write_text("backup")
            if os.name != "nt":
                (Path(work) / "loop").symlink_to(work, target_is_directory=True)
            paths, errors = discover([Path(work)])
            self.assertEqual(len(paths), 1)
            self.assertFalse(errors)


@unittest.skipUnless(DUMPER, "Pass --dumper for runtime-loader integration")
class Runtime(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def dump(self, path):
        output = self.root / "dump.json"
        result = subprocess.run(
            [DUMPER, "inspect", str(path), str(output)],
            cwd=self.root,
            capture_output=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        return json.loads(output.read_text())

    def test_chart_evidence_and_cpu_loading(self):
        path = self.root / "notes.chart"
        path.write_text(CHART)
        data = self.dump(path)
        p = data["instruments"]["[ExpertSingle]"]
        self.assertEqual(len(p["plus"]), 1)
        self.assertEqual(p["plus"][0]["first"], 0)
        self.assertEqual(p["plus"][0]["last"], 3)
        self.assertEqual(
            [e["text"] for e in p["events"] if e["kind"] == "time-signature"],
            ["4/2^2", "3/2^2"],
        )
        self.assertEqual(
            [e["text"] for e in p["events"] if e["kind"] == "plus"], ["2 end_tick=1920"]
        )
        self.assertAlmostEqual(p["notes"][3]["length"], 1.5)
        self.assertEqual(len(data["instruments"]["[EasySingle]"]["plus"]), 0)

    def test_midi_override_and_solo(self):
        path = self.root / "notes.midi"
        midi(path)
        p = self.dump(path)["instruments"]["[ExpertSingle]"]
        self.assertEqual(len(p["plus"]), 1)
        self.assertEqual(p["plus"][0]["last"], 2)
        self.assertTrue(any(e["kind"] == "solo" for e in p["events"]))
        (self.root / "song.ini").write_text("[song]\nstar_power_note=103\n")
        p = self.dump(path)["instruments"]["[ExpertSingle]"]
        self.assertEqual(p["plus"][0]["last"], 1)
        self.assertFalse(any(e["kind"] == "solo" for e in p["events"]))
        (self.root / "song.ini").unlink()
        midi(path, 103)
        self.assertEqual(
            len(self.dump(path)["instruments"]["[ExpertSingle]"]["plus"]), 1
        )

    def test_cli_recursion_determinism_errors_and_selection(self):
        source = self.root / "músicas"
        source.mkdir()
        (source / "notes.chart").write_text(CHART)
        (source / "alternative.chart").write_text(
            CHART.replace("0 = S 2 1920", "480 = S 2 960")
        )
        (source / "song.ini").write_text("[song]\nicon=gh3\n")

        def run(out, *extra):
            command = [
                sys.executable,
                str(Path(__file__).with_name("plus_stats.py")),
                str(source),
                "--dumper",
                DUMPER,
                "--output-dir",
                str(out),
                "--samples",
                "8",
                *extra,
            ]
            return subprocess.run(
                command, cwd=self.root, capture_output=True, timeout=60
            )

        one, two = self.root / "one", self.root / "two"
        self.assertEqual(run(one).returncode, 0)
        self.assertEqual(run(two).returncode, 0)
        self.assertEqual(
            (one / "report.json").read_bytes(), (two / "report.json").read_bytes()
        )
        data = json.loads((one / "report.json").read_text())
        self.assertEqual(len(data["rows"]), 4)
        self.assertEqual(sum(r["selected"] for r in data["rows"]), 2)
        self.assertTrue(
            all(
                r["path"].endswith("notes.chart") for r in data["rows"] if r["selected"]
            )
        )
        before = (one / "report.json").read_bytes()
        self.assertEqual(run(one).returncode, 2)
        self.assertEqual((one / "report.json").read_bytes(), before)
        (source / "broken.mid").write_bytes(b"not midi")
        self.assertEqual(run(self.root / "partial").returncode, 1)
        partial = json.loads((self.root / "partial" / "report.json").read_text())
        self.assertEqual(len(partial["errors"]), 1)
        self.assertEqual(len(partial["rows"]), 4)

    def test_root_order_and_invalid_catalog(self):
        roots = [self.root / name for name in ("alpha", "beta")]
        for i, directory in enumerate(roots):
            directory.mkdir()
            (directory / "notes.chart").write_text(
                CHART.replace('"Synthetic"', f'"Song {i}"')
            )

        def run(paths, output, extra=()):
            return subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).with_name("plus_stats.py")),
                    *map(str, paths),
                    "--dumper",
                    DUMPER,
                    "--samples",
                    "8",
                    "--output-dir",
                    str(output),
                    *extra,
                ],
                cwd=self.root,
                capture_output=True,
                timeout=60,
            )

        first, second = self.root / "first", self.root / "second"
        self.assertEqual(run(roots, first).returncode, 0)
        self.assertEqual(run(list(reversed(roots)), second).returncode, 0)
        self.assertEqual(
            (first / "report.json").read_bytes(), (second / "report.json").read_bytes()
        )
        catalog = self.root / "catalog.json"
        catalog.write_text(
            json.dumps({"schema": 1, "files": {"notes.chart": {"preferred": "false"}}})
        )
        output = self.root / "invalid"
        self.assertEqual(run(roots, output, ("--catalog", str(catalog))).returncode, 2)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--dumper")
    args, rest = parser.parse_known_args()
    DUMPER = str(Path(args.dumper).resolve()) if args.dumper else None
    # unittest's class decorator is evaluated before CLI parsing.
    Runtime.__unittest_skip__ = not bool(DUMPER)
    unittest.main(argv=[sys.argv[0], *rest])
