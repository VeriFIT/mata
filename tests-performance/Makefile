all: perftest

perftest:
	./pycobench -c jobs.yaml < double-automata.input
	./pyco_proc --csv pycobench.tasks

.PHONY: all clean
