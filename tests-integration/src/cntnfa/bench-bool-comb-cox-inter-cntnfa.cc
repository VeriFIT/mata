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

#include "../utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA = true;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {argv[1], argv[2]};
    std::vector<Cntnfa> automata;
    mata::OnTheFlyAlphabet alphabet;
    if (load_counter_automata(filenames, automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    // This might be less-efficient, but more readable.
    Cntnfa lhs = automata[0];
    Cntnfa rhs = automata[1];

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    TIME_BEGIN(cntnfa_intersection);
    Cntnfa intersect_aut = intersection(rhs, rhs);
    TIME_END(cntnfa_intersection);

    TIME_BEGIN(cntnfa_emptiness_check);
    intersect_aut.is_lang_empty();
    TIME_END(cntnfa_emptiness_check);

    return EXIT_SUCCESS;
}
