/* nfa-internals.hh -- Wrapping up algorithms for Nfa manipulation which would be otherwise in anonymous namespaces
 *
 * Copyright (c) 2022
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

namespace Mata {

namespace Nfa {

/**
 * The following namespace contains methods which would be otherwise in anonymous namespace
 * to make them accessible to users of library. Typically, that are different algorithms for
 * operations such as complement, inclusion, or universality checking.
 * In Nfa interface, there are dispatch functions calling these function according to parameters
 * provided by a user.
 */
namespace Algorithms {

    /**
     * Complement implemented by determization, adding sink state and making automaton complete. Then it adds
     * final states which were non final in the original automaton.
     * @param aut Automaton to be complemented
     * @param alphabet Alphabet is needed since no symbols needs to be in automaton transitions
     * @param subset_map Maps states to subsets created during complementation.
     * @return Complemented automaton
     */
    Nfa complement_classical(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            std::unordered_map<StateSet, State>* subset_map);

    /**
     * Complement implemented by determization and making final states which were non final in the original automaton.
     * @param aut Automaton to be complemented
     * @param alphabet Alphabet is needed since no symbols needs to be in automaton transitions
     * @param params Determines algorithms properties
     * @param subset_map Maps states to subsets created during complementation.
     * @return result Complemented automaton
     */
    Nfa complement_naive(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            const StringMap&  params,
            std::unordered_map<StateSet, State>* subset_map);

    /**
     * Inclusion implemented by complementation of bigger automaton, intersecting it with smaller and then
     * it checks emptiness of intersection
     * @param smaller Automaton which language should be included in the bigger one
     * @param bigger Automaton which language should include the smaller one
     * @param alphabet Alphabet of the both automaton
     * @param cex A potential counterexample word which breaks inclusion
     * @return True if smaller language is included,
     * i.e., if the final intersection of smaller complement of bigger is empty.
     */
    bool is_included_naive(
            const Nfa&             smaller,
            const Nfa&             bigger,
            const Alphabet* const  alphabet,
            Run*                   cex,
            const StringMap&  /* params*/);

    /**
     * Inclusion implemented by antichain algorithms.
     * @param smaller Automaton which language should be included in the bigger one
     * @param bigger Automaton which language should include the smaller one
     * @param alphabet Alphabet of the both automaton
     * @param cex A potential counterexample word which breaks inclusion
     * @param params The parameters used in algorithm. E.g., type of simulation relation
     * @return True if smaller language is included,
     * i.e., if the final intersection of smaller complement of bigger is empty.
     */
    bool is_included_antichains(
            const Nfa&             smaller,
            const Nfa&             bigger,
            const Alphabet* const  alphabet,
            Run*                   cex,
            const StringMap&      params);

    /**
     * Universality check implemented by checking emptiness of complemented automaton
     * @param aut Automaton which universality is checked
     * @param alphabet Alphabet of the automaton
     * @param cex Counterexample word which eventually breaks the universality
     * @return True if the complemented automaton has non empty language, i.e., the original one is not universal
     */
    bool is_universal_naive(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            Run*               cex,
            const StringMap&  /* params*/);

    /**
     * Universality checking based on subset construction with antichain.
     * @param aut Automaton which universality is checked
     * @param alphabet Alphabet of the automaton
     * @param cex Counterexample word which eventually breaks the universality
     * @param params Parameters of the automaton, i.e., simulation relation to be used for antichains
     * @return True if the automaton is universal, otherwise false.
     */
    bool is_universal_antichains(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            Run*              cex,
            const StringMap&  params);

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
} // Algorithms
} // Nfa
}

#endif // MATA_NFA_INTERNALS_HH
