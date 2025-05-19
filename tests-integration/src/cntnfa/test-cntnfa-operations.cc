/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 *  - Load NFA and CNTNFA using the same methods for NFA and test them
 */

#include "../utils/utils.hh"

#include "mata/nfa/nfa.hh"
#include "mata/cntnfa/cntnfa.hh"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>

const bool MINTERMIZE_AUTOMATA = true;
constexpr int NUM_ITERATIONS = 200;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa nfa;
    mata::OnTheFlyAlphabet alphabet_nfa{};
    if (load_automaton(filename, nfa, alphabet_nfa, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    Cntnfa cntnfa;
    mata::OnTheFlyAlphabet alphabet_cntnfa{};
    if (load_counter_automaton(filename, cntnfa, alphabet_cntnfa, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(5);

    TIME_BEGIN(trim_nfa);
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa trimmed_nfa = nfa;
        trimmed_nfa.trim();
    }
    TIME_END(trim_nfa);

    TIME_BEGIN(trim_cntnfa);
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa trimmed_cntnfa = cntnfa;
        trimmed_cntnfa.trim();
    }
    TIME_END(trim_cntnfa);

    return EXIT_SUCCESS;
}
