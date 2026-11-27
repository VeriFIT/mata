#!/usr/bin/env python3
"""Run an ARMC example repeatedly and emit one JSON result per run."""

import argparse
import json
import subprocess
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
RTL_SAFE = (
    "s[ifdeo]*"
    "|s[ifdeor]*h[ifdehro]*"
    "|s[ifdeor]*r[ifdro]*"
    "|s[ifdeor]*r[ifdro]*e[ifdehro]*"
    "|s[ifdeor]*r[ifdro]*h[ifdero]*h[ifdehro]*"
)
MODELS = {
    "token-passing": ("tn*", "n*tn*", "n t"),
    "token-copying": ("tn*", "n*tn*", "n t"),
    "burns": ("0*", "[0-4]*(5[0-4]*)?", "0 1 2 3 4 5"),
    "burns-broken": ("0*", "[0-4]*(5[0-4]*)?", "0 1 2 3 4 5"),
    "rtl": ("si*", RTL_SAFE, "s i f d e h r o"),
}


def positive_int(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return number


def run(mata: str, model: str, run_number: int) -> dict[str, object]:
    initial, safe, symbols = MODELS[model]
    command = [
        mata,
        "script",
        "run",
        "--param",
        f"initial-regex={initial}",
        "--param",
        f"bad-regex={safe}",
        "--param",
        f"symbols={symbols}",
        "--param",
        "instrument=true",
        str(HERE / "armc.matasc"),
        str(HERE / f"{model}.mata"),
    ]
    started = time.perf_counter()
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    elapsed = time.perf_counter() - started
    lines = completed.stdout.splitlines()
    verdict = lines[-1] if lines else ""
    records: dict[str, int | float | str] = {}
    for line in lines[:-1]:
        label, separator, value = line.partition(": ")
        if separator:
            try:
                records[label] = int(value)
            except ValueError:
                try:
                    records[label] = float(value)
                except ValueError:
                    records[label] = value
    return {
        "instrument": True,
        "model": model,
        "records": records,
        "returncode": completed.returncode,
        "run": run_number,
        "seconds": elapsed,
        "stderr": completed.stderr,
        "verdict": verdict,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mata", default="mata", help="mata executable (default: mata on PATH)")
    parser.add_argument("--model", choices=MODELS, default="rtl")
    parser.add_argument("--runs", type=positive_int, default=1)
    arguments = parser.parse_args()
    failed = False
    for run_number in range(arguments.runs):
        result = run(arguments.mata, arguments.model, run_number)
        print(json.dumps(result, sort_keys=True))
        failed |= result["returncode"] != 0
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
