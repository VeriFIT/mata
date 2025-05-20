input_file_path = "../../inputs/double-large-sequence.input.in"
benchmark_subpath = "@CMAKE_CURRENT_SOURCE_DIR@/cntnfa-bench/benchmarks/large_sequence"

with open(input_file_path, "w") as f:
    for n in range(1, 10001):
        line = f"{benchmark_subpath}/nfa-0-{n}.mata;{benchmark_subpath}/cntnfa-0-{n}.mata"
        f.write(line + "\n")
