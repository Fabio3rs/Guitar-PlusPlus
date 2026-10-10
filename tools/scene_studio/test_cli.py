#!/usr/bin/env python3
"""Asset-independent CLI/RPC/visual integration tests; run inside NativeLab.
No Pillow dependency. PNG decoding below covers libpng's 8-bit RGBA output.
"""
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib

exe = str(pathlib.Path(sys.argv[1]).resolve())
output = pathlib.Path(sys.argv[2]).resolve()
backend = sys.argv[3] if len(sys.argv) > 3 else "glfw"
output.mkdir(parents=True, exist_ok=True)


def png(path):
    data = pathlib.Path(path).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    offset, compressed = 8, b""
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind, payload = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        assert zlib.crc32(kind + payload) & 0xffffffff == struct.unpack_from(">I", data, offset + 8 + size)[0]
        if kind == b"IHDR":
            w, h, bits, color, _, _, interlaced = struct.unpack(">IIBBBBB", payload)
            assert (bits, color, interlaced) == (8, 6, 0)
        if kind == b"IDAT":
            compressed += payload
        offset += size + 12
    raw = zlib.decompress(compressed)
    stride, pixels, previous = w * 4, bytearray(), bytearray(w * 4)
    for y in range(h):
        kind = raw[y * (stride + 1)]
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a, b, c = row[x - 4] if x >= 4 else 0, previous[x], previous[x - 4] if x >= 4 else 0
            if kind == 1:
                predictor = a
            elif kind == 2:
                predictor = b
            elif kind == 3:
                predictor = (a + b) // 2
            elif kind == 4:
                p = a + b - c
                distances = [abs(p - a), abs(p - b), abs(p - c)]
                predictor = [a, b, c][distances.index(min(distances))]
            else:
                assert kind == 0
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return w, h, pixels


with tempfile.TemporaryDirectory(prefix="isolation-", dir=output) as temporary:
    cwd = pathlib.Path(temporary)
    (cwd / "data/saves").mkdir(parents=True)
    (cwd / "data/saves/settings.json").write_text("personal sentinel")
    (cwd / "Config.lua").write_text("error('Studio must not execute game config')")
    protected = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in cwd.rglob("*") if p.is_file()}
    common = [exe, "--mode", "headless", "--backend", backend, "--scene", "synthetic",
              "--output-dir", str(output), "--width", "320", "--height", "240", "--json"]

    def run(*args, success=True):
        result = subprocess.run(common + list(args), cwd=cwd, text=True, capture_output=True, timeout=40)
        assert (result.returncode == 0) == success, (args, result.stdout, result.stderr)
        value = json.loads(result.stdout)
        assert "Scene Studio:" in result.stderr
        return value

    base = run("--capture", "base.png")
    assert base["ao_active"] and base["shadows_active"]
    if backend == "glfw":
        assert base["acceleration"] == "software", base["renderer"]
        rejected = run("--require-hardware", success=False)
        assert rejected["code"] == "hardware_required"
    else:
        assert base["acceleration"] == "hardware", base["renderer"]
    assert json.loads(pathlib.Path(base["metadata"]).read_text()) == base
    width, height, reference = png(base["capture"])
    assert (width, height) == (320, 240)
    assert len(set(reference)) > 50
    comparisons = {}
    for name, args in {
        "no-shadow": ["--quality", "1"], "no-ao": ["--ao", "off"],
        "camera": ["--eye", "-6", "5", "7"], "light": ["--light", "-2", "4", "1"],
        "bias": ["--shadow-bias", ".02"], "quality3": ["--quality", "3"],
    }.items():
        result = run("--capture", name + ".png", *args)
        _, _, pixels = png(result["capture"])
        difference = sum(abs(a - b) for a, b in zip(reference, pixels)) / len(reference)
        changed = sum(a != b for a, b in zip(reference, pixels))
        assert changed > 20 and difference > .001, (name, changed, difference)
        if name in ("no-shadow", "no-ao"):
            assert sum(pixels) > sum(reference), (name, "expected brighter image")
        comparisons[name] = {"mean_absolute_difference": difference, "changed_channels": changed}
    resized = run("--width", "127", "--height", "95", "--capture", "resized.png")
    assert png(resized["capture"])[:2] == (127, 95)
    bench = run("--benchmark", "4", "--warmup", "2")
    assert bench["median_ms"] > 0 and bench["frames"] == 4
    for args in [("--width", "0"), ("--shadow-bias", "NaN"), ("--quality", "4"),
                 ("--eye", "0", "1", "0"), ("--capture", "../escape.png")]:
        run(*args, success=False)

    # Persistent commands share one context and retain a scene after errors.
    requests = [
        {"method": "capture", "params": {"path": "rpc-a.png"}},
        {"method": "set", "params": {"light": [-2, 4, 1]}},
        {"method": "capture", "params": {"path": "rpc-b.png"}},
        {"method": "load", "params": {"kind": "model", "source": "missing.obj"}},
        {"method": "state"}, {"method": "shutdown"},
    ]
    wire = "\n".join(json.dumps(dict(item, jsonrpc="2.0", id=i)) for i, item in enumerate(requests)) + "\n"
    response = subprocess.run(common + ["--rpc-stdio"], input=wire, text=True, capture_output=True, cwd=cwd, timeout=40)
    assert response.returncode == 0, response.stderr
    replies = [json.loads(line) for line in response.stdout.splitlines()]
    assert len(replies) == len(requests), replies
    assert replies[2]["result"]["shadow_updates"] == replies[0]["result"]["shadow_updates"] + 1
    assert "error" in replies[3] and replies[4]["result"]["scene"] == "synthetic"
    assert replies[-1]["result"]["stopping"]
    assert png(output / "rpc-a.png")[2] != png(output / "rpc-b.png")[2]
    malformed = "x" * 70000 + "\n{bad}\n" + json.dumps({"jsonrpc": "2.0", "id": 99, "method": "shutdown"}) + "\n"
    recovered = subprocess.run(common + ["--rpc-stdio"], input=malformed, text=True, capture_output=True, cwd=cwd, timeout=40)
    assert recovered.returncode == 0, recovered.stderr
    messages = [json.loads(line) for line in recovered.stdout.splitlines()]
    assert [message.get("error", {}).get("code") for message in messages[:2]] == [-32600, -32700]
    assert messages[-1]["result"]["stopping"]
    for path, digest in protected.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
    assert set(p for p in cwd.rglob("*") if p.is_file()) == set(protected)
    report = {"backend": backend, "renderer": base["renderer"], "acceleration": base["acceleration"],
              "comparisons": comparisons, "isolation": "passed", "rpc": "passed", "png": "passed"}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
