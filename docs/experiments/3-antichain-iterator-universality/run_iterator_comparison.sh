#!/usr/bin/env bash
set -euo pipefail

data_dir=${1:-/tmp/p752}
univ_samples=$(cat "$data_dir/univ-sample.list") || { echo "Cannot read $data_dir/univ-sample.list" >&2; exit 1; }
incl_samples=$(cat "$data_dir/incl-sample.list") || { echo "Cannot read $data_dir/incl-sample.list" >&2; exit 1; }

run_probe() {
  local output
  output=$(taskset --cpu-list 3 "$@") || {
    local status=$?
    echo "Probe failed with status $status: $*" >&2
    return "$status"
  }
  tail -n 1 <<<"$output"
}

for round in 1 2 3; do
  echo "ROUND $round"
  for f in $univ_samples; do
    b=$(run_probe ./probe_c_base det "$f")
    c=$(run_probe ./probe_c_c det "$f")
    echo "det;$f;$b;$c"
  done
  for p in $incl_samples; do
    lhs="$p-lhs.mata"
    rhs="$p-rhs.mata"
    b=$(run_probe ./probe_c_base incl "$lhs" "$rhs")
    c=$(run_probe ./probe_c_c incl "$lhs" "$rhs")
    echo "incl;$p;$b;$c"
  done
  for f in $univ_samples; do
    b=$(run_probe ./probe_c_base univ "$f")
    c=$(run_probe ./probe_c_c univ "$f")
    echo "univ;$f;$b;$c"
  done
done
