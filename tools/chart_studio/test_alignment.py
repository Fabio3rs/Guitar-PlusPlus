#!/usr/bin/env python3
"""CPU/headless BASS chroma -> alignment -> export with synthetic known timing."""
import json
import math
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import wave
from fixture import vlq

exe = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.pop("DISPLAY", None)


def run(*args, code=0):
    result = subprocess.run([str(exe), *map(str, args)], capture_output=True,
                            text=True, env=env, timeout=90)
    assert result.returncode == code, result.stdout + result.stderr
    return result


with tempfile.TemporaryDirectory(prefix="alignment-", dir=root) as tmp:
    tmp = Path(tmp)
    midi, audio = tmp / "score.mid", tmp / "audio.wav"
    rng = random.Random(19)
    notes = [rng.randrange(55, 79) for _ in range(50)]
    events = [(0, b"\xff\x51\x03\x07\xa1\x20"), (0, b"\xc0\x1b")]
    for i, pitch in enumerate(notes):
        tick = i * 720 + 480
        events.extend([(tick, bytes([0x90, pitch, 100])),
                       (tick + 480, bytes([0x80, pitch, 0]))])
    data = bytearray()
    previous = 0
    for tick, event in sorted(events, key=lambda e: e[0]):
        data.extend(vlq(tick - previous) + event)
        previous = tick
    data.extend(b"\0\xff\x2f\0")
    midi.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(data)) + data)
    scale, offset, rate = 1.016, 1.7, 22050
    pcm = bytearray(int((40 * scale + offset) * rate) * 2)
    for i, pitch in enumerate(notes):
        begin = int(((.5 + i * .75) * scale + offset) * rate)
        length = int(.5 * scale * rate)
        hz = 440 * 2 ** ((pitch - 69) / 12)
        for j in range(length):
            envelope = min(1, j / 150, (length - j) / 150)
            value = int(10000 * envelope * math.sin(2 * math.pi * hz * j / rate))
            struct.pack_into("<h", pcm, (begin + j) * 2, value)
    with wave.open(str(audio), "wb") as wav:
        wav.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        wav.writeframes(pcm)
    artifact = tmp / "alignment.json"
    args = ["align", midi, "--audio", audio, "--align", "auto", "--align-min-offset", -3,
            "--align-max-offset", 4]
    result = json.loads(run(*args, "--output", artifact).stdout)["alignment"]
    assert result["reliable"], result
    best = result["candidates"][0]
    assert abs(best["offset"] - offset) < .12, best  # FFT window / grid resolution
    assert abs(best["scale"] - scale) < .003, best
    assert json.loads(run(*args).stdout)["alignment"] == result
    before = audio.read_bytes()
    chart = tmp / "aligned.chart"
    convert = ["convert", midi, "--track", 0, "--audio", audio, "--alignment", artifact,
               "--output", chart]
    run(*convert)
    assert audio.read_bytes() == before
    text = chart.read_bytes()
    run(*convert, "--replace")
    assert chart.read_bytes() == text
    assert b" = B 120000" not in text  # Scale changes SyncTrack, not MIDI ticks.
    # Simulate an explicitly ambiguous artifact: conversion must not silently apply it.
    result["reliable"] = False
    artifact.write_text(json.dumps({"alignment": result}))
    rejected = tmp / "ambiguous.chart"
    run("convert", midi, "--track", 0, "--alignment", artifact, "--output", rejected, code=4)
    assert not rejected.exists() and not rejected.with_suffix(".gppchartproj").exists()
    run("convert", midi, "--track", 0, "--alignment", artifact, "--alignment-candidate", 0,
        "--output", rejected)
    run("convert", midi, "--track", 0, "--align", "dtw", "--audio", audio,
        "--output", tmp / "dtw.chart", code=1)
    run(*args, "--output", audio, "--replace", code=1)
    assert audio.read_bytes() == before
    result["audioHash"] = "wrong"
    artifact.write_text(json.dumps({"alignment": result}))
    run(*convert, "--replace", code=1)
    assert chart.read_bytes() == text
    print("Alignment: known offset/scale, deterministic replay, runtime round-trip, ambiguity gate and source protection passed")
