# Integration tests and benchmarks of the Mata library. See ../BENCHMARKING.md for details.

import '../just/common.just'

# The job and input files are generated into the build directory and refer to the binaries built
# there, so every recipe below has to point at the same build directory as `cpp::build`.
BENCH_DIR := BUILD_ROOT / BUILD_MODE / CXX

# Run the correctness benchmarking jobs and process their results into CSV files.
[default]
performance MODE=BUILD_MODE:
	make -C '{{ ROOT }}' test-performance BUILD_DIR='{{ BUILD_ROOT }}/{{ MODE }}/{{ CXX }}'

# Run a single benchmark binary built in BUILD_MODE, e.g. 'just bench run bench-automata-inclusion aut1.mata aut2.mata'.
run BENCH *ARGS:
	'{{ BENCH_DIR }}/tests-integration/{{ BENCH }}' {{ ARGS }}

# Run benchmarks through pycobench, e.g. 'just bench pyco --config jobs/JOB.yaml INPUT'.
pyco *ARGS:
	MATA_BUILD_DIR='{{ BENCH_DIR }}' ./scripts/run_pyco.sh {{ ARGS }}

# Compare CSV profiles of several benchmark runs.
compare +PROFILES:
	python3 ./pycobench/src/compare_profiles.py {{ PROFILES }}

# Profile the memory consumption of a command with Valgrind Massif.
massif +ARGS:
	./scripts/run_massif.sh {{ ARGS }}

# Profile the call graph of a command with Valgrind Callgrind.
callgrind +ARGS:
	./scripts/run_callgrind.sh {{ ARGS }}
