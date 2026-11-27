#!/usr/bin/env python3
"""Run ranked bottom-up tree protocol benchmarks and emit one JSON object per model/mode/run."""

import argparse
import json
import math
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
PHASES = ("post", "collapse", "reduction", "inclusion", "backtrack")


def positive_int(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return number


def read_number(lines: list[str], position: int, label: str) -> float | None:
    if position >= len(lines):
        return None
    actual_label, separator, value = lines[position].partition(": ")
    if separator != ": " or actual_label != label:
        return None
    try:
        number = float(value)
    except ValueError:
        return None
    return number if math.isfinite(number) and number >= 0 else None


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mata", default=str(root / "build/release/g++/examples/rmc-ta-token"))
    parser.add_argument("--directory", type=Path, default=root / "examples/rmc")
    parser.add_argument("--runs", type=positive_int, default=15)
    parser.add_argument("--timeout", type=positive_int, default=60, help="per-batch timeout in seconds")
    parser.add_argument(
        "--reduction-min-states",
        default="16",
        help="Simulation threshold, 0 for always, or off",
    )
    args = parser.parse_args()

    failed = False
    for run in range(args.runs):
        command = [args.mata, str(args.directory)]
        if args.reduction_min_states is not None:
            command.append(str(args.reduction_min_states))
        started = time.perf_counter()
        timed_out = False
        try:
            completed = subprocess.run(
                command,
                capture_output=True,
                text=True,
                check=False,
                timeout=args.timeout,
            )
        except subprocess.TimeoutExpired as error:
            timed_out = True
            stdout = error.stdout.decode() if isinstance(error.stdout, bytes) else (error.stdout or "")
            stderr = error.stderr.decode() if isinstance(error.stderr, bytes) else (error.stderr or "")
            completed = subprocess.CompletedProcess(command, 124, stdout, stderr)
        process_seconds = time.perf_counter() - started
        lines = completed.stdout.splitlines()
        cursor = 0
        malformed = False
        for mode in ("predicates", "height"):
            for model, expected_verdicts in EXPECTED.items():
                expected_verdict = expected_verdicts[mode]
                prefix = f"{model}: "
                if timed_out:
                    verdict = "TIMEOUT"
                elif cursor < len(lines) and lines[cursor].startswith(prefix):
                    verdict = lines[cursor][len(prefix) :]
                else:
                    verdict = ""
                records = {name: read_number(lines, cursor + offset + 1, name) for offset, name in enumerate(PHASES)}
                seconds = read_number(lines, cursor + 6, "seconds")
                record_failed = (
                    completed.returncode != 0
                    or verdict != expected_verdict
                    or seconds is None
                    or any(value is None for value in records.values())
                )
                malformed |= record_failed
                print(
                    json.dumps(
                        {
                            "run": run,
                            "model": model,
                            "mode": mode,
                            "verdict": verdict,
                            "seconds": seconds,
                            "process_seconds": process_seconds,
                            "returncode": completed.returncode,
                            "stderr": completed.stderr,
                            "records": records,
                            "failed": record_failed,
                        },
                        sort_keys=True,
                    )
                )
                cursor += 7
        malformed |= cursor != len(lines)
        failed |= malformed
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
