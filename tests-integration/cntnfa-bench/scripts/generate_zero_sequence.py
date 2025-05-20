import os

output_dir = "../benchmarks/large_sequence"
os.makedirs(output_dir, exist_ok=True)

def generate_nfa(n):
    lines = ["@NFA-explicit", "%Initial q0", f"%Final q{n}"]
    for i in range(n):
        lines.append(f"q{i} 0 q{i+1}")
    return "\n".join(lines)

def generate_cntnfa(n):
    lines = [
        "@CNTNFA-explicit",
        "%States q0 q1",
        "%Alphabet-auto",
        "%Initial q0",
        "%Final q1",
        "%Registers c0",
        f"q0 0 (+ c0 1) q0",
        f"q0 0 (= c0 {n - 1}) q1"
    ]
    return "\n".join(lines)

for n in range(1, 10001):
    nfa_text = generate_nfa(n)
    cntnfa_text = generate_cntnfa(n)

    nfa_path = os.path.join(output_dir, f"nfa-0-{n}.mata")
    cntnfa_path = os.path.join(output_dir, f"cntnfa-0-{n}.mata")

    with open(nfa_path, 'w') as f:
        f.write(nfa_text + "\n")

    with open(cntnfa_path, 'w') as f:
        f.write(cntnfa_text + "\n")
