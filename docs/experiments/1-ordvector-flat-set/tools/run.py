#!/usr/bin/env python3
"""Throwaway: every TACAS'24 Mata instance under each OrdVector variant build, interleaved per instance on one pinned core.

usage: run.py <heavy|light> <out.jsonl> <slug>...   (binaries are ./bin/mata-<slug>)
"""

import glob
import json
import queue
import random
import subprocess
import sys
import threading
import time

EXPS = [
    (
        "armc-incl",
        "./emp-programs/automata_inclusion.emp $1 $2",
        "inputs/bench-double-automata-inclusion.input",
        True,
    ),
    ("b-smt", "$1 $2", "inputs/bench-variadic-bool-comb-ere.input", True),
    (
        "email-filter",
        "./emp-programs/email_filter.emp $1 $2 $3 $4 $5",
        "inputs/bench-quintuple-email-filter.input",
        False,
    ),
    (
        "lia-explicit",
        "./emp-programs/complement.emp $1",
        "inputs/bench-single-presburger-explicit-complement.input",
        False,
    ),
    (
        "lia-symbolic",
        "./emp-programs/complement.emp $1",
        "inputs/bench-single-presburger-complement.input",
        False,
    ),
    (
        "regexps-det-min",
        "./emp-programs/determinize-minimize.emp $1",
        "inputs/bench-regexps_union.input",
        True,
    ),
    (
        "noodler-compl",
        "./emp-programs/complement.emp $1",
        "inputs/bench-single-z3-noodler.input",
        False,
    ),
    (
        "noodler-compl-big",
        "./emp-programs/complement.emp $1",
        "inputs/bench-single-z3-noodler-big.input",
        False,
    ),
    (
        "noodler-concat",
        "./emp-programs/concat.emp $1",
        "inputs/bench-single-z3-noodler.input",
        False,
    ),
    (
        "noodler-inter",
        "./emp-programs/bool_comb_intersect.emp $1",
        "inputs/bench-variadic-z3-noodler-intersect.input",
        False,
    ),
    (
        "param-inter",
        "./emp-programs/bool_comb_inter_sat.emp $1 $2",
        "inputs/bench-double-bool-comb-cox.input",
        True,
    ),
    (
        "param-inter-variadic",
        "./emp-programs/bool_comb_intersect.emp $1",
        "inputs/bench-variadic-bool-comb-intersect.input",
        True,
    ),
    (
        "param-union",
        "./emp-programs/bool_comb_union.emp $1 $2",
        "inputs/bench-double-bool-comb-cox.input",
        True,
    ),
    (
        "param-union-variadic",
        "./emp-programs/bool_comb_intersect_union.emp $1",
        "inputs/bench-variadic-bool-comb-intersect.input",
        True,
    ),
    (
        "param-diff",
        "./emp-programs/bool_comb_diff_sat.emp $1 $2",
        "inputs/bench-double-bool-comb-cox.input",
        True,
    ),
]
TIMEOUT, REPEAT = 30, 3
MODES = {"heavy": (True, 7, [2, 6, 10]), "light": (False, 3, [2, 4, 6, 8, 10, 12])}


def instances(src):
    out = []
    with open(src) as file:
        lines = file.readlines()
    for line in lines:
        if not line.strip():
            continue
        fields = line.strip().split(";")
        # b-smt: the first column is the program, the rest are globs of automata.
        expanded = [fields[0]] + [f for g in fields[1:] for f in (sorted(glob.glob(g)) if "*" in g else [g])]
        out.append(expanded)
    return out


def run_one(cmd, cpu, memout_gb):
    # ulimit in the child shell: preexec_fn is unsafe in a threaded parent.
    shell = f"ulimit -v {memout_gb << 20}; exec {cmd}"
    t0 = time.perf_counter()
    p = subprocess.Popen(
        ["taskset", "--cpu-list", str(cpu), "sh", "-c", shell],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        out, _ = p.communicate(timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        p.kill()
        p.communicate()
        return {"status": "TO"}
    wall = time.perf_counter() - t0
    if p.returncode != 0:
        return {"status": f"ERR{p.returncode}", "out": out[-300:]}
    r = {"status": "ok", "wall": wall}
    for line in out.splitlines():
        k, _, v = line.partition(":")
        v = v.strip()
        try:
            r[k.strip()] = float(v)
        except ValueError:
            r[k.strip()] = v
    return r


def main(mode, outpath, slugs):
    heavy, memout, cpus = MODES[mode]
    tasks = queue.Queue()
    for name, tmpl, src, h in EXPS:
        if h != heavy:
            continue
        for args in instances(src):
            if name == "b-smt":
                cmd = " ".join(args)
            else:
                cmd = tmpl
                for i in range(len(args), 0, -1):
                    cmd = cmd.replace(f"${i}", args[i - 1])
            tasks.put((name, ";".join(args), cmd))
    total, done, lock = tasks.qsize(), [0], threading.Lock()

    def worker(cpu):
        rng = random.Random(cpu)
        while True:
            try:
                name, inst, cmd = tasks.get_nowait()
            except queue.Empty:
                return
            # Probe the baseline (first slug) once: an instance it times out or fails on is skipped for every
            #  build, so the hopeless regexps and param-diff instances do not cost 30 s per build and repeat.
            probe = run_one(f"./bin/mata-{slugs[0]} {cmd}", cpu, memout)
            if probe["status"] != "ok":
                res = {slugs[0]: [probe]}
            else:
                res = {s: [] for s in slugs}
                for rep in range(REPEAT):
                    order = list(slugs)
                    rng.shuffle(order)
                    for s in order:
                        res[s].append(run_one(f"./bin/mata-{s} {cmd}", cpu, memout))
            with lock:
                out.write(json.dumps({"exp": name, "inst": inst, "res": res}) + "\n")
                out.flush()
                done[0] += 1
                if done[0] % 200 == 0:
                    print(f"{mode} {done[0]}/{total}", flush=True)

    with open(outpath, "a") as out:
        threads = [threading.Thread(target=worker, args=(c,)) for c in cpus]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    print(f"{mode} done {done[0]}/{total}", flush=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
