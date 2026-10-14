#!/usr/bin/env python3
"""Cold beam vs UMAP on identical MIDI sources. Never reads professional charts."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from metrics import measure, stability


def run(cli, *args):
    result = subprocess.run(
        [str(cli), *map(str, args)], capture_output=True, text=True, timeout=300
    )
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    return json.loads(result.stdout)


def write(path, data):
    with path.open("x") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def projection(features, output, seed, epochs):
    subprocess.run(
        [
            sys.executable,
            str(Path(__file__).with_name("umap_projection.py")),
            str(features),
            "--output",
            str(output),
            "--seed",
            str(seed),
            "--epochs",
            str(epochs),
        ],
        check=True,
        timeout=300,
    )


def evaluate(cli, source, track, directory, seeds, epochs):
    directory.mkdir()
    feature = run(cli, "features", source, "--track", track)
    feature_file = directory / "features.json"
    write(feature_file, feature)
    baseline_chart = directory / "beam.chart"
    baseline_report = run(
        cli, "convert", source, "--track", track, "--output", baseline_chart
    )
    baseline = run(cli, "events", baseline_chart.with_suffix(".gppchartproj"))
    baseline_repeat = directory / "beam-cold-repeat.chart"
    run(cli, "convert", source, "--track", track, "--output", baseline_repeat)
    assert baseline_chart.read_bytes() == baseline_repeat.read_bytes()
    result = dict(
        midi=str(source),
        sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        track=track,
        beam=measure(baseline),
        beam_deterministic=True,
        umap=[],
        seed_pair_stability=[],
    )
    documents = []
    for seed in seeds:
        artifact = directory / f"projection-{seed}.json"
        projection(feature_file, artifact, seed, epochs)
        chart = directory / f"umap-{seed}.chart"
        args = [
            "convert",
            source,
            "--track",
            track,
            "--algorithm",
            "umap",
            "--projection",
            artifact,
        ]
        report = run(cli, *args, "--output", chart)
        doc = run(cli, "events", chart.with_suffix(".gppchartproj"))
        write(directory / f"umap-{seed}.events.json", doc)
        before = chart.read_bytes()
        # Cold replay in a distinct project and process, then persistent cache replay.
        replay = directory / f"umap-{seed}-cold-repeat.chart"
        run(cli, *args, "--output", replay)
        assert replay.read_bytes() == before
        warm = run(cli, *args, "--output", chart, "--replace")
        assert chart.read_bytes() == before and warm["reuse_ratio"] == 1
        row = dict(
            seed=seed,
            metrics=measure(doc),
            versus_beam=stability(baseline, doc),
            cold_replay_identical=True,
            warm_replay_identical=True,
            cache_reuse=warm["reuse_ratio"],
            artifact_sha256=hashlib.sha256(artifact.read_bytes()).hexdigest(),
        )
        if seed == seeds[0]:
            repeated = directory / f"projection-{seed}-repeat.json"
            projection(feature_file, repeated, seed, epochs)
            row["projection_recomputed_identical"] = (
                repeated.read_bytes() == artifact.read_bytes()
            )
            assert row["projection_recomputed_identical"]
        documents.append(doc)
        result["umap"].append(row)
    for i in range(len(seeds)):
        for j in range(i + 1, len(seeds)):
            result["seed_pair_stability"].append(
                dict(
                    seeds=[seeds[i], seeds[j]],
                    agreement=stability(documents[i], documents[j]),
                )
            )
    write(directory / "comparison.json", result)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cli", type=Path, required=True)
    p.add_argument(
        "--manifest",
        type=Path,
        required=True,
        help="JSON array of {midi,track}; explicit source selection",
    )
    p.add_argument(
        "--output",
        type=Path,
        required=True,
        help="New directory; existing experiments are never overwritten",
    )
    p.add_argument("--seeds", type=int, nargs="+", default=[42, 43, 44])
    p.add_argument("--epochs", type=int, default=150)
    args = p.parse_args()
    if len(set(args.seeds)) != len(args.seeds):
        p.error("Seeds must be distinct")
    args.output.mkdir(parents=True, exist_ok=False)
    results = []
    for i, source in enumerate(json.loads(args.manifest.read_text())):
        midi = Path(source["midi"]).resolve()
        print(f"Evaluating {midi.name}", flush=True)
        result = evaluate(
            args.cli.resolve(),
            midi,
            int(source["track"]),
            args.output / f"{i:02d}",
            args.seeds,
            args.epochs,
        )
        results.append(result)
        print(
            json.dumps(
                dict(
                    song=midi.name,
                    beam=result["beam"],
                    umap=result["umap"],
                    stability=result["seed_pair_stability"],
                )
            ),
            flush=True,
        )
    write(args.output / "summary.json", results)


if __name__ == "__main__":
    main()
