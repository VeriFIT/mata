/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */

#include "mata/alphabet.hh"
#include "utils/utils.hh"

#include "mata/nfa/nfa.hh"

#include <iostream>
#include <string>

using namespace Mata::Nfa;

const bool SKIP_MINTERMIZATION = false;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut;
    Mata::OnTheFlyAlphabet alphabet{};
    if (load_automaton(filename, aut, alphabet, SKIP_MINTERMIZATION) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // trim test
    Nfa aut_trim(aut);
    aut_trim.trim();
    std::cout << "trim:" << (Mata::Nfa::are_equivalent(aut, aut_trim) ? "ok" : "fail") << std::endl;

    // minimization test
    Nfa aut_min(aut);
    aut_min = Mata::Nfa::minimize(aut_min);
    std::cout << "minimize:" << (Mata::Nfa::are_equivalent(aut, aut_min) ? "ok" : "fail") << std::endl;

    return EXIT_SUCCESS;
}
