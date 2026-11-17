"""Alternate baseline and patch and save all native timings as CSV."""
import csv
import random
import shlex
import statistics
import subprocess
import sys
from pathlib import Path

baseline, patch, workload_dir, output = sys.argv[1:5]
prefix = sys.argv[5:]
if prefix:
    prefix = [*prefix, "shell"]
workloads = Path(workload_dir)
rng = random.Random(19610)
metrics = [(new_file, mode) for new_file in sorted(workloads.glob("*.js"))
           for mode in ("compile", "eval")]
if not metrics:
    raise ValueError("no JavaScript workloads")

with Path(output).open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("workload", "mode", "round", "sample", "build", "ms", "gc"))
    for workload, mode in metrics:
        pairs = []
        for round_id in range(10):
            timings = {}
            builds = [("base", baseline), ("patch", patch)]
            if round_id % 2:
                builds.reverse()
            for label, binary in builds:
                workload_path = workload.resolve() if not prefix else binary.rsplit("/", 1)[0] + "/" + workload.name
                args = [binary, str(workload_path), mode, "2", "5"]
                command = [*prefix, shlex.join(args)] if prefix else args
                rows = subprocess.check_output(command, text=True).splitlines()
                assert len(rows) == 5, rows
                timings[label] = []
                for sample, row in enumerate(rows):
                    ms, gc = row.split(",")
                    assert gc == "1", row
                    timings[label].append(float(ms))
                    writer.writerow((workload.stem, mode, round_id, sample, label, ms, gc))
            pairs.append(tuple((statistics.median(timings[label]) for label in ("base", "patch"))))
            stream.flush()
        baseline_ms, patch_ms = map(statistics.median, zip(*pairs))
        deltas = [a - b for a, b in pairs]
        boot = sorted(statistics.median(rng.choices(deltas, k=len(deltas))) for _ in range(6000))
        print(f"{workload.stem:8} {mode:7} {baseline_ms:7.3f} → {patch_ms:7.3f} ms"
              f" | paired {statistics.median(deltas):+7.3f} ms"
              f" | CI95 {boot[150]:+7.3f} … {boot[5849]:+7.3f}",
              flush=True)
