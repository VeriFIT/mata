/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */

#include "utils/utils.hh"

constexpr bool SKIP_MINTERMIZATION{ false };

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::string lhs_filename = argv[1];
    std::string rhs_filename = argv[2];

    Nfa lhs;
    Nfa rhs;
    Mata::OnTheFlyAlphabet alphabet;
    if (load_automaton(lhs_filename, lhs, alphabet, SKIP_MINTERMIZATION, "lhs") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (load_automaton(rhs_filename, rhs, alphabet, SKIP_MINTERMIZATION, "rhs") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    Nfa intersect_aut;
    TIME_BEGIN(intersection);
    Mata::Nfa::Plumbing::intersection(&intersect_aut, lhs, rhs);
    TIME_END(intersection);

    Nfa concat_aut;
    TIME_BEGIN(concatenation);
    Mata::Nfa::Plumbing::concatenate(&concat_aut, lhs, rhs);
    TIME_END(concatenation);

    Nfa union_aut;
    TIME_BEGIN(union);
    Mata::Nfa::Plumbing::uni(&union_aut, lhs, rhs);
    TIME_END(union);

    TIME_BEGIN(naive_inclusion);
    Mata::Nfa::Algorithms::is_included_naive(lhs, rhs, &alphabet);
    TIME_END(naive_inclusion);

    TIME_BEGIN(antichain_inclusion);
    Mata::Nfa::Algorithms::is_included_antichains(lhs, rhs, &alphabet);
    TIME_END(antichain_inclusion);

    return EXIT_SUCCESS;
}
