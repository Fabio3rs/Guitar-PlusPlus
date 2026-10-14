#!/usr/bin/env python3
"""Optional musical-context projection. Does not train on chart lanes or references."""

import argparse
import json
import os
from pathlib import Path
import platform

# Set BEFORE importing numpy/numba. Cache stays in the explicitly writable workspace.
for key in (
    "NUMBA_NUM_THREADS",
    "OMP_NUM_THREADS",
    "OPENBLAS_NUM_THREADS",
    "MKL_NUM_THREADS",
):
    os.environ[key] = "1"
os.environ.setdefault(
    "NUMBA_CACHE_DIR", str(Path.cwd() / "build/chart-studio/umap/numba-cache")
)


def features(document):
    """Phrase-relative, transposition-invariant features; never read the output mask."""
    phrases = {}
    for event in document["events"]:
        phrases.setdefault(event["phrase"], []).append(event)
    canonical = {}
    ppq = document["ppq"]
    for phrase in phrases.values():
        signature = phrase[0]["signature"]
        if not signature or any(not e["sources"] for e in phrase):
            raise ValueError("Projection requires unedited musical source phrases")
        if signature in canonical:
            continue
        top = [max(s["pitch"] for s in e["sources"]) for e in phrase]
        base = min(s["pitch"] for e in phrase for s in e["sources"])
        vectors = []
        for i, event in enumerate(phrase):
            pitches = sorted(set(s["pitch"] for s in event["sources"]))
            durations = [s["duration"] / ppq for s in event["sources"]]
            clip = lambda value, bound: max(-bound, min(bound, value)) / bound
            vectors.append(
                [
                    clip(top[i] - base, 36),
                    clip(pitches[0] - base, 36),
                    clip(sum(pitches) / len(pitches) - base, 36),
                    min((pitches[-1] - pitches[0]) / 24, 1),
                    min(len(pitches), 6) / 6,
                    clip(top[i] - top[i - 1], 12) if i else 0,
                    clip(top[i + 1] - top[i], 12) if i + 1 < len(phrase) else 0,
                    min(min(durations), 4) / 4,
                    min(max(durations), 4) / 4,
                    (
                        min((event["tick"] - phrase[i - 1]["tick"]) / ppq, 4) / 4
                        if i
                        else 0
                    ),
                    (
                        min((phrase[i + 1]["tick"] - event["tick"]) / ppq, 4) / 4
                        if i + 1 < len(phrase)
                        else 0
                    ),
                    ((event["tick"] - phrase[0]["tick"]) % ppq) / ppq,
                    *[
                        float(any((p - pitches[0]) % 12 == interval for p in pitches))
                        for interval in range(12)
                    ],
                ]
            )
        canonical[signature] = vectors
    return canonical


def project(document, seed=42, neighbors=15, epochs=150):
    import numpy as np
    import numba
    import sklearn
    import umap

    if not 0 <= seed < 2**31 or neighbors < 2 or epochs < 10:
        raise ValueError("Require seed 0..2^31-1, neighbors >=2 and epochs >=10")
    canonical = features(document)
    unique = sorted({tuple(v) for rows in canonical.values() for v in rows})
    if len(unique) < 3:
        raise ValueError(
            "UMAP experiment needs at least three distinct musical contexts"
        )
    matrix = np.asarray(unique, dtype=np.float64)
    fitted_neighbors = min(neighbors, len(unique) - 1)
    embedding = (
        umap.UMAP(
            n_components=1,
            n_neighbors=fitted_neighbors,
            n_epochs=epochs,
            min_dist=0.1,
            metric="euclidean",
            init="random",
            n_jobs=1,
            random_state=seed,
            transform_seed=seed,
        )
        .fit_transform(matrix)[:, 0]
        .astype(float)
    )
    if not np.isfinite(embedding).all() or np.ptp(embedding) < 1e-10:
        raise ValueError("Degenerate UMAP projection; no silent fallback")
    # Resolve the arbitrary reflection using source-relative pitch, never chart lanes.
    covariance = float(
        np.dot(embedding - embedding.mean(), matrix[:, 0] - matrix[:, 0].mean())
    )
    if covariance < 0 or (covariance == 0 and embedding[0] > embedding[-1]):
        embedding = -embedding
    low, high = np.quantile(embedding, [0.05, 0.95])
    if high - low < 1e-10:
        raise ValueError("Degenerate projection quantiles")
    targets = np.rint(np.clip((embedding - low) / (high - low), 0, 1) * 48).astype(int)
    lookup = {tuple(row): int(target) for row, target in zip(unique, targets)}
    metadata = dict(
        experiment="umap-context-v1",
        seed=seed,
        neighbors=fitted_neighbors,
        epochs=epochs,
        components=1,
        min_dist=0.1,
        init="random",
        threads=1,
        quantization="0..48, nearest even; 5..95 percentiles; source-pitch reflection",
        feature_dimensions=24,
        unique_contexts=len(unique),
        umap=umap.__version__,
        numpy=np.__version__,
        numba=numba.__version__,
        sklearn=sklearn.__version__,
        python=platform.python_version(),
    )
    return dict(
        schema=1,
        **{
            k: document[k]
            for k in ("input_hash", "difficulty", "track", "channel", "program")
        },
        metadata=json.dumps(metadata, sort_keys=True),
        rows=[
            dict(signature=key, positions=[lookup[tuple(v)] for v in canonical[key]])
            for key in sorted(canonical)
        ]
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("features", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--neighbors", type=int, default=15)
    parser.add_argument("--epochs", type=int, default=150)
    args = parser.parse_args()
    result = project(
        json.loads(args.features.read_text()), args.seed, args.neighbors, args.epochs
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as output:
        json.dump(result, output, sort_keys=True, indent=2)
        output.write("\n")


if __name__ == "__main__":
    main()
