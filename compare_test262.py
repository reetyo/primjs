"""Compare failed Test262 cases from two upstream runner reports."""
from collections import Counter
from pathlib import Path
import re
import sys


def read_failures(report):
    failures = Counter()
    case = None
    for line in Path(report).read_text(errors="replace").splitlines():
        match = re.match(r"^\d+: .*?test262/test/(.*)$", line)
        if match:
            case = match[1]
        elif line.strip() == "FAILED":
            if not case:
                raise ValueError(f"missing test path in {report}")
            failures[case] += 1
    return failures


baseline, patched = map(read_failures, sys.argv[1:3])
print(f"baseline: {sum(baseline.values())} errors in {len(baseline)} tests")
print(f"patch:    {sum(patched.values())} errors in {len(patched)} tests")
print("new:", dict(patched - baseline))
print("removed:", dict(baseline - patched))
raise SystemExit(bool(baseline != patched))
