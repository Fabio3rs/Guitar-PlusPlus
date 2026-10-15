#!/usr/bin/env python3
"""Paired A/B (alternating order), optional perf counters gated after warmup.

Both binaries must use identical flags and the same benchmark harness.
Use uninstrumented binaries with --perf; instrumented binaries without it.
"""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--runtime", type=Path, default=Path("build/bin"))
parser.add_argument("--seconds", default="3")
parser.add_argument("--repetitions", type=int, default=5)
parser.add_argument("--cpus", default="2,3")
parser.add_argument("--perf", help="Path to perf; never use with instrumented binaries")
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
binaries = {"before": args.before.resolve(), "after": args.after.resolve()}
# All cases draw HUD, with 20,000 authored notes/player. Cold load is excluded.
cases = [(1, 0, 1, 0, 1), (1, 1, 0, 0, 1), (2, 1, 1, 0, 1), (2, 1, 1, 50, 1)]
for repetition in range(args.repetitions):
    for case in cases:
        for version in (["before", "after"] if repetition % 2 == 0 else ["after", "before"]):
            name = "-".join(map(str, case)) + f"-r{repetition}-{version}"
            env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="1")
            command = ["xvfb-run", "-a", "taskset", "-c", args.cpus]
            fifos = []
            if args.perf:
                control, ack = output / (name + ".control"), output / (name + ".ack")
                for fifo in (control, ack):
                    os.mkfifo(fifo)
                    fifos.append(fifo)
                env.update(GPP_PERF_CONTROL=str(control), GPP_PERF_ACK=str(ack))
                command += [args.perf, "stat", "--delay=-1", f"--control=fifo:{control},{ack}",
                            "-x,", "-o", str(output / (name + ".csv")), "-e",
                            "cycles:u,instructions:u,cache-misses:u,L1-dcache-loads:u,L1-dcache-load-misses:u,context-switches,cpu-migrations,task-clock", "--"]
            command += [str(binaries[version]), args.seconds, *map(str, case)]
            try:
                with (output / (name + ".log")).open("w") as log:
                    subprocess.run(command, cwd=args.runtime, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, check=True, timeout=120)
            finally:
                for fifo in fifos:
                    fifo.unlink()
            print(name, flush=True)
