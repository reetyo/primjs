"""Measure a four-way lookahead comparison with balanced rotation."""
import argparse
import csv
import os
from pathlib import Path
import random
import shlex
from statistics import median
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("workloads", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--adb", nargs=2, metavar=("PATH", "SERIAL"))
parser.add_argument("--remote-dir", default="/data/local/tmp/primjs-public-lookahead")
parser.add_argument("--rounds", type=int, default=8)
parser.add_argument("--samples", type=int, default=3)
args = parser.parse_args()
labels = ("off", "wrappers", "token-kinds", "both")
orders = ((0, 1, 3, 2), (1, 2, 0, 3), (2, 3, 1, 0), (3, 0, 2, 1))
rng = random.Random(19610)
with args.output.open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("workload", "mode", "round", "sample", "features", "ms", "gc"))
    for path in sorted(args.workloads.glob("*.js")):
        for mode in ("compile", "eval"):
            rounds = []
            for index in range(args.rounds):
                measurements = {}
                for features in orders[index % len(orders)]:
                    if args.adb:
                        adb, serial = args.adb
                        source = f"{args.remote_dir}/{path.name}"
                        cmd = [adb, "-s", serial, "shell", shlex.join([
                            f"PRIMJS_BENCH_LOOKAHEAD={features}", args.binary,
                            source, mode, "1", str(args.samples)])]
                        env = None
                    else:
                        cmd = [args.binary, str(path.resolve()), mode, "1", str(args.samples)]
                        env = dict(os.environ, PRIMJS_BENCH_LOOKAHEAD=str(features))
                    rows = subprocess.check_output(cmd, text=True, env=env).splitlines()
                    assert len(rows) == args.samples, rows
                    values = []
                    for sample, row in enumerate(rows):
                        ms, gc = row.split(",")
                        assert gc == "1", row
                        values.append(float(ms))
                        writer.writerow((path.stem, mode, index, sample, labels[features], ms, gc))
                    measurements[features] = median(values)
                rounds.append(measurements)
                stream.flush()
            for features in (1, 2, 3):
                delta = [x[0] - x[features] for x in rounds]
                resamples = sorted(median(rng.choices(delta, k=len(delta))) for _ in range(4000))
                before = median(x[0] for x in rounds)
                after = median(x[features] for x in rounds)
                print(
                    f"{path.stem} {mode} {labels[features]} "
                    f"{before:.3f} -> {after:.3f} ms, "
                    f"saving {median(delta):+.3f} ms "
                    f"[{resamples[100]:+.3f}, {resamples[3899]:+.3f}]",
                    flush=True,
                )
