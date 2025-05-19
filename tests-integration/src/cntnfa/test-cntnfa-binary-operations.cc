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

    std::vector<Nfa> nfas;
    std::vector<Cntnfa> cntnfas;

    mata::OnTheFlyAlphabet nfa_alphabet;
    mata::OnTheFlyAlphabet cntnfa_alphabet;

    // Load NFAs
    if (load_automata(filenames, nfas, nfa_alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Load CNTNFAs
    if (load_counter_automata(filenames, cntnfas, cntnfa_alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // This might be less-efficient, but more readable.
    Nfa nfa_lhs = nfas[0];
    Nfa nfa_rhs = nfas[1];

    Cntnfa cntnfa_lhs = cntnfas[0];
    Cntnfa cntnfa_rhs = cntnfas[1];

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    // NFA intersection
    Nfa intersect_nfa;
    TIME_BEGIN(intersection_nfa);
    mata::nfa::plumbing::intersection(&intersect_nfa, nfa_lhs, nfa_rhs);
    TIME_END(intersection_nfa);

    // CNTNFA intersection
    Cntnfa intersect_cntnfa;
    TIME_BEGIN(intersection_cntnfa);
    mata::cntnfa::plumbing::intersection(&intersect_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(intersection_cntnfa);

    // NFA concatenation
    Nfa concat_nfa;
    TIME_BEGIN(concatenation_nfa);
    mata::nfa::plumbing::concatenate(&concat_nfa, nfa_lhs, nfa_rhs);
    TIME_END(concatenation_nfa);

    // CNTNFA concatenation
    Cntnfa concat_cntnfa;
    TIME_BEGIN(concatenation_cntnfa);
    mata::cntnfa::plumbing::concatenate(&concat_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(concatenation_cntnfa);

    // NFA union
    Nfa union_nfa;
    TIME_BEGIN(union_nfa);
    mata::nfa::plumbing::union_nondet(&union_nfa, nfa_lhs, nfa_rhs);
    TIME_END(union_nfa);

    // CNTNFA union
    Cntnfa union_cntnfa;
    TIME_BEGIN(union_cntnfa);
    mata::cntnfa::plumbing::union_nondet(&union_cntnfa, cntnfa_lhs, cntnfa_rhs);
    TIME_END(union_cntnfa);

    // NFA naive inclusion
    TIME_BEGIN(naive_inclusion_nfa);
    mata::nfa::algorithms::is_included_naive(nfa_lhs, nfa_rhs, &nfa_alphabet);
    TIME_END(naive_inclusion_nfa);

    // CNTNFA naive inclusion
    TIME_BEGIN(naive_inclusion_cntnfa);
    mata::cntnfa::algorithms::is_included_naive(cntnfa_lhs, cntnfa_rhs, &cntnfa_alphabet);
    TIME_END(naive_inclusion_cntnfa);

    // NFA antichain inclusion
    TIME_BEGIN(antichain_inclusion_nfa);
    mata::nfa::algorithms::is_included_antichains(nfa_lhs, nfa_rhs, &nfa_alphabet);
    TIME_END(antichain_inclusion_nfa);

    // CNTNFA antichain inclusion
    TIME_BEGIN(antichain_inclusion_cntnfa);
    mata::cntnfa::algorithms::is_included_antichains(cntnfa_lhs, cntnfa_rhs, &cntnfa_alphabet);
    TIME_END(antichain_inclusion_cntnfa);

    return EXIT_SUCCESS;
}
