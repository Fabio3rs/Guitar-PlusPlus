#!/usr/bin/env python3
"""Run identical cases from runtime assets; logs stay outside source control."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--runtime", type=Path, default=Path("build/bin"))
parser.add_argument("--seconds", default="3")
parser.add_argument("--repetitions", type=int, default=5)
parser.add_argument("--cpus", default="2,3")
args = parser.parse_args()
binary = args.binary.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="1")
# player count, independent update, BPM lines, render delay in milliseconds
cases = [(1, 0, 0, 0), (1, 0, 1, 0), (1, 1, 0, 0), (1, 1, 1, 0),
         (1, 1, 0, 50), (1, 1, 1, 50), (2, 1, 0, 0), (2, 1, 1, 50)]
for repetition in range(args.repetitions):
    for case in cases:
        name = "-".join(map(str, case))
        with (output / f"{name}-r{repetition}.log").open("w") as log:
            subprocess.run(["taskset", "-c", args.cpus, "xvfb-run", "-a", str(binary),
                            args.seconds, *map(str, case)], cwd=args.runtime,
                           env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        print(name, repetition, flush=True)
