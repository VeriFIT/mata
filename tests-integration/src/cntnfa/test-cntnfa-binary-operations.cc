/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 *  - Load NFA and CNTNFA using the same methods for NFA and test them
 */

#include "../utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA = true;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {argv[1], argv[2]};
    std::vector<Cntnfa> cntnfas;
    mata::OnTheFlyAlphabet alphabet;

    if (load_counter_automata(filenames, cntnfas, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // This might be less-efficient, but more readable.
    Cntnfa cntnfa_lhs = cntnfas[0];
    Cntnfa cntnfa_rhs = cntnfas[1];

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    // CNTNFA intersection
    TIME_BEGIN(intersection);
    Cntnfa intersect_cntnfa;
    mata::cntnfa::plumbing::intersection(&intersect_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(intersection);

    // CNTNFA concatenation
    TIME_BEGIN(concatenation);
    Cntnfa concat_cntnfa;
    mata::cntnfa::plumbing::concatenate(&concat_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(concatenation);

    // CNTNFA union
    TIME_BEGIN(union);
    Cntnfa union_cntnfa;
    mata::cntnfa::plumbing::union_nondet(&union_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(union);

    // CNTNFA naive inclusion
    TIME_BEGIN(naive_inclusion);
    mata::cntnfa::algorithms::is_included_naive(cntnfa_lhs, cntnfa_rhs, &alphabet);
    TIME_END(naive_inclusion);

    // CNTNFA antichain inclusion
    TIME_BEGIN(antichain_inclusion);
    mata::cntnfa::algorithms::is_included_antichains(cntnfa_lhs, cntnfa_rhs, &alphabet);
    TIME_END(antichain_inclusion);

    return EXIT_SUCCESS;
}
