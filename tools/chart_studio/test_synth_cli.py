"""Offline optional-dependency and CLI regression checks; synthetic MIDI only."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import wave
from fixture import fixture

binary = str(Path(sys.argv[1]).resolve())
sf = Path(sys.argv[2]) if len(sys.argv) > 2 else None
with tempfile.TemporaryDirectory(prefix="gpp synth ' paths ") as temp:
    root = Path(temp)
    midi = root / "score.mid"
    fixture(midi)
    project = root / "score.gppchartproj"
    def run(*args, env=None):
        return subprocess.run([binary, *map(str, args)], text=True, capture_output=True, env=env)
    p = run("convert", midi, "--track", 0, "--project", project, "--output", root / "notes.chart")
    assert p.returncode == 0, p.stderr
    before = (root / "notes.chart").read_bytes()
    p = run("render-stems", project)
    if "synthesis unavailable" in p.stderr:
        assert p.returncode != 0
        print("Build without FluidSynth: explicit unavailable error; legacy conversion passed")
        sys.exit(0)
    if not sf or not sf.is_file():
        print("SoundFont absent: rendering integration skipped; legacy conversion passed")
        sys.exit(0)
    base = ["render-stems", project, "--soundfont", sf, "--sample-rate", "16000", "--tail", "0.1"]
    env = os.environ.copy()
    env["PATH"] = str(root / "no executables")
    p = run(*base, "--output-dir", root / "wav", "--save-config", env=env)
    assert p.returncode == 0, p.stderr
    with wave.open(str(root / "wav/guitar.wav")) as f:
        assert f.getframerate() == 16000 and f.getnchannels() == 2
        assert any(f.readframes(f.getnframes())), "SoundFont produced no audio"
    p = run(*base, "--format", "ogg", "--output-dir", root / "missing-ffmpeg", env=env)
    assert p.returncode != 0 and "FFmpeg" in p.stderr, p.stderr
    assert not list((root / "missing-ffmpeg").iterdir())
    fake = root / "bad encoder"
    fake.mkdir()
    shutil.copy2(sys.executable, fake / ("ffmpeg.exe" if os.name == "nt" else "ffmpeg"))
    env["PATH"] = str(fake)
    p = run(*base, "--format", "ogg", "--output-dir", root / "bad-ffmpeg", env=env)
    assert p.returncode != 0
    assert not list((root / "bad-ffmpeg").iterdir())
    p = run(*base, "--instruments", "custom", "--stem", "combined:0:0", "--stem", "combined:0:0", "--output-dir", root / "custom")
    assert p.returncode == 0, p.stderr
    assert (root / "custom/combined.wav").is_file()
    p = run(*base, "--instruments", "custom", "--stem", "song:0", "--output-dir", root / "protected")
    assert p.returncode != 0
    assert (root / "notes.chart").read_bytes() == before
    print("WAV without FFmpeg, missing/broken encoder cleanup, custom grouping, persistence and protection passed")
