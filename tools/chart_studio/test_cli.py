#!/usr/bin/env python3
"""Synthetic SMF -> persistent project -> second process -> runtime round trip."""
import json
import math
import wave
import os
from pathlib import Path
import struct
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
