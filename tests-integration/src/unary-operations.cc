#include "utils/utils.hh"

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut{};
    Mata::OnTheFlyAlphabet alphabet{};
    load_automaton(filename, aut, alphabet);

    Nfa compl_aut;
    auto start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Mata::Nfa::Plumbing::complement(&compl_aut, aut, alphabet);
    // > END OF PROFILED CODE
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "complement: " << elapsed.count() << "\n";

    Nfa min_compl_aut;
    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Mata::Nfa::Plumbing::complement(&min_compl_aut, aut, alphabet, {{"algorithm", "classical"}, {"minimize", "true"}});
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
    Mata::Nfa::Algorithms::is_universal_naive(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "naive-universality: " << elapsed.count() << "\n";

    start = std::chrono::system_clock::now();
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    Mata::Nfa::Algorithms::is_universal_antichains(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    std::cout << "antichains-universality: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
