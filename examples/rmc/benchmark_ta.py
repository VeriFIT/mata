#!/usr/bin/env python3
"""Run ranked bottom-up tree protocol benchmarks and emit one JSON object per model/mode/run."""

import argparse
import json
import subprocess
import time
from pathlib import Path

EXPECTED = {
    "token-passing": {
        "predicates": "safe, predicate refinements 0",
        "height": "safe, height 0",
    },
    "token-copying": {
        "predicates": "unsafe, steps: 1, predicate refinements 0",
        "height": "unsafe, steps: 1, height 0",
    },
    "percolate": {
        "predicates": "safe, predicate refinements 0",
        "height": "safe, height 1",
    },
}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mata", default=str(root / "build/release/g++/examples/rmc-ta-token"))
    parser.add_argument("--directory", type=Path, default=root / "examples/rmc")
    parser.add_argument("--runs", type=int, default=15)
    parser.add_argument(
        "--reduction-min-states",
        default="16",
        help="Simulation threshold, 0 for always, or off",
    )
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be at least 1")

    failed = False
    for run in range(args.runs):
        started = time.perf_counter()
        completed = subprocess.run(
            [args.mata, str(args.directory)]
            + ([] if args.reduction_min_states is None else [str(args.reduction_min_states)]),
            capture_output=True,
            text=True,
            check=False,
        )
        elapsed = time.perf_counter() - started
        lines = completed.stdout.splitlines()
        failed |= completed.returncode != 0
        cursor = 0
        while cursor < len(lines):
            timings = {}
            verdict = lines[cursor] if cursor < len(lines) else ""
            cursor += 1
            for name in ("post", "collapse", "reduction", "inclusion", "backtrack"):
                if cursor < len(lines):
                    label, _, value = lines[cursor].partition(": ")
                    timings[label] = float(value) if label == name else None
                cursor += 1
            model = verdict.split(": ", 1)[0] if ": " in verdict else ""
            actual = verdict.split(": ", 1)[1] if ": " in verdict else ""
            expected_verdicts = EXPECTED.get(model, {})
            failed |= actual not in expected_verdicts.values()
            mode = next(
                (mode for mode, expected in expected_verdicts.items() if actual == expected),
                "unknown",
            )
            print(
                json.dumps(
                    {
                        "run": run,
                        "model": model,
                        "mode": mode,
                        "verdict": actual,
                        "seconds": elapsed,
                        "returncode": completed.returncode,
                        "stderr": completed.stderr,
                        "records": timings,
                    },
                    sort_keys=True,
                )
            )
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
