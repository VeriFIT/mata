#!/usr/bin/env python3
"""Throwaway: per experiment and build, the geometric mean and total of the min-of-repeats time against the baseline.

usage: analyse.py <baseline slug> <jsonl>...
Every record must hold the same builds (or only the baseline, when its probe run failed and the instance was skipped).
Metric 'op' = overall - mataparsing - mataminterm (the operations of the program); 'overall' = whole run.
Only instances where every build finished every repeat are timed, and of those only instances whose baseline time
is at least 1 ms: both the geometric mean and the total ratio cover that population.
Every repeat of every build is checked against the baseline's first repeat: a differing 'result', or a 'result'
present in one run and missing in the other, is a disagreement. Programs printing no 'result' at all are counted
separately, as their answers were not compared.
"""

import collections
import json
import math
import sys

MIN_TIME = 1e-3

base = sys.argv[1]
recs = []
for path in sys.argv[2:]:
    with open(path) as file:
        recs += [json.loads(line) for line in file]
full = [r for r in recs if len(r["res"]) > 1]
if not full:
    sys.exit("no record measured more than the baseline")
slugs = list(full[0]["res"])
if base not in slugs:
    sys.exit(f"baseline '{base}' is not among the builds {slugs}")
for rec in recs:
    builds = set(rec["res"])
    if builds != set(slugs) and builds != {base}:
        sys.exit(f"record {rec['exp']} {rec['inst']} holds builds {sorted(builds)}, expected {sorted(slugs)}")


def op(r):
    return r["overall"] - r.get("mataparsing", 0.0) - r.get("mataminterm", 0.0)


per_exp = collections.defaultdict(list)
failures = collections.Counter()
disagree = collections.Counter()
compared = collections.Counter()
no_result = collections.Counter()
skipped = collections.Counter()
for rec in recs:
    runs = rec["res"]
    if len(runs) == 1:
        skipped[(rec["exp"], runs[base][0]["status"])] += 1
        continue
    if any(r["status"] != "ok" for s in slugs for r in runs[s]):
        for s in slugs:
            failures[(rec["exp"], s, tuple(sorted({r["status"] for r in runs[s]})))] += 1
        continue
    if all("result" not in r for s in slugs for r in runs[s]):
        no_result[rec["exp"]] += 1
    else:
        compared[rec["exp"]] += 1
        reference = runs[base][0].get("result")
        for s in slugs:
            if any(r.get("result") != reference or "result" not in r for r in runs[s]):
                disagree[(rec["exp"], s)] += 1
    per_exp[rec["exp"]].append({s: (min(op(r) for r in runs[s]), min(r["overall"] for r in runs[s])) for s in slugs})


def geomean(ratios):
    return math.exp(sum(map(math.log, ratios)) / len(ratios))


others = [s for s in slugs if s != base]
for metric, idx in (("op", 0), ("overall", 1)):
    print(f"\n## {metric}: ratio vs {base}, instances with {base} {metric} >= 1 ms: geomean (tot = ratio of totals)")
    print(f"| experiment | n | {base} total s | " + " | ".join(others) + " |")
    print("|---|---|---|" + "---|" * len(others))
    all_rows = collections.defaultdict(list)
    for exp in sorted(per_exp):
        rows = [r for r in per_exp[exp] if r[base][idx] >= MIN_TIME and all(r[s][idx] > 0 for s in others)]
        if not rows:
            continue
        cells = []
        for s in others:
            ratios = [r[s][idx] / r[base][idx] for r in rows]
            all_rows[s] += ratios
            tot = sum(r[s][idx] for r in rows) / sum(r[base][idx] for r in rows)
            cells.append(f"{geomean(ratios):.3f} (tot {tot:.3f})")
        print(f"| {exp} | {len(rows)} | {sum(r[base][idx] for r in rows):.2f} | " + " | ".join(cells) + " |")
    if not others or not all_rows:
        print("(no instance qualifies)")
        continue
    print(
        f"| **all** | {len(all_rows[others[0]])} | | "
        + " | ".join(f"{geomean(all_rows[s]):.3f}" for s in others)
        + " |"
    )

print("\n## failures (exp, build, statuses): instances")
for k, v in sorted(failures.items()):
    print(k, v)
print(f"\n## result disagreements with the first repeat of {base}, over every repeat")
print(dict(disagree) or "none")
print("\n## instances whose result was compared, per experiment")
print(dict(compared) or "none")
print("\n## instances whose program prints no result (not compared), per experiment")
print(dict(no_result) or "none")
print("\n## skipped after the baseline probe failed (exp, status): instances")
print(dict(skipped) or "none")
