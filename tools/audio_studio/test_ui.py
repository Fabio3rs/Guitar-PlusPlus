"""Exercise actual ImGui controls under Xvfb; RPC observes resulting state."""
import json
import pathlib
import subprocess
import sys
import tempfile
import time
from test_cli import fixture, rpc, wait_job

exe = str(pathlib.Path(sys.argv[1]).resolve())
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
source = output / 'source.wav'
fixture(source)
with tempfile.TemporaryDirectory(dir=output) as tmp:
    with open(output / 'stderr.log', 'w') as stderr:
        p = subprocess.Popen([exe, '--device', '0', '--file', str(source), '--output-dir', tmp, '--rpc-stdio'],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr, text=True)
        try:
            rpc(p, 'state')
            window = subprocess.check_output(['xdotool', 'search', '--sync', '--name', '^Guitar\\+\\+ Audio Studio$'], text=True).splitlines()[-1]
            time.sleep(.3)
            def click(x, y):
                subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y),
                                'sleep', '.1', 'mousedown', '1', 'sleep', '.08', 'mouseup', '1'],
                               check=True, timeout=5)
                time.sleep(.1)
            before = rpc(p, 'state')['result']
            # Default layout places waveform in upper center, source sidebar left.
            click(650, 130)
            after = rpc(p, 'state')['result']
            assert after['cursor'] != before['cursor'], (before['cursor'], after['cursor'])
            assert 0 <= after['cursor'] < 1
            click(350, 709)
            fretted = rpc(p, 'state')['result']
            assert fretted['settings']['synth.fretMask'] == (after['settings']['synth.fretMask'] ^ 1)
            click(1305, 12)  # Whole Song Analysis dock tab.
            click(1060, 90)  # Scan whole song.
            scanned = wait_job(p)
            assert not scanned['error'] and scanned['adaptive']['regions'] == 1, scanned['adaptive']
            click(1090, 221)  # Generate five per set.
            generated = wait_job(p)
            assert not generated['error'] and generated['adaptive']['samples'] == 5, generated['adaptive']
            assert abs(generated['adaptive']['settings']['synth.duration'] - .051) < .00001
            click(1240, 221)  # Prepare BASS bank.
            prepared = wait_job(p)
            assert not prepared['error'] and prepared['adaptive']['prepared'], prepared['adaptive']
            region = rpc(p, 'song.regions')['result']['region_list'][0]
            click(650, 95)  # Region strip selects the representative.
            selected = rpc(p, 'state')['result']
            assert abs(selected['cursor'] - region['representative']) < .001
            assert 'result' in rpc(p, 'cache.preview', {'mask': 31, 'time': selected['cursor']})
            rpc(p, 'shutdown')
            assert p.wait(timeout=10) == 0
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
    subprocess.run([exe, '--device', '0', '--file', str(source), '--output-dir', tmp,
                    '--frames', '4', '--ui-capture', 'studio.png'], check=True, timeout=30)
    data = (pathlib.Path(tmp) / 'studio.png').read_bytes()
    assert data.startswith(b'\x89PNG') and len(data) > 10000
    (output / 'studio.png').write_bytes(data)
(output / 'report.json').write_text(json.dumps({'waveform_seek': 'passed', 'frets': 'passed',
                                             'adaptive_scan_generate_prepare': 'passed',
                                             'region_selection': 'passed', 'capture': 'passed'}, indent=2))
print('Audio UI tests passed')
