#!/usr/bin/env python3
"""Run an ARMC example repeatedly and emit one JSON result per run."""

import argparse
import json
import subprocess
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
# The complement, over valid configurations s[ifdehro]*, of the source bad automaton s[ifdeo]*r[ifdro]*h[ifdero]*.
RTL_SAFE = (
    "s[ifdeo]*"
    "|s[ifdeo]*h[ifdehro]*"
    "|s[ifdeo]*r[ifdro]*"
    "|s[ifdeo]*r[ifdro]*e[ifdehro]*"
    "|s[ifdeo]*r[ifdro]*h[ifdero]*h[ifdehro]*"
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


def run(
    mata: str,
    model: str,
    run_number: int,
    abstraction: str,
    predicate_seed: str,
    predicate_granularity: str,
    verification_direction: str,
    reduce_before_collapse: bool,
    reduction_min_states: int,
    timeout: float,
) -> dict[str, object]:
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
        "--param",
        f"abstraction={abstraction}",
        "--param",
        f"predicate-seed={predicate_seed}",
        "--param",
        f"predicate-granularity={predicate_granularity}",
        "--param",
        f"verification-direction={verification_direction}",
        "--param",
        f"reduce-before-collapse={'true' if reduce_before_collapse else 'false'}",
        "--param",
        f"reduction-min-states={reduction_min_states}",
        str(HERE / "armc.matasc"),
        str(HERE / f"{model}.mata"),
    ]
    started = time.perf_counter()
    try:
        completed = subprocess.run(command, capture_output=True, text=True, check=False, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout.decode() if isinstance(error.stdout, bytes) else (error.stdout or "")
        stderr = error.stderr.decode() if isinstance(error.stderr, bytes) else (error.stderr or "")
        completed = subprocess.CompletedProcess(command, 124, stdout, stderr)
    elapsed = time.perf_counter() - started
    lines = completed.stdout.splitlines()
    verdict = "TIMEOUT" if completed.returncode == 124 else (lines[-1] if lines else "")
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
        "abstraction": abstraction,
        "instrument": True,
        "predicate_seed": predicate_seed,
        "model": model,
        "predicate_granularity": predicate_granularity,
        "reduce_before_collapse": reduce_before_collapse,
        "reduction_min_states": reduction_min_states,
        "records": records,
        "returncode": completed.returncode,
        "run": run_number,
        "seconds": elapsed,
        "verification_direction": verification_direction,
        "stderr": completed.stderr,
        "verdict": verdict,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mata", default="mata", help="mata executable (default: mata on PATH)")
    parser.add_argument("--model", choices=MODELS, default="rtl")
    parser.add_argument("--runs", type=positive_int, default=1)
    parser.add_argument("--abstraction", choices=("length", "predicates"), default="length")
    parser.add_argument(
        "--predicate-seed",
        choices=("bad", "none", "actions", "bad-actions", "adaptive"),
        default="adaptive",
    )
    parser.add_argument("--predicate-granularity", choices=("states", "languages"), default="states")
    parser.add_argument("--verification-direction", choices=("forward", "backward"), default="forward")
    parser.add_argument("--reduce-before-collapse", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--reduction-min-states", type=int, default=32)
    parser.add_argument("--timeout", type=positive_int, default=60, help="per-run timeout in seconds")
    arguments = parser.parse_args()
    failed = False
    for run_number in range(arguments.runs):
        result = run(
            arguments.mata,
            arguments.model,
            run_number,
            arguments.abstraction,
            arguments.predicate_seed,
            arguments.predicate_granularity,
            arguments.verification_direction,
            arguments.reduce_before_collapse,
            arguments.reduction_min_states,
            arguments.timeout,
        )
        print(json.dumps(result, sort_keys=True))
        failed |= result["returncode"] != 0
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
