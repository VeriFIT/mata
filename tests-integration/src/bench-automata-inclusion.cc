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

#include "utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA{ true};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {argv[1], argv[2]};
    std::vector<Nfa> automata;
    mata::OnTheFlyAlphabet alphabet;
    if (load_automata(filenames, automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    // This might be less-efficient, but more readable.
    Nfa lhs = automata[0];
    Nfa rhs = automata[1];

    ParameterMap params;

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    params["algorithm"] = "naive";
    TIME_BEGIN(automata_inclusion_naive);
    bool test = mata::nfa::is_included(lhs, rhs, &alphabet, params);
    std::cout << "Result: " << test << std::endl;
    TIME_END(automata_inclusion_naive);


    params["algorithm"] = "antichains";
    TIME_BEGIN(automata_inclusion_antichain);
    bool test2 = mata::nfa::is_included(lhs, rhs, &alphabet, params);
    std::cout << "Result: " << test2 << std::endl;
    TIME_END(automata_inclusion_antichain);

    #ifdef USE_BOOST
    TIME_BEGIN(automata_inclusion_boost_antichain);
    params["algorithm"] = "boost";
    bool test3 = mata::nfa::is_included(lhs, rhs, &alphabet, params);
    std::cout << "Result: " << test3 << std::endl;
    TIME_END(automata_inclusion_boost_antichain);
    #endif

    return EXIT_SUCCESS;
}
