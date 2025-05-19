#include "../utils/utils.hh"

#include "mata/alphabet.hh"
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

    Cntnfa cntnfa{};
    mata::OnTheFlyAlphabet alphabet{};
    load_counter_automaton(filename, cntnfa, alphabet);

    TIME_BEGIN(complement);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Cntnfa compl_cntnfa;
    mata::cntnfa::plumbing::complement(&compl_cntnfa, cntnfa, alphabet);
    // > END OF PROFILED CODE
    TIME_END(complement);

    TIME_BEGIN(complement_and_minimize);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    Cntnfa min_compl_cntnfa;
    mata::cntnfa::plumbing::complement(&min_compl_cntnfa, cntnfa, alphabet, {{ "algorithm", "classical"}, { "minimize", "true"}});
    // > END OF PROFILED CODE
    TIME_END(complement_and_minimize);

    TIME_BEGIN(revert);
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    Cntnfa revert_cntnfa;
    mata::cntnfa::plumbing::revert(&revert_cntnfa, cntnfa);
    // > END OF PROFILED CODE
    TIME_END(revert);

    TIME_BEGIN(reduce_and_trim);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Cntnfa trimmed_cntnfa{cntnfa};
    Cntnfa reduced_cntnfa;
    mata::cntnfa::plumbing::reduce(&reduced_cntnfa, trimmed_cntnfa.trim());
    // > END OF PROFILED CODE
    TIME_END(reduce_and_trim);

    TIME_BEGIN(reduce);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Cntnfa untrimmed_reduced_cntnfa;
    mata::cntnfa::plumbing::reduce(&untrimmed_reduced_cntnfa, cntnfa);
    // > END OF PROFILED CODE
    TIME_END(reduce);

    TIME_BEGIN(minimize);
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    Cntnfa minimized_cntnfa;
    mata::cntnfa::plumbing::minimize(&minimized_cntnfa, cntnfa);
    // > END OF PROFILED CODE
    TIME_END(minimize);

    TIME_BEGIN(determinize);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    Cntnfa det_cntnfa;
    mata::cntnfa::plumbing::determinize(&det_cntnfa, cntnfa);
    // > END OF PROFILED CODE
    TIME_END(determinize);

    TIME_BEGIN(naive_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::cntnfa::algorithms::is_universal_naive(cntnfa, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(naive_universality);

    TIME_BEGIN(antichain_universality);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    mata::cntnfa::algorithms::is_universal_antichains(cntnfa, alphabet, nullptr);
    // > END OF PROFILED CODE
    TIME_END(antichain_universality);

    return EXIT_SUCCESS;
}
