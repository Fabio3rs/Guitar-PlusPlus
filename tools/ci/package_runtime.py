#!/usr/bin/env python3
"""Install only the Runtime component, then ZIP it with Unix mode bits intact."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--platform", choices=("linux", "windows"), required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    name = f"GuitarPP-{args.platform}-x64-RelWithDebInfo"
    output = args.output_dir.resolve() / f"{name}.zip"
    with tempfile.TemporaryDirectory(prefix="guitarpp-package-") as work:
        stage = Path(work) / name
        subprocess.run([
            "cmake", "--install", str(args.build_dir.resolve()),
            "--config", "RelWithDebInfo", "--component", "Runtime",
            "--prefix", str(stage),
        ], check=True)
        if not (stage / "Config.lua").is_file():
            raise RuntimeError("Runtime component was not installed; enable GPP_PACKAGE_RUNTIME")
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(stage.parent))
    print(output)


if __name__ == "__main__":
    main()
