#!/usr/bin/env python3
"""Check the downloaded ZIP layout and exercise its relocated chart converter."""
import argparse
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    windows = sys.platform == "win32"
    suffix = ".exe" if windows else ""
    executables = ("GuitarPP", "gpp_chart_studio", "gpp_score_to_chart",
                   "gpp_dump_chart", "gpp_scan_library")
    with tempfile.TemporaryDirectory(prefix="guitarpp-relocated-") as work:
        work = Path(work)
        with zipfile.ZipFile(args.archive) as archive:
            assert archive.testzip() is None, "ZIP CRC failure"
            archive.extractall(work)
            # Python's extractor does not restore ZIP executable permission bits.
            for entry in archive.infolist():
                if not windows:
                    (work / entry.filename).chmod(entry.external_attr >> 16)
        root = work / args.archive.stem
        required = ["Config.lua", "README.txt", "licenses/bass.txt", "licenses/bass_fx.txt",
                    "data/themes/gppdefaulttheme/Theme.lua", "data/scenes/garage.lua"]
        required.extend(name + suffix for name in executables)
        if windows:
            required.extend(["bass.dll", "bass_fx.dll"])
            required.extend(name + ".pdb" for name in executables)
        else:
            required.extend(["lib/libbass.so", "lib/libbass_fx.so"])
        for name in required:
            assert (root / name).is_file(), f"Missing {name}"
        for name in ("songs", "saves", "cache"):
            assert not (root / "data" / name).exists(), f"Unexpected {name}"
        if not windows:
            for name in executables:
                executable = root / name
                assert executable.stat().st_mode & stat.S_IXUSR, f"Not executable: {name}"
                dynamic = subprocess.check_output(["readelf", "-d", executable], text=True)
                assert "$ORIGIN/lib" in dynamic, f"Missing relative library path: {name}"
                for line in dynamic.splitlines():
                    if "RPATH" in line or "RUNPATH" in line:
                        assert line.split("[")[1].split("]")[0] == "$ORIGIN/lib", line
                env = os.environ.copy()
                env.pop("LD_LIBRARY_PATH", None)
                dependencies = subprocess.check_output(["ldd", executable], env=env, text=True)
                assert "not found" not in dependencies, dependencies
                for line in dependencies.splitlines():
                    if any(lib in line for lib in ("libbass", "libGLEW", "libsqlite3", "libpng", "libz.so")):
                        assert str(root / "lib") in line, line
        cli_test = Path(__file__).resolve().parents[1] / "chart_studio" / "test_cli.py"
        env = os.environ.copy()
        env.pop("LD_LIBRARY_PATH", None)
        subprocess.run([sys.executable, str(cli_test),
                        str(root / ("gpp_score_to_chart" + suffix)), str(work / "conversion")],
                       cwd=root, env=env, check=True, timeout=180)
    print("Runtime ZIP layout, libraries and relocated chart conversion passed")


if __name__ == "__main__":
    main()
