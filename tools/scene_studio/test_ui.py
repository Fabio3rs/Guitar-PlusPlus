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
            # A click selects without orbiting; the UI and RPC share selection.
            request("reset")
            request("set", {"eye": [0, .7, 5], "target": [0, .7, 0]})
            mouse("windowfocus", "--sync", window)
            mouse("mousemove", "--window", window, 630, 335)
            initial = request("state")
            mouse("click", 1)
            clicked = request("state")
            assert clicked["selected"] == 1, clicked
            assert clicked["eye"] == initial["eye"]
            mouse("key", "f")
            focused = request("state")
            assert focused["target"] != initial["target"] or focused["eye"] != initial["eye"]
            # Double-click changes the pivot without moving the eye.
            request("set", {"eye": [0, .7, 5], "target": [0, .7, 0]})
            mouse("click", "--repeat", 2, "--delay", 100, 1)
            pivot = request("state")
            assert pivot["eye"] == initial["eye"] and pivot["target"] != initial["target"]
            # Captured free look and physical keyboard input.
            request("set", {"navigation": "free", "speed": 4})
            mouse("mousedown", 3)
            mouse("keydown", "w")
            time.sleep(.2)
            moved = request("state")
            assert moved["eye"] != pivot["eye"]
            mouse("key", "Escape")
            escaped = request("state")
            time.sleep(.15)
            assert request("state")["eye"] == escaped["eye"]
            mouse("keyup", "w")
            mouse("mouseup", 3)
            # Focus loss must release capture, even while the movement key is held.
            mouse("mousemove", "--window", window, 630, 335)
            mouse("mousedown", 3)
            mouse("keydown", "w")
            import ctypes
            x11 = ctypes.CDLL("libX11.so.6")
            x11.XOpenDisplay.restype = ctypes.c_void_p
            x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
            x11.XDefaultRootWindow.restype = ctypes.c_ulong
            x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
            x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
            x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
            display = x11.XOpenDisplay(None)
            assert display
            try:
                x11.XSetInputFocus(display, x11.XDefaultRootWindow(display), 1, 0)
                x11.XSync(display, 0)
            finally:
                x11.XCloseDisplay(display)
            time.sleep(.15)
            unfocused = request("state")
            time.sleep(.15)
            assert request("state")["eye"] == unfocused["eye"]
            mouse("keyup", "w")
            mouse("mouseup", 3)
            mouse("windowfocus", "--sync", window)
            # Source text entry must not trigger free-camera motion or focus hotkeys.
            mouse("mousemove", "--window", window, 80, 75)
            mouse("click", 1)
            before_text = request("state")
            mouse("type", "wasdqef")
            assert request("state")["eye"] == before_text["eye"]
            request("reset")
            request("capture", {"path": "interactive.png"})
            # Preserve capture outside the temporary layout directory.
            for filename in ("interactive.png", "interactive.png.json"):
                (output / filename).write_bytes((pathlib.Path(directory) / filename).read_bytes())
            request("shutdown")
            assert process.wait(timeout=10) == 0
            print(json.dumps({"orbit": "passed", "pan": "passed", "zoom": "passed", "panel_input_isolation": "passed", "selection": "passed", "free_camera": "passed", "focus_loss": "passed"}))
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
