#!/bin/sh
set -eu
for r in 1 2 3; do
  echo "ROUND $r"
  for f in $(cat /tmp/p752/univ-sample.list); do
    b=$(taskset --cpu-list 3 ./probe_c_base det "$f" 2>/dev/null | tail -n 1)
    c=$(taskset --cpu-list 3 ./probe_c_c det "$f" 2>/dev/null | tail -n 1)
    echo "det;$f;$b;$c"
  done
  for p in $(cat /tmp/p752/incl-sample.list); do
    l="$p-lhs.mata"; r="$p-rhs.mata"
    b=$(taskset --cpu-list 3 ./probe_c_base incl "$l" "$r" 2>/dev/null | tail -n 1)
    c=$(taskset --cpu-list 3 ./probe_c_c incl "$l" "$r" 2>/dev/null | tail -n 1)
    echo "incl;$p;$b;$c"
  done
  for f in $(cat /tmp/p752/univ-sample.list); do
    b=$(taskset --cpu-list 3 ./probe_c_base univ "$f" 2>/dev/null | tail -n 1)
    c=$(taskset --cpu-list 3 ./probe_c_c univ "$f" 2>/dev/null | tail -n 1)
    echo "univ;$f;$b;$c"
  done
done
