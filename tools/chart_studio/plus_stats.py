#!/usr/bin/env python3
"""Recursively inspect gameplay charts with Guitar++ and report Plus statistics."""

import argparse
import configparser
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unicodedata

from plus_analysis import Analysis, VERSION, aggregate, digest, fingerprint
from plus_report import render

EXTENSIONS = {".chart", ".gpp", ".mid", ".midi"}


def normalize(value):
    return " ".join(unicodedata.normalize("NFKC", str(value)).casefold().split())


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ini_metadata(directory):
    paths = sorted(
        (
            p
            for p in directory.iterdir()
            if p.name.lower() == "song.ini" and p.is_file()
        ),
        key=str,
    )
    if not paths:
        return {}, None, []
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read_string(paths[0].read_text(encoding="utf-8-sig", errors="replace"))
        section = next(
            (parser[s] for s in parser.sections() if s.lower() == "song"), {}
        )
        return (
            {k: v.strip().strip('"') for k, v in section.items()},
            sha256(paths[0]),
            [],
        )
    except (OSError, configparser.Error) as error:
        return {}, sha256(paths[0]), ["Invalid song.ini: " + str(error)]


def discover(roots):
    files, errors = set(), []
    for root in roots:
        if root.is_file():
            if root.suffix.lower() in EXTENSIONS:
                files.add(root.resolve())
        elif root.is_dir():

            def onerror(error):
                errors.append({"path": str(error.filename), "error": str(error)})

            for directory, subdirs, names in os.walk(
                root, followlinks=False, onerror=onerror
            ):
                subdirs[:] = sorted(
                    d for d in subdirs if not (Path(directory) / d).is_symlink()
                )
                for name in sorted(names):
                    path = Path(directory) / name
                    if path.suffix.lower() in EXTENSIONS and path.is_file():
                        files.add(path.resolve())
        else:
            errors.append({"path": str(root), "error": "Input root does not exist"})
    return sorted(files, key=lambda p: p.as_posix()), errors


def relative(path, roots):
    for root in sorted(roots, key=lambda p: (-len(p.parts), p.as_posix())):
        try:
            return path.relative_to(root if root.is_dir() else root.parent).as_posix()
        except ValueError:
            pass
    return path.as_posix()


def priority(path):
    # SongFiles::charts order, including its case-insensitive matching.
    base = path.parent.name.lower()
    names = [
        base + ".gpp",
        base + ".chart",
        "chart.gpp",
        "notes.gpp",
        "chart.chart",
        "notes.chart",
        "notes.mid",
    ]
    return names.index(path.name.lower()) if path.name.lower() in names else len(names)


def select(rows):
    groups = {}
    for row in rows:
        row["selected"] = False
        if row["excluded"]:
            row["selection_reason"] = "Excluded by catalog"
        else:
            groups.setdefault(
                (row["game"], row["song_id"], row["instrument"]), []
            ).append(row)
    for members in groups.values():
        members.sort(key=lambda r: (not r["preferred"], r["priority"], r["path"]))
        winner = members[0]
        winner["selected"] = True
        winner["selection_reason"] = (
            "Catalog preference"
            if winner["preferred"]
            else "Runtime filename priority, then stable path"
        )
        for row in members[1:]:
            row["representative"] = winner["id"]
            row["selection_reason"] = (
                "Equivalent copy"
                if row["fingerprint"] == winner["fingerprint"]
                else "Alternative chart version"
            )
        if sum(r["preferred"] for r in members) > 1:
            winner.setdefault("warnings", []).append(
                "Multiple preferred files; filename priority and stable path broke the tie."
            )


def atomic_write(path, content, replace):
    if path.exists() and not replace:
        raise FileExistsError(f"Refusing to replace {path}; use --replace")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=path.parent, prefix=".plus-stats-", delete=False
        ) as stream:
            temporary = Path(stream.name)
            stream.write(content.encode("utf-8"))
            stream.flush()
            os.fsync(stream.fileno())
        if replace:
            os.replace(temporary, path)
        else:
            # Exclusive creation, including a concurrent writer after preflight.
            os.link(temporary, path)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


def csv_text(rows):
    output = io.StringIO(newline="")
    keys = list(dict.fromkeys(key for row in rows for key in row)) or ["id"]
    writer = csv.DictWriter(output, fieldnames=keys)
    writer.writeheader()
    for row in rows:
        # Spreadsheet formula injection is irrelevant to JSON but not CSV.
        safe = {
            k: (
                "'" + v
                if isinstance(v, str) and v.lstrip().startswith(("=", "+", "-", "@"))
                else v
            )
            for k, v in row.items()
        }
        writer.writerow(safe)
    return output.getvalue()


