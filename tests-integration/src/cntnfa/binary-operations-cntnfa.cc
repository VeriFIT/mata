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

    Cntnfa intersect_aut;
    TIME_BEGIN(intersection);
    mata::cntnfa::plumbing::intersection(&intersect_aut, lhs, rhs);
    TIME_END(intersection);

    Cntnfa concat_aut;
    TIME_BEGIN(concatenation);
    mata::cntnfa::plumbing::concatenate(&concat_aut, lhs, rhs);
    TIME_END(concatenation);

    Cntnfa union_aut;
    TIME_BEGIN(union);
    mata::cntnfa::plumbing::union_nondet(&union_aut, lhs, rhs);
    TIME_END(union);

    TIME_BEGIN(naive_inclusion);
    mata::cntnfa::algorithms::is_included_naive(lhs, rhs, &alphabet);
    TIME_END(naive_inclusion);

    TIME_BEGIN(antichain_inclusion);
    mata::cntnfa::algorithms::is_included_antichains(lhs, rhs, &alphabet);
    TIME_END(antichain_inclusion);

    return EXIT_SUCCESS;
}
