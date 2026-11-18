#!/usr/bin/env bash
# Throwaway: share of samples per source file of the leaf (inlined) frame, for files tied to the set containers.
P=${PERF:-perf}
for data in "$@"; do
    name=$(basename "$data" .data)
    "$P" report --input "$data" --no-children --sort srcfile --stdio 2>/dev/null |
        awk -v n="$name" '
            /^ *[0-9.]+%/ { pct = $1; sub("%", "", pct); file = $2; share[file] += pct }
            END {
                split("ord-vector.hh small-ord-vector.hh stl_algo.h stl_algobase.h vector.tcc stl_vector.h synchronized-iterator.hh sparse-set.hh hashtable.h", keys, " ")
                line = sprintf("%-18s", n)
                for (i = 1; i in keys; i++) line = line sprintf(" %6.2f", share[keys[i]])
                print line
            }'
done
