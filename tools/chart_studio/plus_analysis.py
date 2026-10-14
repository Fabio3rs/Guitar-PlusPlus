"""CPU statistics over gpp_dump_chart evidence; no independent chart parser."""

import bisect
import hashlib
import json
import math
import random
import re
import statistics

VERSION = "plus-statistics-v1"
EPS = 1e-8
LANES = 31 | 131072


def digest(value):
    return hashlib.sha256(
        json.dumps(
            value, sort_keys=True, separators=(",", ":"), allow_nan=False
        ).encode()
    ).hexdigest()


def mean(values):
    values = [v for v in values if v is not None]
    return statistics.fmean(values) if values else None


def quantile(values, p):
    if not values:
        return None
    values = sorted(values)
    x = (len(values) - 1) * p
    lo = int(x)
    return values[lo] + (values[min(lo + 1, len(values) - 1)] - values[lo]) * (x - lo)


def summary(values):
    values = [v for v in values if v is not None]
    return {
        "n": len(values),
        "median": quantile(values, 0.5),
        "p025": quantile(values, 0.025),
        "p975": quantile(values, 0.975),
    }


def spearman(pairs):
    pairs = [(a, b) for a, b in pairs if a is not None and b is not None]
    if len(pairs) < 3:
        return {"n": len(pairs), "rho": None}

    def ranks(values):
        order = sorted(range(len(values)), key=values.__getitem__)
        result = [0.0] * len(values)
        i = 0
        while i < len(order):
            j = i + 1
            while j < len(order) and values[order[j]] == values[order[i]]:
                j += 1
            for k in range(i, j):
                result[order[k]] = (i + j - 1) / 2
            i = j
        return result

    x = ranks([a for a, _ in pairs])
    y = ranks([b for _, b in pairs])
    mx, my = mean(x), mean(y)
    numerator = sum((a - mx) * (b - my) for a, b in zip(x, y))
    denominator = math.sqrt(
        sum((a - mx) ** 2 for a in x) * sum((b - my) ** 2 for b in y)
    )
    return {"n": len(pairs), "rho": numerator / denominator if denominator else None}


class Timeline:
    def __init__(self, part):
        self.warnings = []
        ppq = part.get("ppq", 0)
        tempos = part.get("tempo_map", [])
        if ppq > 0 and tempos:
            points = [(float(sec), tick / ppq, float(bpm)) for tick, sec, bpm in tempos]
        else:
            points = []
            for sec, bpm in part["bpm"]:
                beat = (
                    0.0
                    if not points
                    else points[-1][1] + (sec - points[-1][0]) * points[-1][2] / 60
                )
                points.append((sec, beat, bpm))
            self.warnings.append(
                "Original tick origin unavailable; beat durations reconstructed from BPM."
            )
        if not points or any(not math.isfinite(v) for p in points for v in p):
            raise ValueError("Invalid or missing tempo map")
        unique = {p[0]: p for p in points}
        self.points = sorted(unique.values())
        if any(p[2] <= 0 for p in self.points) or any(
            b[1] < a[1] for a, b in zip(self.points, self.points[1:])
        ):
            raise ValueError("Non-monotonic tempo map")
        self.times = [p[0] for p in self.points]
        self.beats = [p[1] for p in self.points]
        self.signatures = []
        signatures = {}
        for event in part.get("events", []):
            if event["kind"] == "time-signature":
                match = re.fullmatch(r"(\d+)/2\^(\d+)", event["text"])
                if match and int(match[1]) > 0 and int(match[2]) <= 10 and ppq > 0:
                    signatures[event["tick"] / ppq] = (
                        int(match[1]) * 4 / (2 ** int(match[2]))
                    )
                else:
                    self.warnings.append(
                        "Invalid or unavailable time signature: " + event["text"]
                    )
        if not signatures or min(signatures) > 0:
            self.signatures.append((0.0, 4.0, False))
            self.warnings.append(
                "4/4 assumed before first explicit signature; excluded from bar statistics."
            )
        for beat, length in sorted(signatures.items()):
            if self.signatures:
                previous, previous_length, _ = self.signatures[-1]
                if (
                    abs(
                        (beat - previous) / previous_length
                        - round((beat - previous) / previous_length)
                    )
                    > EPS
                ):
                    self.warnings.append(
                        "Time-signature change truncates a bar; a new bar starts at the change."
                    )
            self.signatures.append((beat, length, True))
        self.signature_beats = [s[0] for s in self.signatures]
        self.tolerance = max(1 / ppq if ppq > 0 else 0, 1 / 48)

    def beat(self, seconds):
        point = self.points[max(0, bisect.bisect_right(self.times, seconds) - 1)]
        return point[1] + (seconds - point[0]) * point[2] / 60

    def seconds(self, beat):
        point = self.points[max(0, bisect.bisect_right(self.beats, beat) - 1)]
        return point[0] + (beat - point[1]) * 60 / point[2]

    def bar_distance(self, beat):
        i = max(0, bisect.bisect_right(self.signature_beats, beat + EPS) - 1)
        start, length, explicit = self.signatures[i]
        if not explicit:
            return None
        phase = (beat - start) % length
        distance = min(phase, length - phase)
        if i + 1 < len(self.signatures):
            distance = min(distance, abs(self.signatures[i + 1][0] - beat))
        return distance

    def bars(self, begin, end):
        value = 0.0
        while begin < end - EPS:
            i = max(0, bisect.bisect_right(self.signature_beats, begin + EPS) - 1)
            _, length, explicit = self.signatures[i]
            if not explicit:
                return None
            stop = (
                min(end, self.signatures[i + 1][0])
                if i + 1 < len(self.signatures)
                else end
            )
            value += (stop - begin) / length
            begin = stop
        return value


