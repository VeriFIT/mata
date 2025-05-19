#include "../utils/utils.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nfa/algorithms.hh"

#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/plumbing.hh"
#include "mata/cntnfa/algorithms.hh"

#include <iostream>
#include <string>

constexpr int NUM_ITERATIONS = 200;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input file missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];

    // Initialize NFA

    Nfa nfa{};
    mata::OnTheFlyAlphabet alphabet_nfa{};
    load_automaton(filename, nfa, alphabet_nfa);

    // Initialize CNTNFA

    Cntnfa cntnfa{};
    mata::OnTheFlyAlphabet alphabet_cntnfa{};
    load_counter_automaton(filename, cntnfa, alphabet_cntnfa);

    // Complement NFA

    TIME_BEGIN(complement_nfa);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa compl_nfa;
        mata::nfa::plumbing::complement(&compl_nfa, nfa, alphabet_nfa);
    }
    // > END OF PROFILED CODE
    TIME_END(complement_nfa);

    // Complement CNTNFA

    TIME_BEGIN(complement_cntnfa);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa compl_cntnfa;
        mata::cntnfa::plumbing::complement(&compl_cntnfa, cntnfa, alphabet_cntnfa);
    }
    // > END OF PROFILED CODE
    TIME_END(complement_cntnfa);

    // Complement and minimize NFA

    TIME_BEGIN(complement_and_minimize_nfa);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa min_compl_nfa;
        mata::nfa::plumbing::complement(&min_compl_nfa, nfa, alphabet_nfa, {{ "algorithm", "classical"}, { "minimize", "true"}});
    }
    // > END OF PROFILED CODE
    TIME_END(complement_and_minimize_nfa);

    // Complement and minimize CNTNFA

    Cntnfa min_compl_cntnfa;
    TIME_BEGIN(complement_and_minimize_cntnfa);
    // > START OF PROFILED CODE
    // Only complement and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa min_compl_cntnfa;
        mata::cntnfa::plumbing::complement(&min_compl_cntnfa, cntnfa, alphabet_cntnfa, {{ "algorithm", "classical"}, { "minimize", "true"}});
    }
    // > END OF PROFILED CODE
    TIME_END(complement_and_minimize_cntnfa);

    // Revert NFA

    TIME_BEGIN(revert_nfa);
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa revert_nfa;
        mata::nfa::plumbing::revert(&revert_nfa, nfa);
    }
    // > END OF PROFILED CODE
    TIME_END(revert_nfa);

    // Revert CNTNFA

    TIME_BEGIN(revert_cntnfa);
    // > START OF PROFILED CODE
    // Only revert and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa revert_cntnfa;
        mata::cntnfa::plumbing::revert(&revert_cntnfa, cntnfa);
    }
    // > END OF PROFILED CODE
    TIME_END(revert_cntnfa);

    // Reduce and trim NFA

    TIME_BEGIN(reduce_and_trim_nfa);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Nfa trimmed_nfa{ nfa };
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa reduced_nfa;
        mata::nfa::plumbing::reduce(&reduced_nfa, trimmed_nfa.trim());
    }
    // > END OF PROFILED CODE
    TIME_END(reduce_and_trim_nfa);

    // Reduce and trim CNTNFA

    TIME_BEGIN(reduce_and_trim_cntnfa);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    Cntnfa trimmed_cntnfa{ cntnfa };
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa reduced_cntnfa;
        mata::cntnfa::plumbing::reduce(&reduced_cntnfa, trimmed_cntnfa.trim());
    }
    // > END OF PROFILED CODE
    TIME_END(reduce_and_trim_cntnfa);

    // Reduce NFA

    TIME_BEGIN(reduce_nfa);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa untrimmed_reduced_nfa;
        mata::nfa::plumbing::reduce(&untrimmed_reduced_nfa, nfa);
    }
    // > END OF PROFILED CODE
    TIME_END(reduce_nfa);

    // Reduce CNTNFA

    TIME_BEGIN(reduce_cntnfa);
    // > START OF PROFILED CODE
    // Only reduce and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa untrimmed_reduced_cntnfa;
        mata::cntnfa::plumbing::reduce(&untrimmed_reduced_cntnfa, cntnfa);
    }
    // > END OF PROFILED CODE
    TIME_END(reduce_cntnfa);

    // Minimize NFA

    TIME_BEGIN(minimize_nfa);
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa minimized_nfa;
        mata::nfa::plumbing::minimize(&minimized_nfa, nfa);
    }
    // > END OF PROFILED CODE
    TIME_END(minimize_nfa);

    // Minimize CNTNFA

    TIME_BEGIN(minimize_cntnfa);
    // > START OF PROFILED CODE
    // Only minimize and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa minimized_cntnfa;
        mata::cntnfa::plumbing::minimize(&minimized_cntnfa, cntnfa);
    }
    // > END OF PROFILED CODE
    TIME_END(minimize_cntnfa);

    // Determinize NFA

    TIME_BEGIN(determinize_nfa);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Nfa det_nfa;
        mata::nfa::plumbing::determinize(&det_nfa, nfa);
    }
    // > END OF PROFILED CODE
    TIME_END(determinize_nfa);

    // Determinize CNTNFA

    TIME_BEGIN(determinize_cntnfa);
    // > START OF PROFILED CODE
    // Only determinize and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        Cntnfa det_cntnfa;
        mata::cntnfa::plumbing::determinize(&det_cntnfa, cntnfa);
    }
    // > END OF PROFILED CODE
    TIME_END(determinize_cntnfa);

    // Naive universality NFA

    TIME_BEGIN(naive_universality_nfa);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        mata::nfa::algorithms::is_universal_naive(nfa, alphabet_nfa, nullptr);
    }
    // > END OF PROFILED CODE
    TIME_END(naive_universality_nfa);

    // Naive universality CNTNFA

    TIME_BEGIN(naive_universality_cntnfa);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        mata::cntnfa::algorithms::is_universal_naive(cntnfa, alphabet_cntnfa, nullptr);
    }
    // > END OF PROFILED CODE
    TIME_END(naive_universality_cntnfa);

    // Antichain universality NFA

    TIME_BEGIN(antichain_universality_nfa);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        mata::nfa::algorithms::is_universal_antichains(nfa, alphabet_nfa, nullptr);
    }
    // > END OF PROFILED CODE
    TIME_END(antichain_universality_nfa);

    // Antichain universality CNTNFA

    TIME_BEGIN(antichain_universality_cntnfa);
    // > START OF PROFILED CODE
    // Only universality check and its callees will be measured
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        mata::cntnfa::algorithms::is_universal_antichains(cntnfa, alphabet_cntnfa, nullptr);
    }
    // > END OF PROFILED CODE
    TIME_END(antichain_universality_cntnfa);

    return EXIT_SUCCESS;
}
