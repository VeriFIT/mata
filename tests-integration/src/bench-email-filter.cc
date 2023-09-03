/**
 * Benchmark: Email Filter (b-regex)
 *
 * The benchmark program reproduces the results of CADE'23 for benchmarks in directory /nfa-bench/benchmarks/email_filter
 *
 * Optimal Inputs: inputs/bench-quintuple-email-filter_values.in
 *
 * The original benchmark had the following statistics:
 *   1. Average: 0.2s
 *   2. Median: 0.1s
 *   3. Timeouts: 0
 *
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "utils/utils.hh"
#include "mata/nfa/algorithms.hh"

constexpr bool MINTERMIZE_AUTOMATA{ true};

int main(int argc, char *argv[]) {
    if (argc != 6) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {
        argv[1], argv[2], argv[3], argv[4], argv[5]
    };
    std::vector<Nfa> automata;
    mata::OnTheFlyAlphabet alphabet;
    if (load_automata(filenames, automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    ParameterMap params;

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    Nfa intersect_aut;
    params["algorithm"] = "naive";
    TIME_BEGIN(automata_inclusion_naive);
    intersect_aut = intersection(intersection(intersection(automata[0], automata[1]), automata[2]), automata[3]);
    is_included(automata[4], intersect_aut, &alphabet, params);
    TIME_END(automata_inclusion_naive);

    params["algorithm"] = "antichains";
    TIME_BEGIN(automata_inclusion_antichain);
    intersect_aut = intersection(intersection(intersection(automata[0], automata[1]), automata[2]), automata[3]);
    is_included(automata[4], intersect_aut, &alphabet, params);
    TIME_END(automata_inclusion_antichain);

    return EXIT_SUCCESS;
}
