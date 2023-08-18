/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */

/* This example
 * https://pajda.fit.vutbr.cz/ifiedortom/afa-comparison-benchmarks/-/blob/master/email_filter/7-8-9-32-52/result.emp
 * from CADE'23 experiment is supposed to take long.
 */

#include "utils/utils.hh"

constexpr bool SKIP_MINTERMIZATION{ true };

int main(int argc, char *argv[]) {
    if (argc != 1) {
        std::cerr << "This guy does not take parameters.\n";
        return EXIT_FAILURE;
    }

    Nfa aut7;
    Nfa aut8;
    Nfa aut9;
    Nfa aut32;
    Nfa aut52;

    Mata::OnTheFlyAlphabet alphabet;

    if (load_automaton("../automata/b-armc-incl-harder/aut7.mata", aut7, alphabet, SKIP_MINTERMIZATION, "aut7") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "aut7 parsed" << "\n";
    aut7.print_to_mata(std::cout);
    if (load_automaton("../automata/b-armc-incl-harder/aut8.mata", aut8, alphabet, SKIP_MINTERMIZATION, "aut8") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "aut8 parsed" << "\n";
    aut8.print_to_mata(std::cout);
    if (load_automaton("../automata/b-armc-incl-harder/aut9.mata", aut9, alphabet, SKIP_MINTERMIZATION, "aut9") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "aut9 parsed" << "\n";
    aut9.print_to_mata(std::cout);
    if (load_automaton("../automata/b-armc-incl-harder/aut32.mata", aut32, alphabet, SKIP_MINTERMIZATION, "aut32") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "aut32 parsed" << "\n";
    aut32.print_to_mata(std::cout);
    if (load_automaton("../automata/b-armc-incl-harder/aut52.mata", aut52, alphabet, SKIP_MINTERMIZATION, "aut52") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "aut52 parsed" << "\n";
    aut52.print_to_mata(std::cout);

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    using namespace Mata::Nfa::Algorithms;

    auto start = std::chrono::system_clock::now();
    auto end = std::chrono::system_clock::now();
    Nfa aut75 = intersection(intersection(intersection(intersection(aut7,aut8),aut9),aut32),aut52);
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "intersection: " << elapsed.count() << "\n";
    aut75.print_to_mata(std::cout);

    start = std::chrono::system_clock::now();
    Mata::Nfa::Algorithms::is_included_antichains(aut75, aut52, &alphabet);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "antichain-inclusion: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
