/**
 * Benchmark: Automata Inclusion (b-armc-incl)
 *
 * The benchmark program reproduces the results of CADE'23 for benchmarks in directory /nfa-bench/benchmarks/automata_inclusion
 *
 * Optimal Inputs: inputs/bench-double-automata-inclusion.in
 *
 * The original benchmark had the following statistics:
 *   1. Average: 1.9s
 *   2. Median: 0.8s
 *   3. Timeouts: 0
 *
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "../utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA = true;

// Compare the inclusion of two NFAs and two CNTNFAs without annotations.
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {argv[1], argv[2]};
    mata::OnTheFlyAlphabet alphabet;
    ParameterMap params;

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    // --- NFA test ---

    std::vector<Nfa> nfa_automata;
    if (load_automata(filenames, nfa_automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        std::cerr << "Failed to load NFA automata\n";
        return EXIT_FAILURE;
    }

    // This might be less-efficient, but more readable.
    Nfa lhs_nfa = nfa_automata[0];
    Nfa rhs_nfa = nfa_automata[1];

    params["algorithm"] = "naive";
    TIME_BEGIN(nfa_inclusion_naive_merged);
    mata::nfa::is_included(lhs_nfa, rhs_nfa, &alphabet, params);
    TIME_END(nfa_inclusion_naive_merged);

    params["algorithm"] = "antichains";
    TIME_BEGIN(nfa_inclusion_antichain_merged);
    mata::nfa::is_included(lhs_nfa, rhs_nfa, &alphabet, params);
    TIME_END(nfa_inclusion_antichain_merged);

    // --- CNTNFA test ---

    std::vector<Cntnfa> cntnfa_automata;
    if (load_counter_automata(filenames, cntnfa_automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        std::cerr << "Failed to load CNTNFA automata\n";
        return EXIT_FAILURE;
    }

    // This might be less-efficient, but more readable.
    Cntnfa lhs_cntnfa = cntnfa_automata[0];
    Cntnfa rhs_cntnfa = cntnfa_automata[1];

    params["algorithm"] = "naive";
    TIME_BEGIN(cntnfa_inclusion_naive_merged);
    mata::cntnfa::is_included(lhs_cntnfa, rhs_cntnfa, &alphabet, params);
    TIME_END(cntnfa_inclusion_naive_merged);

    params["algorithm"] = "antichains";
    TIME_BEGIN(cntnfa_inclusion_antichain_merged);
    mata::cntnfa::is_included(lhs_cntnfa, rhs_cntnfa, &alphabet, params);
    TIME_END(cntnfa_inclusion_antichain_merged);

    return EXIT_SUCCESS;
}
