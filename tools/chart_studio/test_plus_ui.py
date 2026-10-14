#!/usr/bin/env python3
"""Real ImGui Plus preview/apply, undo and persistence under Xvfb."""
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
with tempfile.TemporaryDirectory(prefix='plus-', dir=root) as directory:
    work = Path(directory)
    midi = work / 'musical.mid'
    fixture(midi)
    with (root / 'ui.log').open('w') as log:
        process = subprocess.Popen([str(exe), str(midi), '--track', '0', '--generate',
                                    '--output-dir', str(work)], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            window = None
            while time.monotonic() < deadline and process.poll() is None:
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid),
                                        '--name', 'Guitar'], capture_output=True, text=True)
                if found.returncode == 0:
                    window = found.stdout.strip().splitlines()[-1]
                    break
                time.sleep(.1)
            assert window, (root / 'ui.log').read_text()

            def send(*args):
                subprocess.run(['xdotool', *map(str, args)], check=True, capture_output=True, timeout=5)
                time.sleep(.25)

            def click(x, y):
                send('mousemove', '--window', window, x, y)
                send('mousedown', 1)
                send('mouseup', 1)

            def shortcut(key):
                send('keydown', 'ctrl')
                send('keydown', key)
                send('keyup', key)
                send('keyup', 'ctrl')
                time.sleep(.3)

            def capture(name):
                if shutil.which('import'):
                    subprocess.run(['import', '-window', window, str(root / name)], check=True)

            project = work / 'musical.gppchartproj'

            def document():
                with sqlite3.connect(project) as db:
                    return json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])

            send('windowfocus', '--sync', window)
            time.sleep(1)
            click(1380, 28)  # Plus dock tab.
            click(1078, 73)  # Opt in.
            click(1150, 320)  # Analyze and propose.
            time.sleep(2)
            capture('preview.png')
            shortcut('s')
            before = document()
            assert not before['plus']['phrases'], 'Preview mutated applied document'
            click(1150, 425)  # Apply proposal below the sequence simulation summary.
            shortcut('s')
            applied = document()
            assert applied['plus']['phrases'], 'Apply did not store Plus phrases'
            assert applied['value0']['value6'] == before['value0']['value6'], 'Plus changed lanes'
            capture('applied.png')
            shortcut('z')
            shortcut('s')
            assert not document()['plus']['phrases']
            shortcut('y')
            shortcut('s')
            assert document()['plus']['phrases'] == applied['plus']['phrases']
            shortcut('e')
            assert b' = S 2 ' in (work / 'musical.generated.chart').read_bytes()
            shortcut('q')
            assert process.wait(timeout=15) == 0, (root / 'ui.log').read_text()
            # Reopen the saved project using the real UI and run its shutdown path.
            subprocess.run([str(exe), str(project), '--output-dir', str(work), '--frames', '4'],
                           check=True, stdout=log, stderr=log, timeout=20)
            print('Plus UI: preview isolated; apply, undo/redo, save/reopen and S2 export passed')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
