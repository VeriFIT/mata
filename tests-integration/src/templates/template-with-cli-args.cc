/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "utils/utils.hh"
#include "mata/nfa/nfa.hh"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

using namespace Mata::Nfa;

const bool MINTERMIZE_AUTOMATA = true;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut;
    Mata::OnTheFlyAlphabet alphabet{};
    if (load_automaton(filename, aut, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    TIME_BEGIN(tmp);

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     *   - Use alphabet alph as source alphabet       *
     *   - Use Nfa aut as source automaton            *
     *   - e.g. complement(aut, alph);                *
     **************************************************/

    TIME_END(tmp);

    return EXIT_SUCCESS;
}