def run(args):
    roots = sorted({Path(p).resolve() for p in args.roots}, key=str)
    dumper = Path(args.dumper).resolve()
    if not dumper.is_file():
        raise ValueError("Build gpp_dump_chart with BUILD_CHART_TOOLS=ON first")
    if not 0 <= args.samples <= 100000 or args.timeout <= 0:
        raise ValueError("Samples must be 0..100000 and timeout must be positive")
    output = Path(args.output_dir).resolve()
    names = ("report.json", "report.html", "charts.csv", "phrases.csv", "catalog.json")
    if not args.replace and any((output / name).exists() for name in names):
        raise FileExistsError(
            "Report already exists; choose another output directory or use --replace"
        )
    catalog = (
        json.loads(Path(args.catalog).read_text(encoding="utf-8"))
        if args.catalog
        else {"schema": 1, "files": {}}
    )
    if (
        not isinstance(catalog, dict)
        or catalog.get("schema") != 1
        or not isinstance(catalog.get("files"), dict)
    ):
        raise ValueError("Unsupported catalog schema")
    for path, entry in catalog["files"].items():
        if not isinstance(entry, dict):
            raise ValueError(f"Catalog entry must be an object: {path}")
        for key in ("preferred", "exclude"):
            if key in entry and not isinstance(entry[key], bool):
                raise ValueError(f"Catalog {key} must be a boolean: {path}")
        for key in ("game", "song_id"):
            if key in entry and (
                not isinstance(entry[key], str) or not entry[key].strip()
            ):
                raise ValueError(f"Catalog {key} must be a nonempty string: {path}")
    paths, errors = discover(roots)
    inputs = set(paths)
    if args.catalog:
        inputs.add(Path(args.catalog).resolve())
    if any((output / name).resolve() in inputs for name in names):
        raise ValueError("An output would overwrite an input")
    rows, analyzers, inventory = [], {}, []
    with tempfile.TemporaryDirectory(prefix="gpp-plus-stats-") as work:
        dump_path = Path(work) / "inspection.json"
        for number, path in enumerate(paths):
            label = relative(path, roots)
            print(
                f"Inspect {number + 1}/{len(paths)}: {label}",
                file=sys.stderr,
                flush=True,
            )
            item = {"path": path.as_posix(), "relative_path": label}
            inventory.append(item)
            file_rows = []
            file_analyzers = {}
            try:
                config = catalog["files"].get(
                    path.as_posix(), catalog["files"].get(label, {})
                )
                if not isinstance(config, dict):
                    raise ValueError("Catalog file entry must be an object")
                ini, ini_hash, ini_warnings = ini_metadata(path.parent)
                before = sha256(path)
                if dump_path.exists():
                    dump_path.unlink()
                process = subprocess.run(
                    [str(dumper), "inspect", str(path), str(dump_path)],
                    cwd=work,
                    capture_output=True,
                    text=True,
                    errors="replace",
                    timeout=args.timeout,
                )
                if not dump_path.exists():
                    raise ValueError(
                        f"Loader exit {process.returncode}: {process.stderr[-1000:]}"
                    )
                data = json.loads(dump_path.read_text(encoding="utf-8"))
                if process.returncode or not data.get("success"):
                    raise ValueError(
                        data.get("error") or f"Loader exit {process.returncode}"
                    )
                if before != sha256(path) or ini_hash != ini_metadata(path.parent)[1]:
                    raise ValueError("Input changed while inspecting")
                fields = {k.lower(): v for k, v in data.get("fields", {}).items()}

                def metadata(key):
                    value = data.get(key) or fields.get(key, "")
                    if normalize(value).startswith("unknown "):
                        value = ""
                    return value or ini.get(key, "")

                title = metadata("name") or path.parent.name
                artist = metadata("artist")
                # INI provides the human title when a converted chart only has its internal slug.
                if ini.get("name") and normalize(title).replace(" ", "") == normalize(
                    ini["name"]
                ).replace(" ", ""):
                    title = ini["name"]
                game = config.get(
                    "game", fields.get("icon") or ini.get("icon") or "unknown"
                )
                game = normalize(game)
                if game in ("", "0"):
                    game = "unknown"
                song_id = (
                    config.get("song_id") or normalize(artist + " / " + title)
                    if artist
                    else config.get("song_id") or "directory:" + path.parent.as_posix()
                )
                item.update(
                    input_sha256=before,
                    ini_sha256=ini_hash,
                    game=game,
                    title=title,
                    artist=artist,
                    song_id=song_id,
                    preferred=bool(config.get("preferred", False)),
                    excluded=bool(config.get("exclude", False)),
                )
                parts = data["instruments"]
                all_notes = [n for part in parts.values() for n in part["notes"]]
                horizon = (
                    min(n["time"] for n in all_notes),
                    max(n["time"] + n["length"] for n in all_notes),
                )
                for instrument, part in sorted(parts.items()):
                    row = dict(
                        item,
                        instrument=instrument,
                        charter=re.sub(r"<[^>]*>", "", metadata("charter")),
                        priority=priority(path),
                        fingerprint=fingerprint(part),
                    )
                    row["id"] = digest([path.as_posix(), instrument])[:24]
                    analysis = Analysis(part, horizon)
                    row.update(analysis.observed())
                    row["warnings"].extend(ini_warnings)
                    file_rows.append(row)
                    file_analyzers[row["id"]] = analysis
                rows.extend(file_rows)
                analyzers.update(file_analyzers)
                item["status"] = "loaded"
            except (
                OSError,
                ValueError,
                KeyError,
                TypeError,
                subprocess.TimeoutExpired,
            ) as error:
                item.update(status="error", error=str(error))
                errors.append({"path": path.as_posix(), "error": str(error)})
    select(rows)
    selected = [r for r in rows if r["selected"]]
    for number, row in enumerate(selected):
        print(
            f"Analyze {number + 1}/{len(selected)}: {row['relative_path']} {row['instrument']}",
            file=sys.stderr,
            flush=True,
        )
        identity = [row["fingerprint"], row["instrument"]]
        row["random"] = analyzers[row["id"]].random_comparison(
            args.samples, args.seed, identity
        )
    report = {
        "schema": 1,
        "version": VERSION,
        "settings": {
            "seed": args.seed,
            "samples": args.samples,
            "roots": [str(r) for r in roots],
            "normalization": "first attack to last sustain end across supported parts",
            "context_quarter_beats": 16,
            "bar_tolerance_quarter_beats": "max(1/ppq,1/48)",
            "sustain_threshold_seconds": 0.01,
            "python": sys.version.split()[0],
            "dumper_sha256": sha256(dumper),
        },
        "inventory": inventory,
        "errors": errors,
        "rows": rows,
        "groups": aggregate(rows),
    }
    generated_catalog = {"schema": 1, "files": {}}
    for item in inventory:
        if item.get("status") == "loaded":
            generated_catalog["files"][item["path"]] = {
                "song_id": item["song_id"],
                "game": item["game"],
                "preferred": item["preferred"],
                "exclude": item["excluded"],
            }
    base_keys = (
        "id",
        "path",
        "title",
        "artist",
        "game",
        "instrument",
        "selected",
        "selection_reason",
    )
    charts, phrases = [], []
    for row in rows:
        base = {k: row[k] for k in base_keys}
        charts.append(
            {
                **base,
                **{
                    k: v
                    for k, v in row["statistics"].items()
                    if not isinstance(v, (dict, list))
                },
            }
        )
        phrases.extend({**base, **phrase} for phrase in row["phrases"])
    contents = {
        "report.json": json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False)
        + "\n",
        "report.html": render(report),
        "charts.csv": csv_text(charts),
        "phrases.csv": csv_text(phrases),
        "catalog.json": json.dumps(generated_catalog, ensure_ascii=False, indent=2)
        + "\n",
    }
    for name, content in contents.items():
        atomic_write(output / name, content, args.replace)
    print(
        json.dumps(
            {
                "files": len(paths),
                "loaded": sum(i.get("status") == "loaded" for i in inventory),
                "parts": len(rows),
                "representatives": len(selected),
                "errors": len(errors),
                "report": str(output / "report.html"),
            }
        )
    )
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "roots", nargs="+", help="Song directories or individual gameplay charts"
    )
    parser.add_argument("--dumper", required=True, help="Path to gpp_dump_chart")
    parser.add_argument("--output-dir", required=True)
    parser.add_argument(
        "--catalog",
        help="Version 1 JSON: files keyed by absolute or root-relative path",
    )
    parser.add_argument("--samples", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--replace", action="store_true")
    try:
        return run(parser.parse_args())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
