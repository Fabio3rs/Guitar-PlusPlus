"""Descriptive source-relative measures, not a composite quality score."""

from collections import defaultdict
from statistics import mean


def center(mask):
    lanes = [i for i in range(5) if mask & (1 << i)]
    return mean(lanes) if lanes else 0


def measure(document):
    events = [e for e in document["events"] if not e["removed"]]
    ppq = document["ppq"]
    pairs = [
        (a, b)
        for a, b in zip(events, events[1:])
        if 0 < b["tick"] - a["tick"] <= 2 * ppq and a["sources"] and b["sources"]
    ]
    contours, reversals, jumps, changes, repeated = [], [], [], [], []
    for a, b in pairs:
        delta = max(s["pitch"] for s in b["sources"]) - max(
            s["pitch"] for s in a["sources"]
        )
        movement = center(b["mask"]) - center(a["mask"])
        if delta:
            contours.append(delta * movement > 0)
            reversals.append(delta * movement < 0)
        jumps.append(abs(movement))
        changes.append((a["mask"] ^ b["mask"]).bit_count())
        if sorted(s["pitch"] for s in a["sources"]) == sorted(
            s["pitch"] for s in b["sources"]
        ):
            repeated.append(a["mask"] == b["mask"])
    occurrences = defaultdict(list)
    for e in document["events"]:
        occurrences[e["phrase"]].append(e)
    groups = defaultdict(list)
    for phrase in occurrences.values():
        groups[phrase[0]["signature"]].append(
            tuple(e["mask"] if not e["removed"] else 0 for e in phrase)
        )
    repeat_groups = [
        patterns
        for signature, patterns in groups.items()
        if signature and len(patterns) > 1
    ]
    spans = []
    for e in events:
        lanes = [i for i in range(5) if e["mask"] & (1 << i)]
        spans.append(max(lanes) - min(lanes))
    ratio = lambda xs: mean(xs) if xs else None
    return dict(
        attacks=len(events),
        contour_pairs=len(contours),
        contour_agreement=ratio(contours),
        contour_reversal=ratio(reversals),
        mean_lane_movement=ratio(jumps),
        large_jump_ratio=ratio([j > 2 for j in jumps]),
        mean_finger_changes=ratio(changes),
        wide_chord_ratio=ratio([s > 2 for s in spans]),
        repeated_pitch_pairs=len(repeated),
        repeated_pitch_consistency=ratio(repeated),
        repeated_phrase_groups=len(repeat_groups),
        repeated_phrase_consistency=ratio([len(set(g)) == 1 for g in repeat_groups]),
    )


def stability(a, b):
    left = {e["id"]: (e["tick"], e["mask"], e["removed"]) for e in a["events"]}
    right = {e["id"]: (e["tick"], e["mask"], e["removed"]) for e in b["events"]}
    keys = left.keys() | right.keys()
    return sum(left.get(k) == right.get(k) for k in keys) / len(keys) if keys else None
