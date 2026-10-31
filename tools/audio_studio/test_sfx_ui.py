"""Real ImGui SFX controls under Xvfb/Mesa, with RPC as an observer."""
import json
import pathlib
import subprocess
import shutil
import sys
import tempfile
import time
from test_cli import rpc, wait_job

exe = str(pathlib.Path(sys.argv[1]).resolve())
specs = pathlib.Path(sys.argv[2]).resolve()
output = pathlib.Path(sys.argv[3]).resolve()
output.mkdir(parents=True, exist_ok=True)
spec = specs / 'plus/plus_activate.lua'
with tempfile.TemporaryDirectory(dir=output) as tmp:
    subprocess.run([exe, '--device', '0', '--sfx', str(spec), '--output-dir', tmp,
                    '--frames', '6', '--ui-capture', 'sfx.png'], check=True, timeout=30)
    (output / 'sfx.png').write_bytes((pathlib.Path(tmp) / 'sfx.png').read_bytes())
    with open(output / 'stderr.log', 'w') as stderr:
        p = subprocess.Popen([exe, '--device', '0', '--sfx', str(spec), '--output-dir', tmp,
                              '--rpc-stdio'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=stderr, text=True)
        try:
            initial = rpc(p, 'state')['result']
            assert not initial['source'] and initial['sfx']['generated']
            assert initial['sfx']['generated_id'] == 'plus_activate'
            window = subprocess.check_output(['xdotool', 'search', '--sync', '--name',
                                               '^Guitar\\+\\+ Audio Studio$'], text=True).splitlines()[-1]
            time.sleep(.3)
            def click(x, y):
                subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y),
                                'sleep', '.08', 'mousedown', '1', 'sleep', '.06', 'mouseup', '1'],
                               check=True, timeout=5)
                time.sleep(.1)

            def enter(x, y, value):
                click(x, y)
                subprocess.run(['xdotool', 'key', '--clearmodifiers', 'ctrl+a'], check=True)
                subprocess.run(['xdotool', 'type', '--clearmodifiers', '--', value], check=True)
                subprocess.run(['xdotool', 'key', 'Return'], check=True)
                time.sleep(.1)

            click(417, 334)  # Sound tab in the initial floating designer.
            enter(520, 566, '9911')  # Seed; editing must not start a job.
            edited = rpc(p, 'state')['result']
            assert edited['sfx']['seed'] == 9911, edited['sfx']
            assert edited['sfx']['stale'] and not edited['busy']
            click(357, 80)  # Regenerate.
            regenerated = wait_job(p)
            assert not regenerated['error'] and not regenerated['sfx']['stale']
            assert regenerated['job_id'] > edited['job_id']

            click(539, 80)  # Loop Preview checkbox.
            click(438, 80)  # Play through the actual button.
            assert rpc(p, 'state')['result']['sfx']['preview_state'] != 0
            click(493, 80)  # Stop.
            assert rpc(p, 'state')['result']['sfx']['preview_state'] == 0

            click(482, 334)  # Layers tab.
            click(357, 367)  # Add layer.
            assert rpc(p, 'state')['result']['sfx']['layers'] == 7
            click(481, 367)  # Remove the new layer.
            assert rpc(p, 'state')['result']['sfx']['layers'] == 6

            click(346, 334)  # Recipes tab.
            click(511, 727)  # Duplicate, so the test never saves into the repo.
            assert rpc(p, 'state')['result']['sfx']['id'] == 'plus_activate_copy'
            click(355, 794)  # Save Lua.
            saved = pathlib.Path(tmp) / 'specs/plus_activate_copy.lua'
            assert saved.is_file() and 'seed = 9911' in saved.read_text()
            assert not rpc(p, 'state')['result']['sfx']['dirty']

            rpc(p, 'sfx.set', {'recipe': saved.read_text().replace('seed = 9911', 'seed = 9912')})
            assert 'error' in rpc(p, 'sfx.load', {'path': str(spec)})
            click(355, 727)  # New must not discard the edited copy.
            click(815, 521)  # Cancel in the unsaved-changes modal.
            retained = rpc(p, 'state')['result']['sfx']
            assert retained['seed'] == 9912 and retained['dirty']
            assert retained['id'] == 'plus_activate_copy'

            click(355, 727)
            occupied = pathlib.Path(tmp) / 'occupied.lua'
            occupied.write_text('preserve this file')
            enter(650, 489, str(occupied))
            click(620, 521)  # Failed save must leave the modal/draft available.
            assert rpc(p, 'state')['result']['sfx']['seed'] == 9912
            assert occupied.read_text() == 'preserve this file'
            enter(650, 489, str(saved))
            click(620, 521)  # Save and continue creates the new template.
            created = rpc(p, 'state')['result']['sfx']
            assert created['id'] == 'new_sfx' and created['dirty']
            assert 'seed = 9912' in saved.read_text()
            rpc(p, 'sfx.load', {'path': str(saved), 'discard': True})
            assert not wait_job(p)['sfx']['dirty']
            rpc(p, 'sfx.set', {'recipe': saved.read_text().replace('seed = 9912', 'seed = 9913')})

            enter(500, 433, 'garage_room_tone')  # Filter and open a real loop recipe.
            click(440, 473)
            assert rpc(p, 'state')['result']['sfx']['id'] == 'plus_activate_copy'
            if shutil.which('import'):
                subprocess.run(['import', '-window', window, str(output / 'sfx-unsaved.png')],
                               check=True, timeout=10)
            click(743, 521)  # Explicitly discard edits before opening the loop.
            loaded = wait_job(p)
            assert loaded['sfx']['id'] == 'garage_room_tone', loaded['sfx']
            assert not loaded['sfx']['dirty'] and 'seed = 9912' in saved.read_text()
            assert loaded['sfx']['generated_id'] == 'plus_activate'
            click(357, 80)
            generated = wait_job(p)
            assert not generated['error']
            assert generated['sfx']['loop_end'] == generated['sfx']['frames']
            click(438, 80)
            assert rpc(p, 'state')['result']['sfx']['loop_error'] == 0
            click(493, 80)

            # The loop metrics add one 23-pixel row beneath the waveform.
            click(549, 357)  # Export tab.
            click(323, 423)  # Float32 checkbox.
            click(362, 456)  # Export WAV.
            completed = wait_job(p)
            assert not completed['error'], completed['error']
            data = (pathlib.Path(tmp) / 'sfx.wav').read_bytes()
            assert data[20:22] == b'\x03\x00' and b'smpl' in data
            if shutil.which('import'):
                subprocess.run(['import', '-window', window, str(output / 'sfx-edited.png')],
                               check=True, timeout=10)
            rpc(p, 'shutdown')
            assert p.wait(timeout=10) == 0
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
print('SFX UI: edit, generate, layers, duplicate/save, browse, loop preview and float WAV export passed')
