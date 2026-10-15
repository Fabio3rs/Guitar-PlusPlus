#!/usr/bin/env python3
"""Fetch only the x64 Windows BASS SDK/runtime files used by CI."""
import argparse
import io
from pathlib import Path
import struct
import urllib.request
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=Path("bass"))
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    packages = (
        ("https://www.un4seen.com/files/bass24.zip", {
            "c/bass.h": "bass.h", "c/x64/bass.lib": "bass.lib",
            "x64/bass.dll": "bass.dll", "bass.txt": "bass.txt",
        }),
        ("https://www.un4seen.com/files/z/0/bass_fx24.zip", {
            "C/bass_fx.h": "bass_fx.h", "C/x64/bass_fx.lib": "bass_fx.lib",
            "x64/bass_fx.dll": "bass_fx.dll", "bass_fx.txt": "bass_fx.txt",
        }),
    )
    for url, files in packages:
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for source, destination in files.items():
                content = archive.read(source)
                if destination.endswith(".dll"):
                    pe_offset = struct.unpack_from("<I", content, 0x3C)[0]
                    if (content[:2] != b"MZ" or content[pe_offset:pe_offset + 4] != b"PE\0\0"
                            or struct.unpack_from("<H", content, pe_offset + 4)[0] != 0x8664):
                        raise RuntimeError(f"Not an x64 PE DLL: {source}")
                (args.destination / destination).write_bytes(content)
                print(f"Installed {destination}")


if __name__ == "__main__":
    main()
