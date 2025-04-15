/* algorithms.hh -- Wrapping up algorithms for Nfa manipulation which would be otherwise in anonymous namespaces.
 */

#ifndef MATA_NFA_INTERNALS_HH_
#define MATA_NFA_INTERNALS_HH_

#include "nfa.hh"
#include "mata/simlib/util/binary_relation.hh"

/**
 * Concrete NFA implementations of algorithms, such as complement, inclusion, or universality checking.
 *
 * This is a separation of the implementation from the interface defined in mata::nfa.
 * Note, that in mata::nfa interface, there are particular dispatch functions calling
 * these function according to parameters provided by a user.
 * E.g. we can call the following function: `is_universal(aut, alph, {{'algorithm', 'antichains'}})`
 * to check for universality based on antichain-based algorithm.
 */
namespace mata::nfa::algorithms {

/**
 * Brzozowski minimization of automata (revert -> determinize -> revert -> determinize).
 * @param[in] aut Automaton to be minimized.
 * @return Minimized automaton.
 */
Nfa minimize_brzozowski(const Nfa& aut);

/**
 * Hopcroft minimization of automata. Based on the algorithm from the paper:
 *  "Efficient Minimization of DFAs With Partial Transition Functions" by Antti Valmari and Petri Lehtinen.
 *  The algorithm works in O(a*n*log(n)) time and O(m+n+a) space, where: n is the number of states, a is the size
 *  of the alphabet, and m is the number of transitions. [https://dl.acm.org/doi/10.1016/j.ipl.2011.12.004]
 * @param[in] dfa_trimmed Deterministic automaton without useless states. Perform trimming before calling this function.
 * @return Minimized deterministic automaton.
 */
Nfa minimize_hopcroft(const Nfa& dfa_trimmed);

/**
 * Complement implemented by determization, adding sink state and making automaton complete. Then it adds final states
 *  which were non final in the original automaton.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @param[in] minimize_during_determinization Whether the determinized automaton is computed by (brzozowski)
 *  minimization.
 * @return Complemented automaton.
 */
Nfa complement_classical(const Nfa& aut, const mata::utils::OrdVector<Symbol>& symbols);

/**
 * Complement implemented by determization using Brzozowski minimization, adding a sink state and making the automaton
 *  complete. Then it swaps final and non-final states.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @return Complemented automaton.
 */
Nfa complement_brzozowski(const Nfa& aut, const mata::utils::OrdVector<Symbol>& symbols);

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
        const ParameterMap&  params = {{ "relation", "simulation"}, { "direction", "forward"}});

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
Nfa product(const Nfa& lhs, const Nfa& rhs, const std::function<bool(State,State)> && final_condition,
            const Symbol first_epsilon = EPSILON, std::unordered_map<std::pair<State,State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two NFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Epsilon to be used for concatenation (provided @p use_epsilon is true)
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
Nfa concatenate_eps(const Nfa& lhs, const Nfa& rhs, const Symbol& epsilon, bool use_epsilon = false,
                    StateRenaming* lhs_state_renaming = nullptr, StateRenaming* rhs_state_renaming = nullptr);

/**
 * @brief Reduce NFA using (forward) simulation.
 * 
 * @param[in] nfa NFA to reduce
 * @param[out] state_renaming Map mapping original states to the reduced states.
 */
Nfa reduce_simulation(const Nfa& nfa, StateRenaming &state_renaming);

/**
 * @brief Reduce NFA using residual construction.
 *
 * @param[in] nfa NFA to reduce.
 * @param[out] state_renaming Map mapping original states to the reduced states.
 * @param[in] type Type of the residual construction (values: "after", "with").
 * @param[in] direction Direction of the residual construction (values: "forward", "backward").
 */
Nfa reduce_residual(const Nfa& nfa, StateRenaming &state_renaming,
                    const std::string& type, const std::string& direction);

/**
 * @brief Reduce NFA using residual construction.
 *
 * The residual construction of the residual automaton and the removal of the
 *  covering states is done during the last determinization.
 *
 * Similar performance to `reduce_residual_after()`.
 * The output is almost the same except the transitions: transitions may
 *  slightly differ, but the number of states is the same for both algorithm
 *  types.
 */
Nfa reduce_residual_with(const Nfa& nfa);

/**
 * @brief Reduce NFA using residual construction.
 *
 * The residual construction of the residual automaton and the removal of the
 *  covering states is done after the final determinization.
 *
 * Similar performance to `reduce_residual_with()`.
 * The output is almost the same except the transitions: transitions may
 *  slightly differ, but the number of states is the same for both algorithm
 *  types.
 */
Nfa reduce_residual_after(const Nfa& nfa);

} // Namespace mata::nfa::algorithms.

#endif // MATA_NFA_INTERNALS_HH_
