/* algorithms.hh -- Wrapping up algorithms for Cntnfa manipulation which would be otherwise in anonymous namespaces.
 */

#pragma once

#include "mata/cntnfa/cntnfa.hh"
#include "mata/simlib/util/binary_relation.hh"

/**
 * Concrete NFA implementations of algorithms, such as complement, inclusion, or universality checking.
 *
 * This is a separation of the implementation from the interface defined in mata::cntnfa.
 * Note, that in mata::cntnfa interface, there are particular dispatch functions calling
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
namespace mata::cntnfa::algorithms {

/**
 * Brzozowski minimization of automata (revert -> determinize -> revert -> determinize).
 * @param[in] aut Automaton to be minimized.
 * @return Minimized automaton.
 */
Cntnfa minimize_brzozowski(const Cntnfa& aut);

/**
 * Complement implemented by determization, adding sink state and making automaton complete. Then it adds final states
 *  which were non final in the original automaton.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @param[in] minimize_during_determinization Whether the determinized automaton is computed by (brzozowski)
 *  minimization.
 * @return Complemented automaton.
 */
Cntnfa complement_classical(const Cntnfa& aut, const mata::utils::OrdVector<Symbol>& symbols);

/**
 * Complement implemented by determization using Brzozowski minimization, adding a sink state and making the automaton
 *  complete. Then it swaps final and non-final states.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @return Complemented automaton.
 */
Cntnfa complement_brzozowski(const Cntnfa& aut, const mata::utils::OrdVector<Symbol>& symbols);

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
bool is_included_naive(const Cntnfa& smaller, const Cntnfa& bigger, const Alphabet* alphabet = nullptr, Run* cex = nullptr);

/**
 * Inclusion implemented by antichain algorithms.
 * @param[in] smaller Automaton which language should be included in the bigger one
 * @param[in] bigger Automaton which language should include the smaller one
 * @param[in] alphabet Alphabet of both automata (not needed for antichain algorithm)
 * @param[out] cex A potential counterexample word which breaks inclusion
 * @return True if smaller language is included,
 * i.e., if the final intersection of smaller complement of bigger is empty.
 */
bool is_included_antichains(const Cntnfa& smaller, const Cntnfa& bigger, const Alphabet*  alphabet = nullptr, Run* cex = nullptr);

/**
 * Universality check implemented by checking emptiness of complemented automaton
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the complemented automaton has non empty language, i.e., the original one is not universal
 */
bool is_universal_naive(const Cntnfa& aut, const Alphabet& alphabet, Run* cex);

/**
 * Universality checking based on subset construction with antichain.
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the automaton is universal, otherwise false.
 */
bool is_universal_antichains(const Cntnfa& aut, const Alphabet& alphabet, Run* cex);

Simlib::Util::BinaryRelation compute_relation(
        const Cntnfa& aut,
        const ParameterMap&  params = {{ "relation", "simulation"}, { "direction", "forward"}});

/**
 * @brief Compute product of two NFAs.
 */
Cntnfa product_counter_nfas(const Cntnfa& lhs, const Cntnfa& rhs);

/**
 * @brief Compute product of two NFAs, final condition is to be specified, with a possibility of using multiple epsilons.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[in] first_epsilons The smallest epsilon.
 * @param[in] final_condition The predicate that tells whether a pair of states is final (conjunction for intersection).
 * @param[out] prod_map Can be used to get the mapping of the pairs of the original states to product states.
 *   Mostly useless, it is only filled in and returned if !=nullptr, but the algorithm internally uses another data structures,
 *   because this one is too slow.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Cntnfa product(const Cntnfa& lhs, const Cntnfa& rhs, const std::function<bool(State,State)> && final_condition,
            const Symbol first_epsilon = EPSILON, std::unordered_map<std::pair<State,State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two NFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Epsilon to be used co concatenation (provided @p use_epsilon is true)
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
Cntnfa concatenate_eps(const Cntnfa& lhs, const Cntnfa& rhs, const Symbol& epsilon, bool use_epsilon = false,
                    StateRenaming* lhs_state_renaming = nullptr, StateRenaming* rhs_state_renaming = nullptr);

} // namespace mata::cntnfa::algorithms.
