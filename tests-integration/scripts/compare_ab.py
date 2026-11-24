#!/usr/bin/env python3
"""A/B comparison of two Mata builds on the same instances.

`jobs` merges the pycobench job files of a base and a head build into one, so that a single pycobench run
executes the base and the head of every method both define on an instance right after each other. `compare`
reads the pyco_proc CSVs of several such rounds, takes per-instance medians, and gates on the geometric mean of
the per-instance head/base ratios. Input it cannot pair up (no base/head columns, a missing run of a metric both
builds print, nothing measured by both) is an error with exit status 2.
"""

import argparse
import csv
import math
import statistics
import sys

import yaml

SIDES = ("base", "head")
# The cells pyco_proc writes instead of measurements, by the run status they stand for.
STATUS_CELLS = ("TO", "MO", "ERR", "CRASH")
# Peak memory in kB; every other numeric metric is a time in seconds.
MEMORY_METRICS = ("maxrss",)


class InputError(Exception):
    """Input that cannot be compared."""


def merge_jobs(args):
    """Prints a job file running each method of the base and the head build back to back."""
    configs = {}
    for side, path in zip(SIDES, (args.base_jobs, args.head_jobs)):
        with open(path) as jobs_file:
            config = yaml.safe_load(jobs_file)
        # Inline each side's defaults, as the two files need not share them.
        defaults = config.pop("defaults", None) or {}
        configs[side] = {method: {**defaults, **conf} for method, conf in config.items()}
    common = [method for method in configs["head"] if method in configs["base"]]
    if not common:
        raise InputError(f"{args.base_jobs} and {args.head_jobs} share no method")
    for side in SIDES:
        if only := sorted(configs[side].keys() - set(common)):
            print(f"note: not comparing the {side}-only methods {', '.join(only)}", file=sys.stderr)
    order = tuple(reversed(SIDES)) if args.head_first else SIDES
    merged = {}
    # pycobench runs the methods of an instance in this order, so each base run sits next to its head run.
    for method in common:
        for side in order:
            merged[f"{side}-{method}"] = configs[side][method]
    yaml.safe_dump(merged, sys.stdout, sort_keys=False)


def read_rounds(paths):
    """Returns {(engine, metric): {side: {instance: [cell per round]}}} from the pyco_proc CSVs."""
    cells = {}
    for path in paths:
        with open(path, newline="") as csv_file:
            reader = csv.DictReader(csv_file, delimiter=";")
            if not any(column.partition("-")[0] in SIDES for column in reader.fieldnames or ()):
                raise InputError(f"{path} has no base-/head- columns; expected the CSV of a merged job file")
            for row in reader:
                instance = row.pop("name")
                for column, cell in row.items():
                    side, _, engine_metric = column.partition("-")
                    if side not in SIDES:
                        continue
                    # Engine names may contain dashes; the metric is the last component.
                    engine, _, metric = engine_metric.rpartition("-")
                    per_side = cells.setdefault((engine, metric), {s: {} for s in SIDES})
                    per_side[side].setdefault(instance, []).append(cell)
    return cells


def score(cell, metric, timeout, penalty):
    """Turns a cell into a number: a measurement, a PAR-2 score for a failed timed run, or None."""
    if cell in STATUS_CELLS:
        # A failed run says nothing about its memory, but it costs time instead of vanishing from the mean.
        return None if metric in MEMORY_METRICS else penalty * timeout
    try:
        return float(cell)
    except ValueError:
        return None


