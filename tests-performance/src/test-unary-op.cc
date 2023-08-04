/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */
#include "mata/parser/inter-aut.hh"
#include "mata/nfa/nfa.hh"
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

const bool SKIP_MINTERMIZATION = false;

int load_automaton(std::string filename, Nfa& aut, Mata::StringToSymbolMap& stsm) {
    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    Mata::Parser::Parsed parsed;
    const std::string nfa_str = "NFA";
    const std::string bits_str = "-bits";
    try {
        parsed = Mata::Parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error(
                    "The number of sections in the input file is not 1\n");
        }
        if (parsed[0].type.compare(0, nfa_str.length(), nfa_str) != 0) {
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);

        if (SKIP_MINTERMIZATION or parsed[0].type.compare(parsed[0].type.length() - bits_str.length(), bits_str.length(), bits_str) != 0) {
            aut = Mata::Nfa::Builder::construct(inter_auts[0], &stsm);
        } else {
            Mata::Mintermization mintermization;
            auto mintermized = mintermization.mintermize(inter_auts);
            assert(mintermized.size() == 1);
            aut = Mata::Nfa::Builder::construct(mintermized[0], &stsm);
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        fs.close();
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut;
    Mata::StringToSymbolMap stsm;
    if (load_automaton(filename, aut, stsm) != EXIT_SUCCESS) {
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
