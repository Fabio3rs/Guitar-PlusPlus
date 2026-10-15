#!/usr/bin/env python3
"""Real ImGui draft/preview/apply/undo and reopening; Xvfb required."""
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from fixture import merge_fixture

gui, cli = (Path(p).resolve() for p in sys.argv[1:3])
root = Path(sys.argv[3]).resolve()
root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=root) as folder:
    work = Path(folder)
    midi = work / "synthetic.mid"
    merge_fixture(midi)
    project = work / "synthetic.gppchartproj"
    subprocess.run([str(cli), 'convert', str(midi), '--part', 'ExpertSingle:0',
                    '--merge-source', 'ExpertSingle:1', '--minimum-sustain', '0',
                    '--output', str(work/'synthetic.chart')], check=True, capture_output=True)
    layout = ''
    for name in ['Files', 'Tracks', 'Mapping', 'Edit and patterns', 'Audio alignment', 'Plus', 'Timeline']:
        layout += f'[Window][{name}]\nPos=800,30\nSize=600,900\nCollapsed=0\n\n'
    layout += '[Window][Merge tracks]\nPos=0,30\nSize=780,900\nCollapsed=0\n\n'
    (work/'layout-docking.ini').write_text(layout)
    def document():
        with sqlite3.connect(project) as db:
            return json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])
    def config():
        return document()['merges'][0]['value']['config']
    with (root/'ui.log').open('w') as log:
        process = subprocess.Popen([str(gui), str(project), '--output-dir', str(work)], stdout=log, stderr=log)
        try:
            window = None
            deadline = time.monotonic()+25
            while time.monotonic()<deadline and process.poll() is None:
                p = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),'--name','Guitar'],
                                   capture_output=True,text=True)
                if p.returncode==0:
                    window=p.stdout.strip().splitlines()[-1]
                    break
                time.sleep(.1)
            assert window,(root/'ui.log').read_text()
            def send(*args):
                subprocess.run(['xdotool',*map(str,args)],check=True,capture_output=True,timeout=5)
                time.sleep(.4)
            def click(x,y):
                send('mousemove','--window',window,x,y)
                send('mousedown',1);send('mouseup',1)
            def shortcut(key):
                send('keydown','ctrl');send('keydown',key);send('keyup',key);send('keyup','ctrl')
                time.sleep(.5)  # Separate shortcut transactions under software rendering + ASan.
            def capture(name):
                if shutil.which('import'):
                    subprocess.run(['import','-window',window,str(root/name)],check=True,timeout=10)
            send('windowfocus','--sync',window)
            time.sleep(1)
            click(100,187) # Minimum silence.
            shortcut('a')
            send('type','--clearmodifiers','4')
            send('key','Tab')
            click(55,427) # Analyze.
            time.sleep(1)
            shortcut('s')
            assert config()['minimum']=='1/2','Preview mutated saved recipe'
            capture('preview.png')
            click(154,427) # Apply.
            shortcut('s')
            assert config()['minimum']=='4','Apply did not save recipe'
            assert len(document()['value0']['value6'])==2
            capture('applied.png')
            shortcut('z');shortcut('s')
            assert config()['minimum']=='1/2'
            assert len(document()['value0']['value6'])==3
            shortcut('y');shortcut('s')
            assert config()['minimum']=='4'
            shortcut('q')
            assert process.wait(timeout=15)==0
            subprocess.run([str(gui),str(project),'--output-dir',str(work),'--frames','4'],
                           check=True,stdout=log,stderr=log,timeout=25)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
print('Merge GUI: draft, preview, apply, undo/redo and reopen passed')
