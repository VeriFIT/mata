/**
 * Benchmark: Bool_comb (b-param)
 *
 * The benchmark program reproduces the results of CADE'23 for benchmarks in directory /nfa-bench/benchmarks/bool_comb/cox
 *
 * Optimal Inputs: inputs/bench-double-bool-comb-cox.in
 *
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "utils/utils.hh"
#include "mata/nfa/algorithms.hh"

constexpr bool MINTERMIZE_AUTOMATA{ true};

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames;
    for (int i = 1; i < argc; ++i) {
        filenames.push_back(argv[i]);
    }
    std::vector<Nfa> automata;
    Mata::OnTheFlyAlphabet alphabet;
    if (load_automata(filenames, automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    TIME_BEGIN(intersection_emptiness);
    Nfa result = automata[0];
    for (int i = 1; i < argc - 1; ++i) {
        result = intersection(result, automata[i]);
    }
    is_lang_empty(result);
    TIME_END(intersection_emptiness);

    return EXIT_SUCCESS;
}
