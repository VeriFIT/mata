/* nfa-internals.hh -- Wrapping up algorithms for Nfa manipulation which would be otherwise in anonymous namespaces.
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

#ifndef MATA_NFA_INTERNALS_HH_
#define MATA_NFA_INTERNALS_HH_

#include <mata/nfa.hh>
#include <mata/simlib/util/binary_relation.hh>

/**
 * Concrete NFA implementations of algorithms, such as complement, inclusion, or universality checking.
 *
 * This is a separation of the implementation from the interface defined in Mata::Nfa.
 * Note, that in Mata::Nfa interface, there are particular dispatch functions calling
 * these function according to parameters provided by a user.
 * E.g. we can call the following function: `is_universal(aut, alph, {{'algorithm', 'antichains'}})`
 * to check for universality based on antichain-based algorithm.
 *
 * In particular, this includes algorithms for:
 *   1. Complementation,
 *   2. Inclusion,
 *   3. Universality checking,
 *   4. Intersection/concatenation with epsilon transitions, or,
 *   5. Computing relation.
 */
namespace Mata::Nfa::Algorithms {

/**
 * Brzozowski minimization of automata (revert -> determinize -> revert -> determinize).
 * @param[in] aut Automaton to be minimized.
 * @return Minimized automaton.
 */
Nfa minimize_brzozowski(const Nfa& aut);

/**
 * Complement implemented by determization, adding sink state and making automaton complete. Then it adds final states
 *  which were non final in the original automaton.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @param[in] minimize_during_determinization Whether the determinized automaton is computed by (brzozowski)
 *  minimization.
 * @return Complemented automaton.
 */
Nfa complement_classical(const Nfa& aut, const Mata::Util::OrdVector<Symbol>& symbols,
                         bool minimize_during_determinization = false);

/**
 * Inclusion implemented by complementation of bigger automaton, intersecting it with smaller and then it checks
 *  emptiness of intersection.
 * @param[in] smaller Automaton which language should be included in the bigger one.
 * @param[in] bigger Automaton which language should include the smaller one.
 * @param[in] alphabet Alphabet of both automata (it is computed automatically, but it is more efficient to set it if
 *  you have it).
 * @param[out] cex A potential counterexample word which breaks inclusion
 * @return True if smaller language is included,
 * i.e., if the final intersection of smaller complement of bigger is empty.
 */
bool is_included_naive(const Nfa& smaller, const Nfa& bigger, const Alphabet* alphabet = nullptr, Run* cex = nullptr);

/**
 * Inclusion implemented by antichain algorithms.
 * @param[in] smaller Automaton which language should be included in the bigger one
 * @param[in] bigger Automaton which language should include the smaller one
 * @param[in] alphabet Alphabet of both automata (not needed for antichain algorithm)
 * @param[out] cex A potential counterexample word which breaks inclusion
 * @return True if smaller language is included,
 * i.e., if the final intersection of smaller complement of bigger is empty.
 */
bool is_included_antichains(const Nfa& smaller, const Nfa& bigger, const Alphabet*  alphabet = nullptr, Run* cex = nullptr);

/**
 * Universality check implemented by checking emptiness of complemented automaton
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the complemented automaton has non empty language, i.e., the original one is not universal
 */
bool is_universal_naive(const Nfa& aut, const Alphabet& alphabet, Run* cex);

/**
 * Universality checking based on subset construction with antichain.
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the automaton is universal, otherwise false.
 */
bool is_universal_antichains(const Nfa& aut, const Alphabet& alphabet, Run* cex);

Simlib::Util::BinaryRelation compute_relation(
        const Nfa& aut,
        const StringMap&  params = {{"relation", "simulation"}, {"direction", "forward"}});

/**
 * @brief Compute intersection of two NFAs with a possibility of using multiple epsilons.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[in] preserve_epsilon Whether to compute intersection preserving epsilon transitions.
 * @param[in] epsilons Set of symbols to be considered as epsilons
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa intersection_eps(const Nfa& lhs, const Nfa& rhs, bool preserve_epsilon, const std::set<Symbol>& epsilons,
    std::unordered_map<std::pair<State,State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two NFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Epsilon to be used co concatenation (provided @p use_epsilon is true)
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_result_states_map Map mapping lhs states to result states.
 * @param[out] rhs_result_states_map Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
Nfa concatenate_eps(const Nfa& lhs, const Nfa& rhs, const Symbol& epsilon, bool use_epsilon = false,
    StateToStateMap* lhs_result_states_map = nullptr, StateToStateMap* rhs_result_states_map = nullptr);

} // Namespace Mata::Nfa::Algorithms.

#endif // MATA_NFA_INTERNALS_HH_
