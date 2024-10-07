/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "mata/nfa/algorithms.hh"
#include "utils/utils.hh"
#include "mata/nfa/nfa.hh"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

using namespace mata::nfa;

const bool MINTERMIZE_AUTOMATA = false;

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Something is missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];
    std::string algorithm = argv[2];

    Nfa aut;
    mata::OnTheFlyAlphabet alphabet{};
    if (load_automaton(filename, aut, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    TIME_BEGIN(relation);

    algorithms::compute_relation(aut, ParameterMap{{ "relation", "simulation"}, { "direction", algorithm}});

    TIME_END(relation);

    return EXIT_SUCCESS;
}
