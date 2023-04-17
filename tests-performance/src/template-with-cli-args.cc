/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */
#include <mata/inter-aut.hh>
#include <mata/nfa.hh>
#include <mata/mintermization.hh>
#include <iostream>
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
            aut = construct(inter_auts[0], &stsm);
        } else {
            Mata::Mintermization mintermization;
            auto minterm_start = std::chrono::system_clock::now();
            auto mintermized = mintermization.mintermize(inter_auts);
            auto minterm_end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed = minterm_end - minterm_start;
            assert(mintermized.size() == 1);
            aut = construct(mintermized[0], &stsm);
            std::cout << "mintermization:" << elapsed.count() << "\n";
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

    Mata::OnTheFlyAlphabet alph{ stsm };
    auto start = std::chrono::system_clock::now();

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     *   - Use alphabet alph as source alphabet       *
     *   - Use Nfa aut as source automaton            *
     *   - e.g. complement(aut, alph);                *
     **************************************************/

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "time: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}