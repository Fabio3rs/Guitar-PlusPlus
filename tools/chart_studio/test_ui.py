#!/usr/bin/env python3
"""Exercise file browsing, UI-only generation, editing, docking and persistence."""

from pathlib import Path
import json
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from fixture import fixture

exe = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="ui-", dir=root) as temporary:
    work = Path(temporary)
    midi = work / "musical.mid"
    fixture(midi)
    log = open(root / "ui.log", "w")
    process = subprocess.Popen(
        [str(exe), "--output-dir", str(work)], stdout=log, stderr=subprocess.STDOUT
    )

    def xdo(*args):
        def send(*values):
            subprocess.run(
                ["xdotool", *map(str, values)],
                check=True,
                capture_output=True,
                timeout=5,
            )

        if args[0] == "key" and args[1].startswith("ctrl+"):
            # Give the software renderer separate frames for modifier, press and release.
            send("keydown", "ctrl")
            time.sleep(0.3)
            send("keydown", args[1][5:])
            time.sleep(0.3)
            send("keyup", args[1][5:])
            time.sleep(0.3)
            send("keyup", "ctrl")
        else:
            send(*args)
        time.sleep(0.5)

    def screenshot(name):
        if shutil.which("import"):
            subprocess.run(
                ["import", "-window", window, str(root / name)], check=True, timeout=10
            )

    try:
        deadline = time.monotonic() + 20
        window = None
        while time.monotonic() < deadline and process.poll() is None:
            p = subprocess.run(
                [
                    "xdotool",
                    "search",
                    "--all",
                    "--onlyvisible",
                    "--pid",
                    str(process.pid),
                    "--name",
                    r"^Guitar\+\+ Chart Studio$",
                ],
                capture_output=True,
                text=True,
            )
            if p.returncode == 0:
                window = p.stdout.strip().splitlines()[-1]
                break
            time.sleep(0.1)
        assert window, (root / "ui.log").read_text()
        xdo("windowfocus", "--sync", window)
        time.sleep(0.5)

        def click(x, y):
            # Separate movement, press and release across rendered frames.
            xdo("mousemove", "--window", window, x, y)
            xdo("mousedown", 1)
            xdo("mouseup", 1)

        click(15, 53)  # Contextual manual; longer explanations stay out of the canvas.
        screenshot("manual.png")
        click(450, 650)  # Close manual.
        click(340, 106)  # Browse musical MIDI, no CLI source/track/generation.
        click(550, 287)  # Folder field in the browser modal.
        xdo("key", "ctrl+a")
        xdo("type", "--clearmodifiers", str(work))
        xdo("key", "Return")
        click(440, 385)  # Only musical.mid matches the browser's type filter.
        screenshot("browser.png")
        click(413, 669)  # Use path; this must not open the MIDI by itself.
        click(83, 130)  # Open MIDI / project.
        click(110, 28)  # Tracks tab.
        click(150, 231)  # Active part's musical-source selector.
        click(100, 253)  # First musical track.
        click(453, 90)  # Generate chart (timeline guide).
        screenshot("generated.png")
        click(428, 426)  # Second generated attack, initially red.
        xdo("key", "1")
        xdo("key", "ctrl+l")
        xdo("key", "ctrl+s")
        project = work / "musical.gppchartproj"
        deadline = time.monotonic() + 10
        while not project.exists() and time.monotonic() < deadline:
            time.sleep(0.1)
        if not project.exists():
            screenshot("failed.png")
        assert project.exists(), (root / "ui.log").read_text()

        def state():
            with sqlite3.connect(project) as db:
                assert (
                    db.execute(
                        "SELECT count(*) FROM patterns WHERE status=2"
                    ).fetchone()[0]
                    > 0
                ), "Click did not select/lock a phrase"
                return json.loads(
                    db.execute(
                        "SELECT value FROM metadata WHERE key='document'"
                    ).fetchone()[0]
                )["value0"]

        before = state()
        # Under software rendering a shortcut can be delivered between frames.
        # Wait for the observable saved result, with bounded retries, rather
        # than asserting against the previous save immediately.
        for attempt in range(3):
            xdo("key", "ctrl+g")
            time.sleep(1)
            xdo("key", "ctrl+s")
            after = state()
            final_masks = [e["value0"]["value3"] for e in after["value6"]]
            if final_masks[:5] == final_masks[5:10] == final_masks[10:]:
                break

        def masks(s):
            return [
                (e["value0"]["value0"], e["value0"]["value3"])
                for e in s["value6"]
                if e["value9"] == 2
            ]

        assert dict(masks(before)).items() <= dict(masks(after)).items()
        final_masks = [e["value0"]["value3"] for e in after["value6"]]
        assert final_masks[:5] == final_masks[5:10] == final_masks[10:], final_masks
        assert any(e["value10"] for e in after["value6"]), "Manual edit missing"
        click(40, 186)  # Add the default new Bass / Expert part.
        click(150, 248)  # Source selector after the additional table row.
        click(100, 270)  # The same MIDI track may feed another game instrument.
        click(80, 307)  # Generate all parts (asynchronous batch).
        time.sleep(2)
        xdo("key", "ctrl+s")

        def whole_document():
            with sqlite3.connect(project) as db:
                return json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])

        multi = whole_document()
        screenshot("multipart.png")
        assert multi['instrument'] == 'bass' and len(multi['parts']) == 1
        assert len(multi['value0']['value6']) == 15, 'Batch did not generate bass'
        assert len(multi['parts'][0]['events']) == 15, 'Guitar disappeared'
        assert any(e['value9'] == 2 for e in multi['parts'][0]['events']), 'Guitar lock lost'
        xdo("key", "ctrl+z")
        xdo("key", "ctrl+s")
        assert not whole_document()['value0']['value6'], 'Batch undo did not restore empty bass'
        xdo("key", "ctrl+y")
        xdo("key", "ctrl+s")
        assert len(whole_document()['value0']['value6']) == 15
        click(80, 136)  # Select Guitar again; navigation must preserve both parts.
        xdo("key", "ctrl+s")
        assert whole_document()['instrument'] == 'guitar'
        xdo("key", "ctrl+e")
        exported = (work / "musical.generated.chart").read_text()
        assert '[ExpertSingle]' in exported and '[ExpertDoubleBass]' in exported
        xdo("mousemove", "--window", window, 1200, 28)  # Detach editing tab.
        xdo("mousedown", 1)
        xdo("mousemove", "--window", window, 780, 300)
        xdo("mouseup", 1)
        xdo("windowsize", window, 1000, 700)
        xdo("windowsize", window, 1440, 960)
        xdo("key", "ctrl+q")
        assert process.wait(timeout=15) == 0, (root / "ui.log").read_text()
        layout = (work / "layout-docking.ini").read_text()
        assert "[Docking][Data]" in layout and "DockId=" in layout
        subprocess.run(
            [
                str(exe),
                str(project),
                "--frames",
                "5",
                "--output-dir",
                str(work),
                "--ui-capture",
                str(root / "reopened.png"),
            ],
            check=True,
            timeout=30,
        )
        print(
            "Browser, UI-only load/generation, edit, phrase lock, save/export, docking and reopen passed"
        )
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log.close()
