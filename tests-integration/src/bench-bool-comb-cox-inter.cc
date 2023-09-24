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

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

 //   TIME_BEGIN(intersection);
 //   Nfa intersect_aut = intersection(lhs, rhs);
 //   TIME_END(intersection);

    TIME_BEGIN(intersection2);
    Nfa intersect_aut2 = intersection2(lhs, rhs);
    TIME_END(intersection2);

//    TIME_BEGIN(emptiness_check);
//    intersect_aut.is_lang_empty();
//    TIME_END(emptiness_check);

    return EXIT_SUCCESS;
}
