import argparse

def generate_anbn_nfa(n: int, output_path: str):
    states = set()
    transitions = []
    final_states = {f"q{n * 2}"}

    # add states
    for i in range(0, n * 2):
        states.add(f"q{i}")

    for i in range(0, n):
        # add transitions with a
        transitions.append(f"q{i} 0 q{i + 1}")
        # add transitions with b
        transitions.append(f"q{i + n} 1 q{i + n + 1}")

    if n > 1:
        # add middle transitions with b
        for i in range(0, n - 1):
            transitions.append(f"q{i + 1} 1 q{n * 2 - i}")

    with open(output_path, "w") as file:
        file.write("@NFA-explicit\n")
        file.write("%Initial q0\n")
        file.write("%Final " + " ".join(sorted(final_states)) + "\n")

        for transition in transitions:
            file.write(transition + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--n", type=int, required=True, help="Maximum n for a^nb^n")
    parser.add_argument("--output", type=str, required=True, help="Output .nfa file path")
    args = parser.parse_args()

    generate_anbn_nfa(args.n, args.output)
