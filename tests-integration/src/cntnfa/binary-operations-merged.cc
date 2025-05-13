/**
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
    mata::OnTheFlyAlphabet alphabet;

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    std::vector<Nfa> nfa_automata;
    if (load_automata(filenames, nfa_automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::vector<Cntnfa> cntnfa_automata;
    if (load_counter_automata(filenames, cntnfa_automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // This might be less-efficient, but more readable.
    Nfa lhs_nfa = nfa_automata[0];
    Nfa rhs_nfa = nfa_automata[1];

    Cntnfa lhs_cntnfa = cntnfa_automata[0];
    Cntnfa rhs_cntnfa = cntnfa_automata[1];

    Nfa intersect_aut_nfa;
    TIME_BEGIN(nfa_intersection_merged);
    mata::nfa::plumbing::intersection(&intersect_aut_nfa, lhs_nfa, rhs_nfa);
    TIME_END(nfa_intersection_merged);

    Cntnfa intersect_aut_cntnfa;
    TIME_BEGIN(cntnfa_intersection_merged);
    mata::cntnfa::plumbing::intersection(&intersect_aut_cntnfa, lhs_cntnfa, rhs_cntnfa);
    TIME_END(cntnfa_intersection_merged);

    Nfa concat_aut_nfa;
    TIME_BEGIN(nfa_concatenation_merged);
    mata::nfa::plumbing::concatenate(&concat_aut_nfa, lhs_nfa, rhs_nfa);
    TIME_END(nfa_concatenation_merged);

    Cntnfa concat_aut_cntnfa;
    TIME_BEGIN(cntnfa_concatenation_merged);
    mata::cntnfa::plumbing::concatenate(&concat_aut_cntnfa, lhs_cntnfa, rhs_cntnfa);
    TIME_END(cntnfa_concatenation_merged);

    Nfa union_aut_nfa;
    TIME_BEGIN(nfa_union_merged);
    mata::nfa::plumbing::union_nondet(&union_aut_nfa, lhs_nfa, rhs_nfa);
    TIME_END(nfa_union_merged);

    Cntnfa union_aut_cntnfa;
    TIME_BEGIN(cntnfa_union_merged);
    mata::cntnfa::plumbing::union_nondet(&union_aut_cntnfa, lhs_cntnfa, rhs_cntnfa);
    TIME_END(cntnfa_union_merged);

    TIME_BEGIN(nfa_naive_inclusion_merged);
    mata::nfa::algorithms::is_included_naive(lhs_nfa, rhs_nfa, &alphabet);
    TIME_END(nfa_naive_inclusion_merged);

    TIME_BEGIN(cntnfa_naive_inclusion_merged);
    mata::cntnfa::algorithms::is_included_naive(lhs_cntnfa, rhs_cntnfa, &alphabet);
    TIME_END(cntnfa_naive_inclusion_merged);

    TIME_BEGIN(nfa_antichain_inclusion_merged);
    mata::nfa::algorithms::is_included_antichains(lhs_nfa, rhs_nfa, &alphabet);
    TIME_END(nfa_antichain_inclusion_merged);

    TIME_BEGIN(cntnfa_antichain_inclusion_merged);
    mata::cntnfa::algorithms::is_included_antichains(lhs_cntnfa, rhs_cntnfa, &alphabet);
    TIME_END(cntnfa_antichain_inclusion_merged);

    return EXIT_SUCCESS;
}