def fingerprint(part):
    """Playback equivalence, including Plus and meter; excludes transient state."""
    return digest(
        {
            "notes": [
                [
                    round(n["time"], 6),
                    round(n["length"], 6),
                    n["flags"] & (LANES | 32 | 512 | 65536),
                ]
                for n in part["notes"]
            ],
            "plus": [
                [round(p["time"], 6), round(p["length"], 6), p["first"], p["last"]]
                for p in part["plus"]
            ],
            "bpm": [[round(t, 6), round(b, 6)] for t, b in part["bpm"]],
            "meter": [
                (e["tick"], e["text"])
                for e in part.get("events", [])
                if e["kind"] == "time-signature"
            ],
        }
    )


class Analysis:
    def __init__(self, part, horizon):
        self.part = part
        self.clock = Timeline(part)
        self.warnings = list(
            dict.fromkeys(part.get("warnings", []) + self.clock.warnings)
        )
        self.notes = part["notes"]
        if not self.notes or any(
            not math.isfinite(n[k]) for n in self.notes for k in ("time", "length")
        ):
            raise ValueError("Empty or non-finite notes")
        if any(n["length"] < 0 for n in self.notes):
            raise ValueError("Negative sustain")
        self.times = [n["time"] for n in self.notes]
        if self.times != sorted(self.times):
            raise ValueError("Unsorted notes")
        self.beats = [self.clock.beat(t) for t in self.times]
        self.end = self.clock.beat(max(n["time"] + n["length"] for n in self.notes))
        self.horizon = horizon
        self.sections = sorted(
            {
                self.clock.beat(e["time"])
                for e in part.get("events", [])
                if e["kind"] == "section"
            }
        )
        self.section_names = sorted(
            (e["time"], e["text"])
            for e in part.get("events", [])
            if e["kind"] == "section"
        )
        self.solos = []
        solo_start = None
        for event in sorted(part.get("events", []), key=lambda e: e["time"]):
            if event["kind"] == "solo" and part.get("ppq", 0) > 0:
                match = re.search(r"end_tick=(\d+)", event["text"])
                if match:
                    self.solos.append(
                        (event["tick"] / part["ppq"], int(match[1]) / part["ppq"])
                    )
            elif event["kind"] == "text":
                label = event["text"].strip(' []"').lower()
                if label == "solo":
                    solo_start = self.clock.beat(event["time"])
                elif label == "soloend" and solo_start is not None:
                    self.solos.append((solo_start, self.clock.beat(event["time"])))
                    solo_start = None
        if solo_start is not None:
            self.warnings.append("Unclosed solo marker excluded from solo statistics.")
        merged_solos = []
        for begin, end in sorted(self.solos):
            if end <= begin:
                continue
            if merged_solos and begin <= merged_solos[-1][1]:
                merged_solos[-1][1] = max(merged_solos[-1][1], end)
            else:
                merged_solos.append([begin, end])
        self.solos = merged_solos
        self.prefix = {
            k: [0.0]
            for k in (
                "gems",
                "chords",
                "sustains",
                "tail_seconds",
                "hopos",
                "taps",
                "opens",
                "movement",
                "movement_pairs",
                "chord_span",
            )
        }
        previous = None
        for n in self.notes:
            lanes = [i for i in range(5) if n["lanes"] & (1 << i)]
            center = mean(lanes)
            count = (n["lanes"] & LANES).bit_count()
            values = {
                "gems": count,
                "chords": int(count > 1),
                "sustains": int(n["length"] > 0.01),
                "tail_seconds": n["length"],
                "hopos": int(not n["flags"] & (32 | 65536)),
                "taps": int(bool(n["flags"] & 65536)),
                "opens": int(bool(n["lanes"] & 131072)),
                "movement": (
                    abs(center - previous)
                    if center is not None and previous is not None
                    else 0
                ),
                "movement_pairs": int(center is not None and previous is not None),
                "chord_span": max(lanes) - min(lanes) if lanes else 0,
            }
            for key, value in values.items():
                self.prefix[key].append(self.prefix[key][-1] + value)
            previous = center
        self.intervals = []
        self.phrases = []
        for index, phrase in enumerate(part["plus"]):
            start, length = phrase["time"], phrase["length"]
            if not math.isfinite(start) or not math.isfinite(length) or length < 0:
                self.warnings.append(
                    f"Phrase {index}: invalid interval excluded from metrics."
                )
                continue
            begin, end = self.clock.beat(start), self.clock.beat(start + length)
            self.intervals.append((begin, end))
            details = self.phrase(begin, end)
            lo, hi = self.indices(begin, end)
            valid_indices = 0 <= phrase["first"] <= phrase["last"] < len(self.notes)
            runtime_count = (
                phrase["last"] - phrase["first"] + 1 if valid_indices else None
            )
            if not valid_indices or lo != phrase["first"] or hi - 1 != phrase["last"]:
                self.warnings.append(
                    f"Phrase {index}: runtime indices disagree with half-open interval."
                )
            details.update(
                index=index,
                runtime_first=phrase["first"],
                runtime_last=phrase["last"],
                runtime_attacks=runtime_count,
            )
            self.phrases.append(details)
        self.intervals.sort()
        self.phrases.sort(key=lambda p: (p["start_seconds"], p["index"]))
        self.overlaps = sum(
            b[0] < a[1] - EPS for a, b in zip(self.intervals, self.intervals[1:])
        )
        if self.overlaps:
            self.warnings.append(
                "Overlapping Plus phrases; observed coverage uses their union."
            )

    def indices(self, begin, end):
        return bisect.bisect_left(self.beats, begin - EPS), bisect.bisect_left(
            self.beats, end - EPS
        )

    def window(self, begin, end):
        lo, hi = self.indices(begin, end)
        count = hi - lo
        seconds = self.clock.seconds(end) - self.clock.seconds(begin)
        result = {k: values[hi] - values[lo] for k, values in self.prefix.items()}
        # Only transitions wholly inside the window contribute to movement.
        if count:
            for key in ("movement", "movement_pairs"):
                result[key] -= self.prefix[key][lo + 1] - self.prefix[key][lo]
        result.update(
            attacks=count,
            seconds=seconds,
            density=count / seconds if seconds > EPS else 0,
            chord_fraction=result["chords"] / count if count else 0,
            sustain_fraction=result["sustains"] / count if count else 0,
            mean_movement=(
                result["movement"] / result["movement_pairs"]
                if result["movement_pairs"]
                else 0
            ),
        )
        return result

    def phrase(self, begin, end):
        start, stop = self.clock.seconds(begin), self.clock.seconds(end)
        result = self.window(begin, end)
        bar_start, bar_end = self.clock.bar_distance(begin), self.clock.bar_distance(
            end
        )
        section_distance = None
        if self.sections:
            i = bisect.bisect_left(self.sections, begin)
            section_distance = min(
                abs(v - begin) for v in self.sections[max(0, i - 1) : i + 1]
            )
        horizon_start, horizon_end = self.horizon
        duration = horizon_end - horizon_start
        result.update(
            start_seconds=start,
            end_seconds=stop,
            start_beat=begin,
            duration_beats=end - begin,
            duration_bars=self.clock.bars(begin, end),
            position=(start - horizon_start) / duration if duration > EPS else 0,
            end_position=(stop - horizon_start) / duration if duration > EPS else 0,
            beat_start_distance=abs(begin - round(begin)),
            beat_end_distance=abs(end - round(end)),
            bar_start_distance=bar_start,
            bar_end_distance=bar_end,
            bar_start_aligned=(
                None if bar_start is None else int(bar_start <= self.clock.tolerance)
            ),
            bar_end_aligned=(
                None if bar_end is None else int(bar_end <= self.clock.tolerance)
            ),
            section_distance=section_distance,
            section_aligned=(
                None
                if section_distance is None
                else int(section_distance <= self.clock.tolerance)
            ),
            before_density=(
                self.window(max(self.beats[0], begin - 16), begin)["density"]
                if begin > self.beats[0]
                else None
            ),
            after_density=(
                self.window(end, min(self.end, end + 16))["density"]
                if end < self.end
                else None
            ),
        )
        section_index = (
            bisect.bisect_right(self.section_names, (start + EPS, chr(0x10FFFF))) - 1
        )
        result["section"] = (
            self.section_names[section_index][1] if section_index >= 0 else None
        )
        overlap = sum(
            max(0, self.clock.seconds(min(end, b)) - self.clock.seconds(max(begin, a)))
            for a, b in self.solos
            if b > begin and a < end
        )
        result["solo_overlap_fraction"] = (
            overlap / (stop - start) if self.solos and stop > start else None
        )
        return result

    def metrics(self, intervals, phrases=None):
        phrases = (
            phrases
            if phrases is not None
            else [self.phrase(a, b) for a, b in intervals]
        )
        fields = (
            "density",
            "chord_fraction",
            "sustain_fraction",
            "mean_movement",
            "position",
            "bar_start_aligned",
            "bar_end_aligned",
            "section_aligned",
            "before_density",
            "after_density",
            "solo_overlap_fraction",
        )
        result = {
            (key if key.startswith("mean_") else "mean_" + key): mean(
                [p[key] for p in phrases]
            )
            for key in fields
        }
        result["mean_gap_seconds"] = mean(
            [
                self.clock.seconds(b[0]) - self.clock.seconds(a[1])
                for a, b in zip(intervals, intervals[1:])
            ]
        )
        return result

    def observed(self):
        # Include the final zero-duration attack in whole-part statistics.
        whole = self.window(self.beats[0], self.end + 2 * EPS)
        whole["seconds"] = self.clock.seconds(self.end) - self.times[0]
        whole["density"] = (
            len(self.notes) / whole["seconds"] if whole["seconds"] > EPS else None
        )
        whole["peak_attacks_per_second"] = max(
            bisect.bisect_left(self.times, t + 1 - EPS) - i
            for i, t in enumerate(self.times)
        )
        span = self.horizon[1] - self.horizon[0]
        merged = []
        for begin, end in self.intervals:
            if merged and begin <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], end)
            else:
                merged.append([begin, end])
        coverage = sum(
            max(
                0,
                min(self.horizon[1], self.clock.seconds(b))
                - max(self.horizon[0], self.clock.seconds(a)),
            )
            for a, b in merged
        )
        included = sum(self.indices(a, b)[1] - self.indices(a, b)[0] for a, b in merged)
        whole.update(
            plus_count=len(self.part["plus"]),
            valid_plus_count=len(self.phrases),
            plus_per_minute=len(self.part["plus"]) * 60 / span if span > EPS else 0,
            coverage_fraction=coverage / span if span > EPS else 0,
            attack_fraction=included / len(self.notes),
            overlaps=self.overlaps,
            empty_phrases=sum(p["attacks"] == 0 for p in self.phrases),
            duration_seconds=span,
            normalization_start=self.horizon[0],
            normalization_end=self.horizon[1],
            phrase_seconds=summary([p["seconds"] for p in self.phrases]),
            phrase_beats=summary([p["duration_beats"] for p in self.phrases]),
            phrase_bars=summary([p["duration_bars"] for p in self.phrases]),
            start_intervals_seconds=summary(
                [
                    b["start_seconds"] - a["start_seconds"]
                    for a, b in zip(self.phrases, self.phrases[1:])
                ]
            ),
            gaps_seconds=summary(
                [
                    self.clock.seconds(b[0]) - self.clock.seconds(a[1])
                    for a, b in zip(self.intervals, self.intervals[1:])
                ]
            ),
            onset_intervals_seconds=summary(
                [b - a for a, b in zip(self.times, self.times[1:])]
            ),
        )
        whole["bar_duration_matches"] = {
            str(n): sum(
                p["duration_bars"] is not None
                and abs(p["duration_bars"] - n) <= self.clock.tolerance / 4
                for p in self.phrases
            )
            for n in (2, 4, 8)
        }
        whole["bar_duration_matches"]["eligible"] = sum(
            p["duration_bars"] is not None for p in self.phrases
        )
        timeline = []
        if span > EPS:
            for i in range(100):
                begin, end = (
                    self.horizon[0] + span * i / 100,
                    self.horizon[0] + span * (i + 1) / 100,
                )
                timeline.append(
                    self.window(
                        self.clock.beat(begin),
                        self.clock.beat(end + (2 * EPS if i == 99 else 0)),
                    )["density"]
                )
        return {
            "statistics": whole,
            "observed": self.metrics(self.intervals, self.phrases),
            "phrases": self.phrases,
            "density_timeline": timeline,
            "warnings": list(dict.fromkeys(self.warnings)),
        }

    def random_schedule(self, rng, lengths):
        """Attack-conditioned sequential null, not uniform over all schedules."""
        candidates = sorted(set(self.beats))
        limits = {
            length: bisect.bisect_right(candidates, self.end - length + EPS)
            for length in lengths
        }
        if any(not limit for limit in limits.values()):
            return None
        for _ in range(32):
            placed = []
            order = list(lengths)
            rng.shuffle(order)
            for length in order:
                for _ in range(64):
                    begin = candidates[rng.randrange(limits[length])]
                    end = begin + length
                    i = bisect.bisect_left(placed, (begin, end))
                    if (i and placed[i - 1][1] > begin + EPS) or (
                        i < len(placed) and placed[i][0] < end - EPS
                    ):
                        continue
                    placed.insert(i, (begin, end))
                    break
                else:
                    break
            else:
                return placed
        return None

    def random_comparison(self, samples, seed, identity):
        lengths = [b - a for a, b in self.intervals]
        output = {
            "requested": samples,
            "completed": 0,
            "failed": 0,
            "metrics": {},
            "model": "attack-conditioned-sequential-v1",
        }
        if (
            not lengths
            or any(length <= EPS for length in lengths)
            or self.overlaps
            or len(lengths) != len(self.part["plus"])
        ):
            output["reason"] = (
                "No nonempty valid phrases, or overlapping source phrases."
            )
            return output
        rng = random.Random(int(digest([VERSION, seed, identity]), 16))
        observed = self.metrics(self.intervals, self.phrases)
        distributions = {key: [] for key in observed}
        for _ in range(samples):
            schedule = self.random_schedule(rng, lengths)
            if schedule is None:
                output["failed"] += 1
                continue
            output["completed"] += 1
            for key, value in self.metrics(schedule).items():
                if value is not None:
                    distributions[key].append(value)
        for key, values in distributions.items():
            actual = observed[key]
            item = summary(values)
            item["observed"] = actual
            item["percentile"] = (
                (
                    sum(v < actual for v in values)
                    + 0.5 * sum(v == actual for v in values)
                )
                / len(values)
                if values and actual is not None
                else None
            )
            item["difference_from_null_median"] = (
                actual - item["median"]
                if actual is not None and item["median"] is not None
                else None
            )
            output["metrics"][key] = item
        return output


