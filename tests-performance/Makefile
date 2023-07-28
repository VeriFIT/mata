all: perftest

perftest:
	./pycobench -c jobs/jobs.yaml < inputs/double-automata.input
	./pyco_proc --csv pycobench.tasks

.PHONY: all clean
