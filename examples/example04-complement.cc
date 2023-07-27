// example04-complement.cc - complementing an automaton

#include "mata/nfa/nfa.hh"
#include "mata/nfa/builder.hh"

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
    Mata::StringToSymbolMap stsm;
    try {
        parsed = Mata::Parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error(
                "The number of sections in the input file is not 1\n");
        }
        if (parsed[0].type != "NFA") {
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        aut = Mata::Nfa::Builder::construct(parsed[0], &stsm);
    }
    catch (const std::exception& ex) {
        fs.close();
        std::cerr << "libMATA error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    Mata::OnTheFlyAlphabet alph{ stsm };
    Nfa cmpl = complement(aut, alph);

    std::cout << std::to_string(cmpl);
    return EXIT_SUCCESS;
}
