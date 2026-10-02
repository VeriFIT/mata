#!/usr/bin/env bash

# Helper script that runs experiments and generates .csv files.
#
# The results are stored in results/data directory.
#
# The result of the script is in form of `;` delimited .csv file for set of outputs of benchmarks.

usage() { {
        echo "usage: ./run_pyco.sh [OPTION]... [BENCHMARK]..."
        echo "options:"
        echo "  -c|--config <conf.yaml>     configuration file in yaml format"
        echo "                              [default=<MATA_BUILD_DIR>/tests-integration/jobs/bench-cade-23.yaml]"
        echo "  -t|--timeout <int>         timeout for each benchmark in seconds [default=60s]"
        echo "  -m|--methods                will measure only selected tools"
        echo "  -j|--jobs                   number of paralel jobs"
        echo "  -s|--suffix                 adds suffix to the target file"
    } >&2
}

die() {
    echo "error: $*" >&2
    exit 1
}

# Be default we run the experiments on 6 CPUs (any more, in our experience, leads to some unexpected
# behaviour), it deletes temporary files, runs no warmups, and measures once with timeout 60s.
# In our experience, 60s timeout is enough.
timeout=60
benchmarks=()
jobs=6
methods=
# `methods` is only the tag embedded in the result file name; the actual arguments are passed as an array.
methods_args=()
suffix=""
basedir=$(realpath "$(dirname "$0")")
rootdir=$(realpath "$basedir/..")
# The `.yaml` job files are generated into the build directory, so that each build directory refers
# to the binaries it has built. Override `MATA_BUILD_DIR` for an out-of-tree build directory.
build_dir=${MATA_BUILD_DIR:-"$rootdir/../build"}
config=$build_dir/tests-integration/jobs/bench-cade-23.yaml

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0;;
        -s|--suffix)
            suffix="$2"
            shift 2;;
        -t|--timeout)
            timeout="$2"
            shift 2;;
        -c|--config)
            config="$2"
            shift 2;;
        -j|--jobs)
            jobs=$2
            shift 2;;
        -m|--methods)
            methods="-m $2"
            methods_args=( -m "$2" )
            shift 2;;
        *)
            benchmarks+=( "$1" )
            shift 1;;
    esac
done

# $1: filename, $2: extension
# Adds extension $2 to the $filename, unless it already contains it
escape_extension () {
  if [[ "$1" == *.$2 ]];
  then
    echo "$1"
  else
    echo "$1.$2"
  fi
}

# $1: filename, $2: directory
# Adds $2 directory before the filename, unless it already contains it
prepend_directory() {
  if [[ "$1" == $2/* ]];
  then
    echo "$1"
  else
    echo "$2/$1"
  fi
}

# Check that dependencies for running pycobench were installed

tmp_output=$(mktemp)
"$rootdir/"pycobench >& "$tmp_output"
if grep -q ModuleNotFoundError "$tmp_output"; then
  echo "[!] Your system is missing python modules for running pycobench."
  read -rp "Do you wish to install these requirements? [y/n] " choice
  case "$choice" in
    y|Y ) python3 -m pip install "$rootdir";;
    * ) die "missing pycobench dependencies; please, install them from './tests-integration/pyproject.toml'";;
  esac
fi
rm "$tmp_output"

# Prepares configuration
config=$(escape_extension "$config" "yaml")
result_dir=$rootdir/results
mkdir -p "$result_dir"

[ ${#benchmarks[@]} -gt 0 ] 2>/dev/null || die "error: you must specify some *.input file with benchmarks"

start_time=$SECONDS
for benchmark in "${benchmarks[@]}"
do
    # Script takes .input files with list of benchmarks
    benchmark_file=$(escape_extension "$benchmark" "input")
    # The result contains information about date, timeout, number of jobs and measured subset of tools (if specified)
    # Run the script with -s|--suffix to append other information to your benchmarks
    # Spaces in the result file name confuse the downstream tools that consume it, so collapse them
    #  into dashes. Only the name is sanitised: `$result_dir` is a path we are given, and rewriting
    #  it would point the run at a directory that does not exist.
    benchmark_name=$(basename "$benchmark")
    result_name=${benchmark_name%.*}-$(date +%Y-%m-%d-%H-%M-%S)-timeout-$timeout-jobs-$jobs$methods$suffix
    result_file=$result_dir/${result_name// /-}

    # Perform actual runs
    intermediate=()
    echo "[!] Performing benchmarks"
    sub_result_file="$result_file.output"
    intermediate+=( "$sub_result_file" )
    "$rootdir/"pycobench "${methods_args[@]}" -j "$jobs" -c "$config" -t "$timeout" -o "$sub_result_file" < "$benchmark_file"

    number_of_params=$(($(head -1 < "$benchmark_file"  | tr -cd ';' | wc -c) + 1))

    echo "[!] Benchmark measured"
    "$basedir"/process_pyco.sh -o "$result_file.csv" -p "$number_of_params" "${intermediate[@]}"
    echo "[!] Benchmark processed"

    # All intermediate files are deleted
    rm "${intermediate[@]}"
done

python3 "$basedir"/compare_profiles.py "$result_file.csv"

secs=$((SECONDS - start_time))
formated_elapsed=$(printf "%dd:%dh:%dm:%ds\n" $((secs/86400)) $((secs%86400/3600)) $((secs%3600/60)) $((secs%60)))
eval "echo [!] Benchmarking done in $formated_elapsed"
