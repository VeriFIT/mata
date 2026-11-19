#!/usr/bin/env python3
"""Summarize symbolized NFA benchmarking results from automata-lib-bench.

usage: analyse.py BASELINE RESULTS_ROOT

For each family, use the merged CSV, or the newest repeats CSV if no merged
report exists. A row's automaton operands add unlabelled columns before the
measurement columns. Each ratio is the geometric mean of min-of-three operation
times on instances where the two builds finished. Comparisons have independent
completed-instance populations. Empty operation results and zero durations are
not timed.
"""

import collections
import csv
import math
import sys
from pathlib import Path

if len(sys.argv) != 3:
    sys.exit(__doc__)
BASELINE = sys.argv[1]
ROOT = Path(sys.argv[2])


def seconds(value):
    try:
        value = float(value)
    except (TypeError, ValueError):
        return None
    return value if math.isfinite(value) and value >= 0 else None


def read_family(csv_path):
    with csv_path.open(newline="") as csv_file:
        reader = csv.reader(csv_file, delimiter=";")
        header = next(reader)
        methods = []
        for index, column in enumerate(header[1:], start=1):
            method, separator, metric = column.rpartition("-")
            if separator and metric in {"operation", "runtime", "result"}:
                methods.append((index, method, metric))
        families = []
        for row in reader:
            operand_offset = len(row) - len(header)
            if operand_offset < 0:
                raise ValueError(f"{csv_path}:{reader.line_num} has fewer columns than the header")
            values = collections.defaultdict(dict)
            for index, method, metric in methods:
                values[method][metric] = row[index + operand_offset]
            families.append(values)
    return families


families = {}
for family_dir in sorted(path for path in ROOT.iterdir() if path.is_dir()):
    csv_paths = sorted(family_dir.glob("*.csv"))
    merged = [path for path in csv_paths if not path.name.endswith("-repeats.csv")]
    csv_paths = merged or csv_paths[-1:]
    if len(csv_paths) != 1:
        sys.exit(f"expected one merged CSV in {family_dir}, found {len(csv_paths)}")
    families[family_dir.name] = read_family(csv_paths[0])

if BASELINE not in next(iter(families.values()))[0]:
    sys.exit(f"baseline {BASELINE!r} is missing from {ROOT}")
METHODS = sorted(next(iter(next(iter(families.values())))))
assert BASELINE in METHODS
METHODS = [BASELINE] + [method for method in METHODS if method != BASELINE]


def status(record):
    runtime = record.get("runtime", "")
    if runtime == "TO":
        return "timeouts"
    if runtime.startswith("MO"):
        return "memouts"
    if runtime.startswith("ERR"):
        return "errors"
    if runtime == "MISSING":
        return "unmeasured"
    operation = seconds(record.get("operation"))
    if operation is None:
        return "errors"
    assert seconds(runtime) is not None, record
    return "solved"


keys = []
for family, records in families.items():
    for method in METHODS:
        keys.append((family, method))
totals = {key: collections.Counter() for key in keys}
disagreements = collections.Counter()
common = collections.defaultdict(list)
for family, records in families.items():
    for record in records:
        for method in METHODS:
            totals[family, method]["instances"] += 1
            totals[family, method][status(record[method])] += 1
        for method in METHODS[1:]:
            pair = (family, method)
            if status(record[method]) != "solved" or status(record[BASELINE]) != "solved":
                continue
            baseline_time = seconds(record[BASELINE]["operation"])
            method_time = seconds(record[method]["operation"])
            if baseline_time > 0 and method_time > 0:
                common[pair].append(method_time / baseline_time)
            baseline_answer = record[BASELINE].get("result", "")
            method_answer = record[method].get("result", "")
            if baseline_answer and method_answer and baseline_answer != method_answer:
                disagreements[pair] += 1


def print_table(headers, rows):
    print("| " + " | ".join(headers) + " |")
    print("|" + "|".join("---" for _ in headers) + "|")
    for row in rows:
        print("| " + " | ".join(str(value) for value in row) + " |")


print("## Pairwise operation-time ratios\n")
print(
    "Geometric mean of `build / baseline` on instances completed by both. A row with no completed pair has no ratio.\n"
)
rows = []
for family in families:
    for method in METHODS[1:]:
        ratios = common[family, method]
        ratio = f"{math.exp(sum(map(math.log, ratios)) / len(ratios)):.2f}x" if ratios else "—"
        rows.append([family, f"{method} / {BASELINE}", len(ratios), ratio])
for method in METHODS[1:]:
    ratios = [ratio for (family, pair), values in common.items() if pair == method for ratio in values]
    rows.append(
        [
            "**total**",
            f"{method} / {BASELINE}",
            sum(len(common[family, method]) for family in families),
            f"{math.exp(sum(map(math.log, ratios)) / len(ratios)):.2f}x" if ratios else "—",
        ]
    )
print_table(["family", "comparison", "completed pairs", "ratio"], rows)

print("\n## Completed instances and failures\n")
print("Unmeasured rows are pycobench's `MISSING`, not a completed run or a measured failure.\n")
status_names = ["solved", "instances", "timeouts", "memouts", "errors", "unmeasured"]
rows = []
for family in families:
    for method in METHODS:
        counts = totals[family, method]
        rows.append([family, method] + [counts[name] for name in status_names])
for method in METHODS:
    rows.append(
        ["**total**", method] + [sum(totals[family, method][name] for family in families) for name in status_names]
    )
print_table(["family", "build"] + status_names, rows)

print("\n## Answer disagreements\n")
print("Distinct instances on which both completed builds printed different results; zero denotes none.\n")
print_table(
    ["family", "comparison", "disagreements"],
    [
        [family, f"{method} / {BASELINE}", disagreements[family, method]]
        for family in families
        for method in METHODS[1:]
    ],
)
