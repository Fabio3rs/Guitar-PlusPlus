#!/usr/bin/env python3
"""Synthetic SMF -> persistent project -> second process -> runtime round trip."""
import json
import math
import wave
import os
from pathlib import Path
import struct
from contextlib import closing
import sqlite3
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)

from fixture import fixture

env = os.environ.copy()
env.pop('DISPLAY', None)

def run(*args, ok=True):
    result = subprocess.run([str(exe), *map(str, args)], env=env, capture_output=True, text=True, timeout=60)
    if ok:
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0, result.stdout + result.stderr
    return result

with tempfile.TemporaryDirectory(prefix='cli-', dir=root) as work:
    work = Path(work)
    midi = work / 'musical with spaces.mid'
    fixture(midi)
    audio = work / 'tone.wav'
    with wave.open(str(audio), 'wb') as wavefile:
        wavefile.setparams((1,2,44100,0,'NONE','not compressed'))
        wavefile.writeframes(b''.join(struct.pack('<h',int(10000*math.sin(2*math.pi*440*i/44100))) for i in range(44100)))
    fft = json.loads(run('fft', audio, '--at', '.1').stdout)
    assert fft['sample_rate']==44100 and len(fft['spectrum'])==4096
    assert any(n['pitch']==69 for n in fft['candidates'])
    source = midi.read_bytes()
    project = work / 'song.gppchartproj'
    chart = work / 'song.generated.chart'
    library = work / 'library.sqlite'
    base = ['convert', midi, '--track', 0, '--project', project, '--output', chart]
    run('inspect', midi)
    first = json.loads(run(*base).stdout)
    assert first['attacks'] == 15 and first['reused_events'] > 0
    original = chart.read_bytes()
    second = json.loads(run(*base, '--replace').stdout)
    assert second['reuse_ratio'] == 1 and second['new_patterns'] == 0
    assert chart.read_bytes() == original
    run('patterns', project, 'lock', '--event', 1)
    run('patterns', project, 'clear-experimental')
    run(*base, '--replace', '--weight0', 0, '--weight1', 100)
    assert chart.read_bytes() == original
    run('patterns', project, 'promote', '--event', 1, '--cache', library)
    library_before = library.read_bytes()
    another = work / 'other.chart'
    third = json.loads(run('convert', midi, '--track', 0, '--output', another, '--cache', library).stdout)
    assert third['reuse_ratio'] == 1
    assert library.read_bytes() == library_before
    assert json.loads(run('compare', chart, another).stdout)['windows']
    run('convert', midi, '--track', 0, '--output', midi, '--replace', ok=False)
    assert midi.read_bytes() == source
    run(*base, ok=False)
    assert chart.read_bytes() == original
    assert first['sustains']==0 and first['short_sustains_removed']==15
    sustain_chart = work / 'with-sustains.chart'
    sustained = json.loads(run('convert', midi, '--track', 0, '--output', sustain_chart,
                               '--sustain-beats', '.125').stdout)
    assert sustained['sustains']==15
    run('convert', midi, '--track', 0, '--output', work/'bad.chart', '--sustain-beats', '-1', ok=False)
    run('convert', midi, '--track', 0, '--output', work/'bad-seconds.chart', '--minimum-sustain', '-2', ok=False)
    project_bytes = project.read_bytes()
    run('convert', midi, '--track', 0, '--project', project, '--output', project, '--replace', ok=False)
    run('compare', chart, another, '--output', chart, '--replace', ok=False)
    run(*base, '--replace', '--report', midi, ok=False)
    run(*base, '--replace', '--track', '1e100', ok=False)
    assert project.read_bytes() == project_bytes and midi.read_bytes() == source
    plus_chart = work / 'with-plus.chart'
    plus = json.loads(run('convert', midi, '--track', 0, '--plus', 'auto',
                          '--plus-trials', 8, '--output', plus_chart).stdout)
    assert len(plus['simulations']) == 20
    assert all(s['completion'] == 'sequence' for s in plus['simulations'])
    assert plus['phrases'], plus
    plus_bytes = plus_chart.read_bytes()
    assert b' = S 2 ' in plus_bytes
    assert [line for line in plus_bytes.splitlines() if b' = N ' in line] == [line for line in original.splitlines() if b' = N ' in line]
    run('convert', midi, '--track', 0, '--plus', 'auto', '--plus-trials', 8,
        '--output', plus_chart, '--replace')
    assert plus_chart.read_bytes() == plus_bytes
    analyzed = json.loads(run('plus-simulate', plus_chart, '--plus-trials', 8).stdout)
    assert len(analyzed['phrases']) == len(plus['phrases'])
    run('plus-generate', project, '--output', midi, '--replace', ok=False)
    assert midi.read_bytes() == source
    print(json.dumps({'first': first, 'second_process': second, 'confirmed_library': third}, indent=2))

    # Repeated --part, real loader validation, persistence and scoped edits.
    multi = work / 'multi.chart'
    multi_project = multi.with_suffix('.gppchartproj')
    args = ['convert', midi, '--part', 'ExpertSingle:0', '--part', 'ExpertDoubleBass:0',
            '--part', 'ExpertKeyboard:0', '--plus', 'auto', '--plus-trials', 2, '--output', multi]
    batch = json.loads(run(*args).stdout)
    assert len(batch['parts']) == 3
    multi_bytes = multi.read_bytes()
    for section in ('ExpertSingle', 'ExpertDoubleBass', 'ExpertKeyboard'):
        assert f'[{section}]'.encode() in multi_bytes
        assert json.loads(run('events', multi_project, '--instrument', section).stdout)['events']
    run('patterns', multi_project, 'lock', '--instrument', 'ExpertDoubleBass', '--event', 1)
    run(*args, '--replace')
    assert multi.read_bytes() == multi_bytes
    assert 'status=2' in run('patterns', multi_project, 'list', '--instrument', 'ExpertDoubleBass').stdout
    assert 'status=2' not in run('patterns', multi_project, 'list', '--instrument', 'ExpertSingle').stdout
    reversed_chart = work / 'reversed.chart'
    run('convert', midi, '--part', 'ExpertKeyboard:0', '--part', 'ExpertDoubleBass:0',
        '--part', 'ExpertSingle:0', '--plus', 'auto', '--plus-trials', 2, '--output', reversed_chart)
    assert reversed_chart.read_bytes() == multi_bytes
    simulations = json.loads(run('plus-simulate', multi_project, '--plus-trials', 2).stdout)
    assert len(simulations['parts']) == 3
    single = json.loads(run('plus-simulate', multi_project, '--instrument', 'ExpertDoubleBass', '--plus-trials', 2).stdout)
    assert len(single['simulations']) == 20
    run('compare', multi_project, multi, '--instrument', '[ExpertDoubleBass]')
    # Reports must not overwrite reference charts belonging to inactive parts.
    reference = work / 'inactive-reference.chart'
    reference.write_bytes(multi_bytes)
    with closing(sqlite3.connect(multi_project)) as db, db:
        document = json.loads(db.execute("SELECT value FROM metadata WHERE key='document'").fetchone()[0])
        document['parts'][0]['reference'] = str(reference)
        db.execute("UPDATE metadata SET value=? WHERE key='document'", (json.dumps(document),))
    protected_output = work / 'protected-plus.chart'
    result = run('plus-generate', multi_project, '--output', protected_output,
                 '--report', reference, '--replace', '--plus-trials', 2, ok=False)
    assert reference.read_bytes() == multi_bytes
    assert not protected_output.exists(), result.stderr
    for invalid in (['--part', 'ExpertSingle:0', '--part', 'ExpertSingle:0'],
                    ['--part', 'ExpertDrums:0'], ['--part', 'ExpertDoubleBass:99'],
                    ['--part', 'ExpertSingle:0', '--track', 0],
                    ['--part', 'ExpertSingle:0:15']):
        bad = work / 'bad-multipart.chart'
        run('convert', midi, *invalid, '--output', bad, ok=False)
        assert not bad.exists() and not bad.with_suffix('.gppchartproj').exists()
    print('Multi-part CLI: deterministic sections, reopen/regenerate, scoped locks, reports and failures passed')
