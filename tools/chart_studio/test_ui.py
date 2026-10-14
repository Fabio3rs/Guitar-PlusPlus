#!/usr/bin/env python3
"""Exercise actual ImGui selection, manual mapping, locking, regeneration and save."""
from pathlib import Path
import json
import os
import sqlite3
import subprocess
import sys
import tempfile
import time
from fixture import fixture

exe=Path(sys.argv[1]).resolve()
root=Path(sys.argv[2]).resolve()
root.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ui-',dir=root) as temporary:
    work=Path(temporary)
    midi=work/'musical.mid'
    fixture(midi)
    log=open(root/'ui.log','w')
    process=subprocess.Popen([str(exe),str(midi),'--track','0','--generate','--output-dir',str(work)],stdout=log,stderr=subprocess.STDOUT)
    def xdo(*args):
        def send(*values):
            subprocess.run(['xdotool',*map(str,values)],check=True,capture_output=True,timeout=5)
        if args[0]=='key' and args[1].startswith('ctrl+'):
            # Give the software renderer separate frames for modifier, press and release.
            send('keydown','ctrl'); time.sleep(.3)
            send('keydown',args[1][5:]); time.sleep(.3)
            send('keyup',args[1][5:]); time.sleep(.3)
            send('keyup','ctrl')
        else:
            send(*args)
        time.sleep(.5)
    try:
        deadline=time.monotonic()+20
        window=None
        while time.monotonic()<deadline and process.poll() is None:
            p=subprocess.run(['xdotool','search','--all','--onlyvisible','--pid',str(process.pid),'--name',r'^Guitar\+\+ Chart Studio$'],capture_output=True,text=True)
            if p.returncode==0:
                window=p.stdout.strip().splitlines()[-1]
                break
            time.sleep(.1)
        assert window,(root/'ui.log').read_text()
        xdo('windowfocus','--sync',window)
        time.sleep(.5)
        xdo('mousemove','--window',window,416,398)
        xdo('click',1)
        xdo('key','1')
        xdo('key','ctrl+l')
        xdo('key','ctrl+s')
        project=work/'untitled.gppchartproj'
        deadline=time.monotonic()+10
        while not project.exists() and time.monotonic()<deadline:
            time.sleep(.1)
        if not project.exists():
            subprocess.run(['import','-window',window,str(root/'failed.png')],timeout=10)
        assert project.exists(),(root/'ui.log').read_text()
        def state():
            with sqlite3.connect(project) as db:
                assert db.execute('SELECT count(*) FROM patterns WHERE status=2').fetchone()[0]>0, 'Click did not select/lock a phrase'
                return json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])['value0']
        before=state()
        xdo('key','ctrl+g')
        xdo('key','ctrl+s')
        after=state()
        def masks(s):
            return [(e['value0']['value0'],e['value0']['value3']) for e in s['value6'] if e['value9']==2]
        assert dict(masks(before)).items() <= dict(masks(after)).items()
        final_masks=[e['value0']['value3'] for e in after['value6']]
        assert final_masks[:5]==final_masks[5:10]==final_masks[10:], final_masks
        assert any(e['value10'] for e in after['value6']), 'Manual edit missing'
        xdo('key','ctrl+e')
        assert (work/'untitled.generated.chart').exists()
        xdo('key','ctrl+q')
        assert process.wait(timeout=15)==0,(root/'ui.log').read_text()
        subprocess.run([str(exe),str(project),'--frames','5','--output-dir',str(work),
                        '--ui-capture',str(root/'reopened.png')],check=True,timeout=30)
        print('Mouse edit, phrase lock, generation, save, export and reopen passed')
    finally:
        if process.poll() is None:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill();process.wait()
        log.close()
