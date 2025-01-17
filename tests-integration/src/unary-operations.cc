#include "utils/utils.hh"

#include "mata/nfa/nfa.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nfa/algorithms.hh"

#include <iostream>
#include <chrono>
#include <string>

using namespace mata::nfa;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Nfa aut{};
    mata::OnTheFlyAlphabet alphabet{};
    load_automaton(filename, aut, alphabet);

    Nfa compl_aut;
    TIME_BEGIN(complement);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    mata::nfa::plumbing::complement(&compl_aut, aut, alphabet);
    // > END OF PROFILED CODE
    TIME_END(complement);

    Nfa min_compl_aut;
    TIME_BEGIN(complement_and_minize);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    mata::nfa::plumbing::complement(&min_compl_aut, aut, alphabet, {{ "algorithm", "classical"}, { "minimize", "true"}});
    // > END OF PROFILED CODE
    TIME_END(complement_and_minize);

    Nfa revert_aut;
    TIME_BEGIN(revert);
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    mata::nfa::plumbing::revert(&revert_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(revert);

    Nfa reduced_aut;
    TIME_BEGIN(reduce_and_trim);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Nfa trimmed{ aut };
    mata::nfa::plumbing::reduce(&reduced_aut, trimmed.trim());
    // > END OF PROFILED CODE
    TIME_END(reduce_and_trim);

    Nfa untrimmed_reduced_aut;
    TIME_BEGIN(reduce);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    mata::nfa::plumbing::reduce(&untrimmed_reduced_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(reduce);

    Nfa minimized_aut;
    TIME_BEGIN(minimize);
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    mata::nfa::plumbing::minimize(&minimized_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(minimize);

    Nfa det_aut;
    TIME_BEGIN(determinize);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    mata::nfa::plumbing::determinize(&det_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(determinize);

    Nfa det_aut_boost;
    TIME_BEGIN(determinize_boost);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    mata::nfa::plumbing::determinize_boost(&det_aut_boost, aut);
    TIME_END(determinize_boost);

    TIME_BEGIN(naive_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::nfa::algorithms::is_universal_naive(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(naive_universality);

    TIME_BEGIN(antichain_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::nfa::algorithms::is_universal_antichains(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(antichain_universality);

    return EXIT_SUCCESS;
}
