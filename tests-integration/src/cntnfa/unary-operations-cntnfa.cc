#include "../utils/utils.hh"

#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/plumbing.hh"
#include "mata/cntnfa/algorithms.hh"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    Cntnfa aut{};
    mata::OnTheFlyAlphabet alphabet{};
    load_counter_automaton(filename, aut, alphabet);

    Cntnfa compl_aut;
    TIME_BEGIN(cntnfa_complement);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    mata::cntnfa::plumbing::complement(&compl_aut, aut, alphabet);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_complement);

    Cntnfa min_compl_aut;
    TIME_BEGIN(cntnfa_complement_and_minize);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    mata::cntnfa::plumbing::complement(&min_compl_aut, aut, alphabet, {{ "algorithm", "classical"}, { "minimize", "true"}});
    // > END OF PROFILED CODE
    TIME_END(cntnfa_complement_and_minize);

    Cntnfa revert_aut;
    TIME_BEGIN(cntnfa_revert);
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    mata::cntnfa::plumbing::revert(&revert_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_revert);

    Cntnfa reduced_aut;
    TIME_BEGIN(cntnfa_reduce_and_trim);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Cntnfa trimmed{ aut };
    mata::cntnfa::plumbing::reduce(&reduced_aut, trimmed.trim());
    // > END OF PROFILED CODE
    TIME_END(cntnfa_reduce_and_trim);

    Cntnfa untrimmed_reduced_aut;
    TIME_BEGIN(cntnfa_reduce);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    mata::cntnfa::plumbing::reduce(&untrimmed_reduced_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_reduce);

    Cntnfa minimized_aut;
    TIME_BEGIN(cntnfa_minimize);
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    mata::cntnfa::plumbing::minimize(&minimized_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_minimize);

    Cntnfa det_aut;
    TIME_BEGIN(cntnfa_determinize);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    mata::cntnfa::plumbing::determinize(&det_aut, aut);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_determinize);

    TIME_BEGIN(cntnfa_naive_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::cntnfa::algorithms::is_universal_naive(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_naive_universality);

    TIME_BEGIN(cntnfa_antichain_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::cntnfa::algorithms::is_universal_antichains(aut, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(cntnfa_antichain_universality);

    return EXIT_SUCCESS;
}
