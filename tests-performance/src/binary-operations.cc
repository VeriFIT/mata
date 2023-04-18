/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */
#include <mata/inter-aut.hh>
#include <mata/nfa.hh>
#include <mata/nfa-plumbing.hh>
#include <mata/nfa-algorithms.hh>
#include <mata/mintermization.hh>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

const bool SKIP_MINTERMIZATION = false;

int load_automaton(std::string filename, Nfa& aut, Mata::StringToSymbolMap& stsm, std::string aut_name) {
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
            std::cout << "mintermization-" << aut_name<< ":" << elapsed.count() << "\n";
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
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::string lhs_filename = argv[1];
    std::string rhs_filename = argv[2];

    Nfa lhs;
    Nfa rhs;
    Mata::StringToSymbolMap lhs_stsm;
    Mata::StringToSymbolMap rhs_stsm;
    if (load_automaton(lhs_filename, lhs, lhs_stsm, "lhs") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (load_automaton(rhs_filename, rhs, rhs_stsm, "rhs") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    Mata::OnTheFlyAlphabet alph{ lhs_stsm };
    alph.add_symbols_from(rhs_stsm);

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    Nfa intersect_aut;
    auto start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::intersection(&intersect_aut, lhs, rhs);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "intersection: " << elapsed.count() << "\n";

    Nfa concat_aut;
    start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::concatenate(&concat_aut, lhs, rhs);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "concatenation: " << elapsed.count() << "\n";

    Nfa union_aut;
    start = std::chrono::system_clock::now();
    Mata::Nfa::Plumbing::uni(&union_aut, lhs, rhs);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "union: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    Mata::Nfa::Algorithms::is_included_naive(lhs, rhs, &alph);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "naive-inclusion: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    Mata::Nfa::Algorithms::is_included_antichains(lhs, rhs, &alph);
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "antichain-inclusion: " << elapsed.count() << "\n";


    return EXIT_SUCCESS;
}