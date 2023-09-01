/* nfa-plumbings.hh -- Wrapping up different supporting functions.
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

/// Make the transition relation complete.
inline void make_complete(Nfa* aut, const Alphabet& alphabet, State sink_state) { make_complete(*aut, alphabet, sink_state); }

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

inline void uni(Nfa *unionAutomaton, const Nfa &lhs, const Nfa &rhs) { *unionAutomaton = uni(lhs, rhs); }

/**
 * @brief Compute intersection of two NFAs.
 *
 * Supports epsilon symbols when @p preserve_epsilon is set to true.
 * When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can
 *  contain ε-transitions. The product preserves the ε-transitions
 *  of both automata. This means that for each ε-transition of the form `s -ε-> p` and each product state `(s, a)`,
 *  an ε-transition `(s, a) -ε-> (p, a)` is created. Furthermore, for each ε-transition `s -ε-> p` and `a -ε-> b`,
 *  a product state `(s, a) -ε-> (p, b)` is created.
 *
 * Automata must share alphabets.
 */
inline void intersection(Nfa* res, const Nfa& lhs, const Nfa& rhs,
                  bool preserve_epsilon = false,
                  std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr) {
    *res = intersection(lhs, rhs, preserve_epsilon, prod_map);
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
