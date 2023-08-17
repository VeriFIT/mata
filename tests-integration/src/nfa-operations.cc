/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */

#include "utils/utils.hh"

#include "mata/parser/inter-aut.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/parser/mintermization.hh"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

const bool SKIP_MINTERMIZATION{ false };

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

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(5);

    Nfa trimmed_aut = aut;
    auto start = std::chrono::system_clock::now();
    trimmed_aut.trim();
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "trim: " << elapsed.count() << "\n";

    Nfa trimmed_aut2 = aut;
    start = std::chrono::system_clock::now();
    trimmed_aut.trim_inplace();
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "trim-inplace: " << elapsed.count() << "\n";

    Nfa trimmed_aut3 = aut;
    start = std::chrono::system_clock::now();
    trimmed_aut.trim_reverting();
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "trim-reverting: " << elapsed.count() << "\n";

    Nfa trimmed_aut4 = aut;
    start = std::chrono::system_clock::now();
    trimmed_aut.get_trimmed_automaton();
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "get-trimmed-automaton: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}