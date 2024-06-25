/* nfa-plumbings.hh -- Wrapping up different supporting functions.
 */

#ifndef MATA_NFA_PLUMBING_HH_
#define MATA_NFA_PLUMBING_HH_

#include "nfa.hh"
#include "builder.hh"

using namespace mata::nfa::builder;

/**
 * Simplified NFA API, used in binding to call NFA algorithms.
 *
 * In particular, this mostly includes operations and checks, that do not return Automaton,
 * but instead take resulting automaton as pointer (e.g. `void f(Nfa* result, const Nfa& lhs, const Nfa& rhs)`).
 */
namespace mata::nfa::plumbing {

inline void get_elements(StateSet* element_set, const BoolVector& bool_vec) {
    element_set->clear();
    element_set->reserve(bool_vec.count());
    for (size_t i{ 0 }; i < bool_vec.size(); ++i) {
        if (bool_vec[i] == 1) {
            element_set->push_back(i);
        }
    }
}

inline void complement(
        Nfa*               result,
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const ParameterMap&  params = {{ "algorithm", "classical"},
                                       { "minimize",  "false"}}) { *result = complement(aut, alphabet, params);
}

inline void minimize(Nfa* res, const Nfa &aut) { *res = minimize(aut); }

inline void determinize(Nfa* result, const Nfa& aut, std::unordered_map<StateSet, State> *subset_map = nullptr) {
    *result = determinize(aut, subset_map);
}

inline void reduce(Nfa* result, const Nfa &aut, StateRenaming *state_renaming = nullptr,
                   const ParameterMap& params = {{ "algorithm", "simulation"}}) {
    *result = reduce(aut, state_renaming, params);
}

inline void revert(Nfa* result, const Nfa& aut) { *result = revert(aut); }

inline void remove_epsilon(Nfa* result, const Nfa& aut, Symbol epsilon = EPSILON) { *result = remove_epsilon(aut, epsilon); }

/** Loads an automaton from Parsed object */
template <class ParsedObject>
void construct(Nfa* result, const ParsedObject& parsed, Alphabet* alphabet = nullptr,
               NameStateMap* state_map = nullptr) {
    OnTheFlyAlphabet tmp_alphabet{};
    if (!alphabet) { alphabet = &tmp_alphabet; }
    *result = builder::construct(parsed, alphabet, state_map);
}

inline void union_nondet(Nfa *unionAutomaton, const Nfa &lhs, const Nfa &rhs) { *unionAutomaton = union_nondet(lhs, rhs); }

/**
 * @brief Compute intersection of two NFAs.
 *
 * Both automata can contain ε-transitions. The product preserves the ε-transitions, i.e.,
 * for each each product state `(s, t)` with`s -ε-> p`, `(s, t) -ε-> (p, t)` is created, and vice versa.
 *
 * Automata must share alphabets.
 *
 * @param[out] res The resulting intersection NFA.
 * @param[in] lhs Input NFA.
 * @param[in] rhs Input NFA.
 * @param[in] first_epsilon smallest epsilon.
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states (not used internally, allocated only when !=nullptr, expensive).
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
inline void intersection(Nfa* res, const Nfa& lhs, const Nfa& rhs, Symbol first_epsilon = EPSILON,
                  std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr) {
    *res = intersection(lhs, rhs, first_epsilon, prod_map);
}

/**
 * @brief Concatenate two NFAs.
 * @param[out] lhs_result_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_result_state_renaming Map mapping rhs states to result states.
 */
inline void concatenate(Nfa* res, const Nfa& lhs, const Nfa& rhs, bool use_epsilon = false,
                 StateRenaming* lhs_result_state_renaming = nullptr, StateRenaming* rhs_result_state_renaming = nullptr) {
    *res = concatenate(lhs, rhs, use_epsilon, lhs_result_state_renaming, rhs_result_state_renaming);
}

} // namespace mata::nfa::Plumbing.

#endif // MATA_NFA_PLUMBING_HH_
