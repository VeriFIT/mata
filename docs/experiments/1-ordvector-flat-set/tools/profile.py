#!/usr/bin/env python3
"""Throwaway: perf profiles of one build over a sample of every TACAS'24 Mata experiment, as used for EXP-1.

usage: profile.py <binary> <out dir>   (run from the automata-lib-bench root; PERF selects the perf binary)

Per experiment, in the order below and from one random.Random(1) stream, up to 40 lines of its input are sampled
without replacement. Every sampled instance runs under 'timeout 15'; one 'perf record --freq 2000' covers the whole
sample and writes <out dir>/<experiment>.data. Then run shares.sh on the .data files.
"""

import glob
import os
import pathlib
import random
import subprocess
import sys

# Order matters: all samples come from one random stream.
EXPS = {
    "armc-incl": (
        "emp-programs/automata_inclusion.emp",
        "inputs/bench-double-automata-inclusion.input",
    ),
    "email-filter": (
        "emp-programs/email_filter.emp",
        "inputs/bench-quintuple-email-filter.input",
    ),
    "lia-explicit": (
        "emp-programs/complement.emp",
        "inputs/bench-single-presburger-explicit-complement.input",
    ),
    "lia-symbolic": (
        "emp-programs/complement.emp",
        "inputs/bench-single-presburger-complement.input",
    ),
    "regexps-det-min": (
        "emp-programs/determinize-minimize.emp",
        "inputs/bench-regexps_union.input",
    ),
    "noodler-compl": (
        "emp-programs/complement.emp",
        "inputs/bench-single-z3-noodler.input",
    ),
    "noodler-concat": (
        "emp-programs/concat.emp",
        "inputs/bench-single-z3-noodler.input",
    ),
    "noodler-inter": (
        "emp-programs/bool_comb_intersect.emp",
        "inputs/bench-variadic-z3-noodler-intersect.input",
    ),
    "param-inter": (
        "emp-programs/bool_comb_inter_sat.emp",
        "inputs/bench-double-bool-comb-cox.input",
    ),
    "param-union": (
        "emp-programs/bool_comb_union.emp",
        "inputs/bench-double-bool-comb-cox.input",
    ),
    "param-diff": (
        "emp-programs/bool_comb_diff_sat.emp",
        "inputs/bench-double-bool-comb-cox.input",
    ),
    # Every line brings its own program in the first column; the others are globs of automata.
    "b-smt": (None, "inputs/bench-variadic-bool-comb-ere.input"),
}
SAMPLE, TIMEOUT, FREQ = 40, 15, 2000


def main(binary, out_dir):
    perf = os.environ.get("PERF", "perf")
    os.makedirs(out_dir, exist_ok=True)
    rng = random.Random(1)
    for name, (program, src) in EXPS.items():
        with open(src) as file:
            lines = [line.strip() for line in file if line.strip()]
        script = ["set +e"]
        for line in rng.sample(lines, min(SAMPLE, len(lines))):
            fields = line.split(";")
            if program is None:
                args = [fields[0]] + [f for pattern in fields[1:] for f in sorted(glob.glob(pattern))]
            else:
                args = [program] + fields
            # Unquoted on purpose: the shell expands the globs of the variadic inputs (noodler-inter).
            script.append(f"timeout {TIMEOUT} {binary} {' '.join(args)} >/dev/null 2>&1")
        commands = pathlib.Path(out_dir, f"{name}.sh")
        commands.write_text("\n".join(script) + "\n")
        data = os.path.join(out_dir, f"{name}.data")
        subprocess.run(
            [
                perf,
                "record",
                "--quiet",
                "--freq",
                str(FREQ),
                "--output",
                data,
                "--",
                "bash",
                str(commands),
            ],
            check=True,
        )
        print(name, os.path.getsize(data), flush=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
