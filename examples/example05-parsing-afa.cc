// example5-parsing-afa.cc - parsing AFA from file

#include "mata/afa/afa.hh"
#include "mata/parser/inter-aut.hh"

#include <iostream>
#include <fstream>

using namespace Mata::Afa;

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
    Afa aut;
    std::unordered_map<std::string, Mata::Symbol> stsm;
    try {
        parsed = Mata::Parser::parse_mf(fs, true);
        fs.close();

        std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);
        for (const auto& ia : inter_auts)
            std::cout << ia << '\n';

        if (inter_auts[0].is_afa())
            aut = construct(inter_auts[0]);
        std::cout << aut << '\n';
    }
    catch (const std::exception& ex) {
        fs.close();
        std::cerr << "libMATA error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
