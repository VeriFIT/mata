// TODO: Insert file header.

#ifndef MATA_TESTS_PERFORMANCE_UTILS_HH
#define MATA_TESTS_PERFORMANCE_UTILS_HH

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
#include <cstring>

using namespace mata::nfa;

/**
 * @brief Load automaton from file at @p filename into @p aut, using @p alphabet for symbols on transitions.
 *
 * @param[in] filename Path to the file with automaton to load.
 * @param[out] aut Automaton instance to load into.
 * @param[out] alphabet Alphabet to use for symbols on transitions.
 * @param[in] mintermize_automata Whether to mintermize the input loaded automaton.
 *   (Note, that if you want to mintermize multiple automata together, either use `load_automata` or
 *   `load_intermediate_automaton` and mintermize yourself.)
 * @return 0 if loading the automaton succeeded. Otherwise value != 0 if loading failed.
 */
int load_automaton(
        const std::string& filename,
        Nfa& aut,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata = true
);

/**
 * @brief Load automata from list of files at @p filename into list of automata @p aut,
 * using @p alphabet for symbols on transitions.
 *
 * @param[in] filenames Paths to the file with automaton to load.
 * @param[out] auts Vector of instances to load into.
 * @param[out] alphabet Alphabet to use for symbols on transitions.
 * @param[in] mintermize_automata Whether to mintermize the input loaded automaton.
 * @return 0 if loading the automaton succeeded. Otherwise value != 0 if loading failed.
 */
int load_automata(
        std::vector<std::string>& filenames,
        std::vector<Nfa>& auts,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata = true
);

/**
 * @brief Load automaton from file at @p filename into @p inter_aut
 *
 * @param[in] filename Path to the file with automaton to load.
 * @param[out] inter_aut Intermediate automaton instance to load into.
 * @return 0 if loading the automaton succeeded. Otherwise value != 0 if loading failed.
 */
int load_intermediate_automaton(
        const std::string& filename,
        std::vector<mata::IntermediateAut>& out_inter_auts
);

/*
 * Use to print elapsed time of set of timers with user-defined prefix `timer`
 */
#define TIME_PRINT(timer) std::cout << #timer ": " << timer##_elapsed.count() << "\n" << std::flush

/*
 * Use to create initial timer with user-defined prefix `timer`

 */
#define TIME_BEGIN(timer) auto timer##_start = std::chrono::system_clock::now()

/*
 * Use to create final timer with user-defined prefix `timer`

 * and print time elapsed between initial and final timers.
 */
#define TIME_END(timer) do { \
        auto timer##_end = std::chrono::system_clock::now(); \
        std::chrono::duration<double> timer##_elapsed = timer##_end - timer##_start; \
        TIME_PRINT(timer); \
    } while(0)

/*
 * Use to profile single statement.
 *
 * Warning: typing `,` in your statements might cause trouble,
 *   e.g., when used as `TIME_STATEMENT(intersection, intersection(lhs, rhs))`.
 *   The C/C++ macros are not omnipotent, and might consider the `,` as delimiter of parameters.
 *   Use `TIME_BLOCK` instead then, which might be less error-prone.
 *
 * ```c
 *    TIME_STATEMENT(size, lhs.size() == rhs.size());
 * ```
 */
#define TIME_STATEMENT(timer, stmt) do { \
        TIME_BEGIN(timer); \
        stmt; \
        TIME_END(timer); \
    } while(0)

/*
 * Use to profile multiple statements in block.
 *
 * Warning: this might fail for some blocks. Use pair of `TIME_BEGIN()` and `TIME_END()` instead then.
 *
 * ```c
 *    TIME_BLOCK(intersection_with_complement,
 *        result = intersection(lhs, rhs);
 *        complement(result)
 *    );
 * ```
 */
#define TIME_BLOCK(timer, ...) do { \
        TIME_BEGIN(timer); \
        __VA_ARGS__ \
        TIME_END(timer); \
    } while(0)

#endif // MATA_TESTS_PERFORMANCE_UTILS_HH.
