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
    auto start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::intersection(&intersect_aut, lhs, rhs);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "intersection: " << elapsed.count() << "\n";

    Nfa concat_aut;
    start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::concatenate(&concat_aut, lhs, rhs);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "concatenation: " << elapsed.count() << "\n";

    Nfa union_aut;
    start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::uni(&union_aut, lhs, rhs);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "union: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    Mata::Nfa::Algorithms::is_included_naive(lhs, rhs, &alphabet);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "naive-inclusion: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    Mata::Nfa::Algorithms::is_included_antichains(lhs, rhs, &alphabet);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "antichain-inclusion: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
