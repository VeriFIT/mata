#!/usr/bin/env python3
"""Throwaway: per experiment and build, the geometric mean and total of the min-of-repeats time against the baseline.

usage: analyse.py <baseline slug> <jsonl>...
Metric 'op' = overall - mataparsing - mataminterm (the operations of the program); 'overall' = whole run.
Only instances where every build finished every repeat count; disagreeing 'result' fields are reported.
"""

import collections
import json
import math
import sys

base = sys.argv[1]
recs = []
for path in sys.argv[2:]:
    with open(path) as file:
        recs += [json.loads(line) for line in file]
slugs = max((list(r["res"]) for r in recs), key=len)


def op(r):
    return r["overall"] - r.get("mataparsing", 0.0) - r.get("mataminterm", 0.0)


per_exp = collections.defaultdict(list)
failures = collections.Counter()
disagree = collections.Counter()
skipped = collections.Counter()
for rec in recs:
    runs = rec["res"]
    if len(runs) == 1:
        skipped[(rec["exp"], next(iter(runs.values()))[0]["status"])] += 1
        continue
    if any(r["status"] != "ok" for s in slugs for r in runs[s]):
        for s in slugs:
            failures[(rec["exp"], s, tuple(sorted({r["status"] for r in runs[s]})))] += 1
        continue
    results = {s: runs[s][0].get("result") for s in slugs}
    for s in slugs:
        if results[s] != results[base]:
            disagree[(rec["exp"], s)] += 1
    per_exp[rec["exp"]].append({s: (min(op(r) for r in runs[s]), min(r["overall"] for r in runs[s])) for s in slugs})

others = [s for s in slugs if s != base]
for metric, idx in (("op", 0), ("overall", 1)):
    print(f"\n## {metric}: geomean ratio vs {base} (instances with {base} {metric} >= 1 ms) | total {base} s")
    print(f"| experiment | n | {base} total s | " + " | ".join(others) + " |")
    print("|---|---|---|" + "---|" * len(others))
    all_rows = collections.defaultdict(list)
    for exp in sorted(per_exp):
        rows = [r for r in per_exp[exp] if r[base][idx] >= 1e-3]
        if not rows:
            continue
        cells = []
        for s in others:
            ratios = [r[s][idx] / r[base][idx] for r in rows]
            all_rows[s] += ratios
            gm = math.exp(sum(map(math.log, ratios)) / len(ratios))
            tot = sum(r[s][idx] for r in rows) / sum(r[base][idx] for r in rows)
            cells.append(f"{gm:.3f} (tot {tot:.3f})")
        print(f"| {exp} | {len(rows)} | {sum(r[base][idx] for r in rows):.2f} | " + " | ".join(cells) + " |")
    print(
        f"| **all** | {len(all_rows[others[0]])} | | "
        + " | ".join(f"{math.exp(sum(map(math.log, all_rows[s])) / len(all_rows[s])):.3f}" for s in others)
        + " |"
    )

print("\n## failures (exp, build, statuses): instances")
for k, v in sorted(failures.items()):
    print(k, v)
print("\n## result disagreements with", base)
print(dict(disagree) or "none")
print("\n## skipped after the baseline probe failed (exp, status): instances")
print(dict(skipped) or "none")
