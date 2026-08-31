# Integration tests and benchmarks of the Mata library. See ../BENCHMARKING.md for details.

import '../just/common.just'

# Run the correctness benchmarking jobs and process their results into CSV files.
[default]
performance:
	make -C '{{ ROOT }}' test-performance

# Run a single benchmark binary built in BUILD_MODE, e.g. 'just bench run bench-automata-inclusion aut1.mata aut2.mata'.
run BENCH *ARGS:
	'{{ BUILD_ROOT }}/{{ BUILD_MODE }}/{{ CXX }}/tests-integration/{{ BENCH }}' {{ ARGS }}

# Run benchmarks through pycobench, e.g. 'just bench pyco --config jobs/JOB.yaml INPUT'.
pyco *ARGS:
	./scripts/run_pyco.sh {{ ARGS }}

# Process raw pycobench outputs into CSV files.
process *ARGS:
	./scripts/process_pyco.sh {{ ARGS }}

# Compare CSV profiles of several benchmark runs.
compare +PROFILES:
	python3 ./scripts/compare_profiles.py {{ PROFILES }}

# Profile the memory consumption of a command with Valgrind Massif.
massif +ARGS:
	./scripts/run_massif.sh {{ ARGS }}

# Profile the call graph of a command with Valgrind Callgrind.
callgrind +ARGS:
	./scripts/run_callgrind.sh {{ ARGS }}
