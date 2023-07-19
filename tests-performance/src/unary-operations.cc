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

using namespace Mata::Nfa;

int main(int argc, char *argv[])
{
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

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    Mata::Parser::Parsed parsed;
    Nfa aut;
    Mata::StringToSymbolMap stsm;
    const std::string nfa_str = "NFA";
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
        Mata::Mintermization mintermization;
        auto mintermized = mintermization.mintermize(inter_auts);
        assert(mintermized.size() == 1);
        aut = Mata::Nfa::Builder::construct(mintermized[0], &stsm);
    }
    catch (const std::exception& ex) {
        fs.close();
        std::cerr << "libMATA error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    Mata::OnTheFlyAlphabet alph{ stsm };
    Nfa compl_aut;
    auto start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Mata::Nfa::Plumbing::complement(&compl_aut, aut, alph);
    // > END OF PROFILED CODE
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "complement: " << elapsed.count() << "\n";

    Nfa min_compl_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Mata::Nfa::Plumbing::complement(&min_compl_aut, aut, alph, {{"algorithm", "classical"}, {"minimize", "true"}});
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "complement-and-minimize: " << elapsed.count() << "\n";

    Nfa revert_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    Mata::Nfa::Plumbing::revert(&revert_aut, aut);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "revert: " << elapsed.count() << "\n";

    Nfa reduced_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Mata::Nfa::Plumbing::reduce(&reduced_aut, aut);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "reduce-and-trim: " << elapsed.count() << "\n";

    Nfa untrimmed_reduced_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Mata::Nfa::Plumbing::reduce(&untrimmed_reduced_aut, aut, false);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "reduce: " << elapsed.count() << "\n";

    Nfa minimized_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    Mata::Nfa::Plumbing::minimize(&minimized_aut, aut);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "minimize: " << elapsed.count() << "\n";

    Nfa det_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    Mata::Nfa::Plumbing::determinize(&det_aut, aut);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "determinize: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    Mata::Nfa::Algorithms::is_universal_naive(aut, alph, nullptr);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "naive-universality: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    Mata::Nfa::Algorithms::is_universal_antichains(aut, alph, nullptr);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "antichains-universality: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
