#!/usr/bin/env python3
"""Exercise actual ImGui/GLFW mouse routing under Xvfb, with xdotool."""
import json
import pathlib
import select
import subprocess
import sys
import time

exe = str(pathlib.Path(sys.argv[1]).resolve())
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
# A fresh layout makes coordinates reproducible without altering user layouts.
import tempfile
with tempfile.TemporaryDirectory(prefix="ui-", dir=output) as directory:
    with open(output / "gui-stderr.log", "w") as stderr:
        process = subprocess.Popen([exe, "--scene", "synthetic", "--output-dir", directory,
                                    "--rpc-stdio"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=stderr, text=True, bufsize=1)
        try:
            def request(method, params=None):
                process.stdin.write(json.dumps({"jsonrpc": "2.0", "id": method, "method": method, "params": params or {}}) + "\n")
                process.stdin.flush()
                assert select.select([process.stdout], [], [], 10)[0], "RPC timeout"
                result = json.loads(process.stdout.readline())
                assert "error" not in result, result
                return result["result"]

            request("state")
            window = subprocess.check_output(["xdotool", "search", "--sync", "--name", "^Guitar\\+\\+ Scene Studio$"], text=True).splitlines()[-1]
            time.sleep(.3)

            def mouse(*args):
                subprocess.run(["xdotool", *map(str, args)], check=True, timeout=5)
                time.sleep(.08)

            before = request("state")
            # Drag starts in the central viewport.
            mouse("mousemove", "--window", window, 600, 300)
            mouse("mousedown", 1)
            mouse("mousemove", "--window", window, 680, 340)
            mouse("mouseup", 1)
            orbit = request("state")
            assert orbit["eye"] != before["eye"]
            assert orbit["target"] == before["target"]
            assert orbit["shadow_updates"] == before["shadow_updates"]
            mouse("mousedown", 2)
            mouse("mousemove", "--window", window, 700, 360)
            mouse("mouseup", 2)
            pan = request("state")
            assert pan["target"] != orbit["target"]
            mouse("click", 4)
            zoom = request("state")
            assert zoom["eye"] != pan["eye"]
            # A click/drag in the explorer must not orbit the scene.
            mouse("mousemove", "--window", window, 80, 300)
            mouse("mousedown", 1)
            mouse("mousemove", "--window", window, 110, 330)
            mouse("mouseup", 1)
            assert request("state")["eye"] == zoom["eye"]
            request("capture", {"path": "interactive.png"})
            # Preserve capture outside the temporary layout directory.
            for filename in ("interactive.png", "interactive.png.json"):
                (output / filename).write_bytes((pathlib.Path(directory) / filename).read_bytes())
            request("shutdown")
            assert process.wait(timeout=10) == 0
            print(json.dumps({"orbit": "passed", "pan": "passed", "zoom": "passed", "panel_input_isolation": "passed"}))
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
