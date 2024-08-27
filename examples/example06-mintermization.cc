// example6.cc - mintermization of automaton

#include "mata/parser/inter-aut.hh"
#include "mata/parser/mintermization.hh"
#include "mata/nfa/nfa.hh"

#include <iostream>
#include <fstream>

using namespace mata::nfa;

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

    mata::parser::Parsed parsed;
    Nfa aut;
    try {
        parsed = mata::parser::parse_mf(fs, true);
        fs.close();

        std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
        for (const auto& ia : inter_auts) {
            mata::Mintermization<mata::BDDDomain> mintermization;
            std::cout << ia << '\n';
            if ((ia.is_nfa() || ia.is_afa()) && ia.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR) {
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