def aggregate(rows):
    groups = {}
    for row in rows:
        if row["selected"] and "statistics" in row:
            groups.setdefault((row["game"], row["instrument"]), []).append(row)
    result = []
    for (game, instrument), members in sorted(groups.items()):
        keys = (
            "plus_count",
            "plus_per_minute",
            "coverage_fraction",
            "attack_fraction",
            "density",
            "chord_fraction",
            "sustain_fraction",
            "peak_attacks_per_second",
        )
        correlations = {}
        for key in (
            "density",
            "sustain_fraction",
            "duration_seconds",
            "peak_attacks_per_second",
        ):
            correlations[key + "_vs_plus_per_minute"] = spearman(
                [
                    (r["statistics"][key], r["statistics"]["plus_per_minute"])
                    for r in members
                ]
            )
        null_effects = {}
        for key in members[0]["observed"]:
            null_effects[key] = summary(
                [
                    r.get("random", {})
                    .get("metrics", {})
                    .get(key, {})
                    .get("difference_from_null_median")
                    for r in members
                ]
            )
        result.append(
            {
                "game": game,
                "instrument": instrument,
                "songs": len(members),
                "zero_plus": sum(r["statistics"]["plus_count"] == 0 for r in members),
                "statistics": {
                    key: summary([r["statistics"][key] for r in members])
                    for key in keys
                },
                "correlations": correlations,
                "null_effects_per_song": null_effects,
            }
        )
    return result
