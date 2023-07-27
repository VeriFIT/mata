// example6.cc - mintermization of automaton

#include "mata/parser/inter-aut.hh"
#include "mata/parser/mintermization.hh"
#include "mata/nfa/nfa.hh"

#include <iostream>
#include <fstream>

using namespace Mata::Nfa;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    Mata::Parser::Parsed parsed;
    Nfa aut;
    std::unordered_map<std::string, Mata::Symbol> stsm;
    try {
        parsed = Mata::Parser::parse_mf(fs, true);
        fs.close();

        std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);
        for (const auto& ia : inter_auts) {
            Mata::Mintermization mintermization;
            std::cout << ia << '\n';
            if ((ia.is_nfa() || ia.is_afa()) && ia.alphabet_type == Mata::IntermediateAut::AlphabetType::BITVECTOR) {
                const auto& aut = mintermization.mintermize(ia);
                assert(ia.transitions.size() <= aut.transitions.size());
                std::cout << aut << '\n';
            }
        }
    } catch (const std::exception& ex) {
        fs.close();
        std::cerr << "libMATA error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
