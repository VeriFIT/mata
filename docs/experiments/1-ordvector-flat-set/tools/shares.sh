#!/usr/bin/env bash
# Throwaway: share of samples per source file of the leaf (inlined) frame, for files tied to the set containers.
set -euo pipefail
P=${PERF:-perf}
for data in "$@"; do
    name=$(basename "$data" .data)
    # Report first: a missing perf or an unreadable profile must fail, not print a row of zeros.
    report=$("$P" report --input "$data" --no-children --sort srcfile --stdio) || {
        echo "error: perf report failed for $data" >&2
        exit 1
    }
    printf '%s\n' "$report" |
        awk -v n="$name" '
            /^ *[0-9.]+%/ { pct = $1; sub("%", "", pct); file = $2; share[file] += pct }
            END {
                split("ord-vector.hh small-ord-vector.hh stl_algo.h stl_algobase.h vector.tcc stl_vector.h synchronized-iterator.hh sparse-set.hh hashtable.h", keys, " ")
                line = sprintf("%-18s", n)
                for (i = 1; i in keys; i++) line = line sprintf(" %6.2f", share[keys[i]])
                print line
            }'
done
