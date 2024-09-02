/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "mata/nfa/algorithms.hh"
#include "mata/simlib/util/binary_relation.hh"
#include "utils/utils.hh"
#include "mata/nfa/nfa.hh"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

using namespace mata::nfa;

const bool MINTERMIZE_AUTOMATA = false;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Something is missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut;
    mata::OnTheFlyAlphabet alphabet{};
    if (load_automaton(filename, aut, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    Simlib::Util::BinaryRelation forward = algorithms::compute_relation(aut, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});
    Simlib::Util::BinaryRelation iny = algorithms::compute_relation(aut, ParameterMap{{ "relation", "simulation"}, { "direction", "iny"}});

    if (forward.size() != iny.size()){
        std::cerr << "Size is wrong";
        return EXIT_FAILURE;
    }

    for (int i = 0; i < forward.size(); i++){
        if (forward.get(i % forward.size(), i) != iny.get(i % forward.size(), i)){
            std::cerr << "Position is wrong";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
