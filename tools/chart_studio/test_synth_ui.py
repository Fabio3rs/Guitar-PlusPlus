"""Exercise the explicit render action in real ImGui under Xvfb."""
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import time
import wave
from fixture import fixture

gui, cli = (Path(p).resolve() for p in sys.argv[1:3])
sf=Path(sys.argv[3])
if not sf.is_file():
    print('No test SoundFont; UI synthesis integration skipped')
    sys.exit(0)
with tempfile.TemporaryDirectory(prefix='gpp synth ui ') as temp:
    work=Path(temp);midi=work/'test.mid';fixture(midi);project=work/'test.gppchartproj'
    subprocess.run([str(cli),'convert',str(midi),'--track','0','--output',str(work/'test.chart')],check=True,capture_output=True)
    probe=subprocess.run([str(cli),'render-stems',str(project)],text=True,capture_output=True)
    if 'synthesis unavailable' in probe.stderr:
        print('Build without FluidSynth: UI synthesis test skipped')
        sys.exit(0)
    with sqlite3.connect(project) as db:
        doc=json.loads(db.execute("select value from metadata where key='document'").fetchone()[0])
        doc['synth']={'mode':'chart','soundfont':str(sf),'output':str(work/'rendered'),'format':'wav','sampleRate':16000,'quality':5,'gain':.5,'tail':.1,'stems':[]}
        db.execute("update metadata set value=? where key='document'",(json.dumps(doc),))
    (work/'layout-docking.ini').write_text('[Window][MIDI Synth]\nPos=0,30\nSize=760,900\nCollapsed=0\n')
    with (work/'gui.log').open('w') as log:
        process=subprocess.Popen([str(gui),str(project),'--output-dir',str(work)],stdout=log,stderr=log)
        try:
            window=None
            for _ in range(200):
                p=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],capture_output=True,text=True)
                if p.returncode==0:window=p.stdout.splitlines()[-1];break
                time.sleep(.1)
            assert window,(work/'gui.log').read_text()
            def send(*args):
                subprocess.run(['xdotool',*map(str,args)],check=True,capture_output=True)
                time.sleep(.4)
            def click(x,y):
                send('mousemove','--window',window,x,y);send('mousedown',1);send('mouseup',1)
            send('windowfocus','--sync',window);time.sleep(1)
            click(20,10);click(60,152)  # Panels > MIDI Synth.
            click(80,375)  # Explicit Render selected stems action.
            target=work/'rendered/guitar.wav'
            deadline=time.monotonic()+30
            while not target.exists() and time.monotonic()<deadline:time.sleep(.1)
            assert target.is_file(),(work/'gui.log').read_text()
            with wave.open(str(target)) as audio:assert audio.getframerate()==16000 and audio.getnframes()>0
            send('key','ctrl+q');assert process.wait(timeout=15)==0
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)
print('ImGui synthesis panel: offline WAV render and shutdown passed')
