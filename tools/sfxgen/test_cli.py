"""Headless integration; generated recipes only, no audio device or BASS init."""
import json
import pathlib
import subprocess
import sys
import tempfile

exe, specs = map(lambda x: str(pathlib.Path(x).resolve()), sys.argv[1:3])


def run(*args, ok=True):
    p = subprocess.run([exe, *map(str, args)], capture_output=True, text=True, timeout=40)
    assert (p.returncode == 0) == ok, (p.returncode, p.stdout, p.stderr)
    return p


with tempfile.TemporaryDirectory(prefix='gpp-sfx-cli-') as tmp:
    root = pathlib.Path(tmp)
    check = json.loads(run('--all', specs, '--check', '--json').stdout)
    assert check['failures'] == 0 and len(check['recipes']) == 17
    generated = json.loads(run('--all', specs, '--output-dir', root, '--json').stdout)
    assert generated['failures'] == 0 and len(list(root.glob('*.wav'))) == 17
    run('--all', specs, '--compare-dir', root)
    run('--all', specs, '--output-dir', root, ok=False)
    run('--all', specs, '--output-dir', root, '--force')
    run('--all', specs, '--compare-dir', root)
    source = pathlib.Path(specs) / 'plus/plus_activate.lua'
    run('--spec', source, '--output', root / 'float.wav', '--float32')
    assert (root / 'float.wav').read_bytes()[20:22] == b'\x03\x00'
    (root / 'plus_activate.wav').write_bytes(b'unchanged')
    run('--all', specs, '--compare-dir', root, ok=False)
    assert (root / 'plus_activate.wav').read_bytes() == b'unchanged'
    batch = root / 'batch'
    batch.mkdir()
    (batch / 'valid.lua').write_text(source.read_text())
    (batch / 'invalid.lua').write_text('return {schemaVersion=99}')
    report = json.loads(run('--all', batch, '--output-dir', root / 'partial', '--json', ok=False).stdout)
    assert report['failures'] == 1 and len(report['recipes']) == 2
    assert (root / 'partial/plus_activate.wav').is_file()
    run('--spec', source, '--output', root / 'wrong.mp3', ok=False)
    run('--spec', source, '--all', specs, '--check', ok=False)
    empty = root / 'empty'
    empty.mkdir()
    run('--all', empty, '--check', ok=False)

    run('--all', specs, '--check', '--require-filename-id')
    run('--spec', batch / 'valid.lua', '--check', '--require-filename-id', ok=False)
    # Collisions are checked before any writes, on Linux as well as Windows.
    collisions = root / 'collisions'
    collisions.mkdir()
    template = 'return {schemaVersion=1,id="%s",duration=.01,layers={{type="noise"}}}'
    for index, name in enumerate(['Click', 'click', 'CLICK']):
        (collisions / f'{index}.lua').write_text(template % name)
    (collisions / 'valid.lua').write_text(template % 'other')
    output = root / 'colliding-output'
    output.mkdir()
    (output / 'Click.wav').write_bytes(b'keep existing audio')
    report = json.loads(run('--all', collisions, '--output-dir', output,
                            '--force', '--json', ok=False).stdout)
    assert report['failures'] == 3
    assert all('collision' in r['error'] for r in report['recipes'] if not r['ok'])
    assert (output / 'Click.wav').read_bytes() == b'keep existing audio'
    assert (output / 'other.wav').is_file()
    assert len(list(output.glob('*.wav'))) == 2
    run('--all', collisions, '--check', ok=False)
print('SFX CLI: validation, batch, export, comparison and failure isolation passed')