def compare(args):
    """Prints per-metric geometric means of the head/base ratios and fails above the threshold."""
    regressions = []
    rows = []
    unpaired, one_sided = [], set()
    for (engine, metric), per_side in sorted(read_rounds(args.rounds).items()):
        ratios, excluded, failed = [], 0, {side: 0 for side in SIDES}
        for instance in sorted(per_side["base"].keys() | per_side["head"].keys()):
            medians, measured = {}, []
            for side in SIDES:
                side_cells = per_side[side].get(instance, [])
                failed[side] += sum(cell in STATUS_CELLS for cell in side_cells)
                scores = [score(cell, metric, args.timeout, args.penalty) for cell in side_cells]
                if any(cell in STATUS_CELLS or value is not None for cell, value in zip(side_cells, scores)):
                    measured.append(side)
                scores = [value for value in scores if value is not None]
                if scores:
                    medians[side] = statistics.median(scores)
            if len(measured) == 1:
                if all(per_side.values()):
                    unpaired.append(f"{engine}-{metric} of {instance} ({measured[0]} only)")
                else:
                    # A metric only one build prints at all, e.g., a timer the head adds, has nothing to compare.
                    one_sided.add(f"{engine}-{metric} ({measured[0]} only)")
                continue
            if len(medians) != len(SIDES):
                continue
            base, head = medians["base"], medians["head"]
            if metric in MEMORY_METRICS:
                if base <= 0 or head <= 0:
                    continue
            elif max(base, head) < args.min_runtime:
                # Below the timer resolution both sides are noise; one tick would read as a 2x change.
                excluded += 1
                continue
            else:
                base, head = max(base, args.min_runtime), max(head, args.min_runtime)
            ratios.append((head / base, instance))
        if not ratios:
            if excluded == 0:
                # A metric with no number at all (e.g., an answer column) is not a measurement.
                continue
            rows.append([f"{engine}-{metric}", 0, excluded, failed["base"], failed["head"], None, None, ""])
            continue
        geomean = math.exp(statistics.fmean(math.log(ratio) for ratio, _ in ratios))
        worst_ratio, worst_instance = max(ratios)
        rows.append(
            [
                f"{engine}-{metric}",
                len(ratios),
                excluded,
                failed["base"],
                failed["head"],
                geomean,
                worst_ratio,
                worst_instance,
            ]
        )
        if args.threshold is not None and geomean > args.threshold:
            regressions.append((f"{engine}-{metric}", geomean))
    if unpaired:
        raise InputError("measured on one side only:\n  " + "\n  ".join(unpaired))
    if not rows:
        raise InputError("no metric measured on both sides")

    headers = [
        "metric",
        "compared",
        f"excluded (< {args.min_runtime} s)",
        "failed runs (base)",
        "failed runs (head)",
        "geomean head/base",
        "worst ratio",
        "worst instance",
    ]
    lines = ["| " + " | ".join(headers) + " |", "|" + " --- |" * len(headers)]
    for row in rows:
        lines.append(
            "| "
            + " | ".join(
                "-" if cell is None else f"{cell:.3f}" if isinstance(cell, float) else str(cell) for cell in row
            )
            + " |"
        )
    report = "\n".join(lines) + "\n"
    if one_sided:
        report += f"\nNot compared, measured by one build only: {', '.join(sorted(one_sided))}.\n"
    if regressions:
        report += (
            "\n"
            + "\n".join(
                f"REGRESSION: '{metric}' geometric mean of head/base ratios {geomean:.3f} exceeds {args.threshold}."
                for metric, geomean in regressions
            )
            + "\n"
        )
    print(report, end="")
    if args.summary:
        with open(args.summary, "a") as summary:
            summary.write(report)
    return 1 if regressions else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    jobs = subparsers.add_parser("jobs", help="Print a job file running the base and the head of every common method.")
    jobs.add_argument("base_jobs", help="Job file generated in the base build directory.")
    jobs.add_argument("head_jobs", help="Job file generated in the head build directory.")
    jobs.add_argument(
        "--head-first",
        action="store_true",
        help="Run the head before the base of every method; alternate it between rounds.",
    )
    jobs.set_defaults(func=merge_jobs)

    comp = subparsers.add_parser("compare", help="Compare the rounds of a merged job file.")
    comp.add_argument("rounds", nargs="+", help="pyco_proc CSVs, one per round of the merged job file.")
    comp.add_argument("--timeout", type=float, required=True, help="Timeout (in seconds) of the runs.")
    comp.add_argument(
        "--penalty",
        type=float,
        default=2.0,
        help="A failed run (TO, MO, ERR, CRASH) costs this multiple of --timeout (default: PAR-2).",
    )
    comp.add_argument(
        "--min-runtime",
        type=float,
        default=0.05,
        help="Exclude a time metric of an instance when both medians are below this many seconds, "
        "and floor the compared times at it; GNU time reports user time in 0.01 s steps "
        "(default: %(default)s).",
    )
    comp.add_argument(
        "--threshold",
        type=float,
        default=None,
        help="Fail with exit status 1 when the geometric mean of a metric's head/base ratios exceeds this value.",
    )
    comp.add_argument("--summary", help="Append the Markdown report to this file, e.g. $GITHUB_STEP_SUMMARY.")
    comp.set_defaults(func=compare)

    args = parser.parse_args()
    try:
        return args.func(args)
    except InputError as error:
        parser.exit(2, f"{parser.prog}: error: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
