#!/usr/bin/env python3
"""Compare the rejected ARMC backtracking and early-fixpoint variants."""

import argparse
import json
import subprocess
import time
from pathlib import Path

SAFE = (
    "s[ifdeo]*|s[ifdeor]*h[ifdehro]*|s[ifdeor]*r[ifdro]*"
    "|s[ifdeor]*r[ifdro]*e[ifdehro]*|s[ifdeor]*r[ifdro]*h[ifdero]*h[ifdehro]*"
)
MODELS = {
    "token-passing": ("tn*", "n*tn*", "n t"),
    "token-copying": ("tn*", "n*tn*", "n t"),
    "burns": ("0*", "[0-4]*(5[0-4]*)?", "0 1 2 3 4 5"),
    "burns-broken": ("0*", "[0-4]*(5[0-4]*)?", "0 1 2 3 4 5"),
    "rtl": ("si*", SAFE, "s i f d e h r o"),
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--mata", required=True)
parser.add_argument("--model", required=True, choices=MODELS)
parser.add_argument("--backtracking", choices=("eager", "eager-unminimized", "lazy"), default="eager")
parser.add_argument("--early-fixpoint", action="store_true")
parser.add_argument("--repeats", type=int, default=15)
parser.add_argument("--script", type=Path, help="alternate armc.matasc path when testing an archive")
args = parser.parse_args()
initial, safe, symbols = MODELS[args.model]
script = args.script or Path(__file__).resolve().parents[3] / "examples/rmc/armc.matasc"
step = script.parent / f"{args.model}.mata"
for run in range(args.repeats):
    command = [
        args.mata,
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
        "--param",
        f"backtracking={args.backtracking}",
        "--param",
        f"early-fixpoint={'true' if args.early_fixpoint else 'false'}",
        "--param",
        "reduce-before-collapse=false",
        str(script),
        str(step),
    ]
    started = time.perf_counter()
    try:
        completed = subprocess.run(command, capture_output=True, text=True, timeout=5, check=False)
        code = completed.returncode
        stdout, stderr = completed.stdout, completed.stderr
    except subprocess.TimeoutExpired as error:
        code, stdout, stderr = 124, error.stdout or "", error.stderr or ""
    lines = stdout.splitlines()
    verdict = "TIMEOUT" if code == 124 else (lines[-1] if lines else "")
    records = {}
    for line in lines[:-1]:
        label, separator, value = line.partition(": ")
        if separator:
            records[label] = value
    print(
        json.dumps(
            {
                "run": run,
                "model": args.model,
                "backtracking": args.backtracking,
                "early_fixpoint": args.early_fixpoint,
                "seconds": time.perf_counter() - started,
                "returncode": code,
                "verdict": verdict,
                "records": records,
                "stderr": stderr,
            },
            sort_keys=True,
        )
    )
