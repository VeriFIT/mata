/** @file
 * @brief Concrete NFA implementations of algorithms, such as complement, inclusion, or universality checking.
 *
 * This is a separation of the implementation from the interface defined in @c mata::nfa.
 * @note In @c mata::nfa interface, there are particular dispatch functions calling these function according to
 *  parameters provided by a user. E.g., we can call the following function:
 * `is_universal(nfa, alphabet, {{'algorithm', 'antichains'}})` to check for universality based on antichain-based
 * algorithm.
 */

#ifndef MATA_NFA_INTERNALS_HH_
#define MATA_NFA_INTERNALS_HH_

#include <stdexcept>
#include <string>

#include "mata/simlib/util/binary_relation.hh"
#include "nfa.hh"

/**
 * Concrete NFA implementations of algorithms, such as complement, inclusion, or universality checking.
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
 * @brief Minimization that picks the algorithm from the shape of the input.
 *
 * Determinizes @p aut when it is not deterministic already, trims it, and runs Hopcroft minimization on the
 *  result. Brzozowski minimization is never picked: it determinizes twice, so it can blow up exponentially even
 *  on an input that is already a trimmed DFA, where Hopcroft is O(a*n*log(n)).
 * An automaton with an empty language is returned as a single non-final initial state with no transitions.
 * @param[in] aut Automaton to be minimized.
 * @return Minimal deterministic automaton.
 */
Nfa minimize_auto(const Nfa& aut);

/**
 * Complement implemented by determization, adding sink state and making automaton complete. Then it adds final states
 *  which were non-final in the original automaton.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
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
 * @return True if smaller language is included, i.e., if the final intersection of smaller complement of bigger is
 * empty.
 */
bool is_included_naive(const Nfa& smaller, const Nfa& bigger, const Alphabet* alphabet = nullptr, Run* cex = nullptr);

/**
 * @brief Inclusion implemented by antichain algorithms.
 *
 * @param[in] smaller Automaton which language should be included in the bigger one
 * @param[in] bigger Automaton which language should include the smaller one
 * @param[in] alphabet Alphabet of both automata (not needed for antichain algorithm)
 * @param[out] cex A potential counterexample word which breaks inclusion
 * @return True if smaller language is included, i.e., if the final intersection of smaller complement of bigger is
 * empty.
 */
bool is_included_antichains(
	const Nfa& smaller, const Nfa& bigger, const Alphabet* alphabet = nullptr, Run* cex = nullptr
);

/**
 * @brief Check universality by checking the emptiness of a complement of @p aut.
 *
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the complemented automaton has non-empty language, i.e., the original one is not universal
 */
bool is_universal_naive(const Nfa& aut, const Alphabet& alphabet, Run* cex);

/**
 * @brief check universality based on subset construction with antichains.
 *
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the automaton is universal, otherwise false.
 */
bool is_universal_antichains(const Nfa& aut, const Alphabet& alphabet, Run* cex);

Simlib::Util::BinaryRelation compute_relation(
	const Nfa& aut, const ParameterMap& params = {{"relation", "simulation"}, {"direction", "forward"}}
);

/**
 * @brief Compute product of two NFAs, final condition is to be specified, with a possibility of using multiple
 * epsilons.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[in] first_epsilon The smallest epsilon.
 * @param[in] final_condition The predicate that tells whether a pair of states is final (conjunction for intersection).
 * @param[out] product_map Can be used to get the mapping of the pairs of the original states to product states.
 *   Mostly useless, it is only filled in and returned if !=nullptr, but the algorithm internally uses another data
 * structures, because this one is too slow.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa product(
	const Nfa& lhs,
	const Nfa& rhs,
	const std::function<bool(State, State)>&& final_condition,
	Symbol first_epsilon = EPSILON,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* product_map = nullptr
);

/**
 * @brief Concatenate two NFAs.
 *
 * Connects @p lhs and @p rhs by adding ε-transitions from each final state of @p lhs to each initial state of
 * @p rhs. The ε is set to @p epsilon. If @p use_epsilon is false, the ε-transitions are then removed (and the
 * choice of @p epsilon determines which symbol is removed). The operands' own @p epsilon transitions are preserved
 * when @p use_epsilon is false; only transitions with the connecting @p epsilon symbol are removed.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Symbol used for the connecting transitions between @p lhs and @p rhs.
 * @param[in] use_epsilon Whether to keep the connecting epsilon transitions in the result.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
Nfa concatenate_eps(
	const Nfa& lhs,
	const Nfa& rhs,
	const Symbol& epsilon,
	bool use_epsilon = false,
	StateRenaming* lhs_state_renaming = nullptr,
	StateRenaming* rhs_state_renaming = nullptr
);

/**
 * @brief Concatenate two NFAs with epsilon transitions, consuming the left operand.
 *
 * The left operand's state posts are moved into the result. The right operand is copied normally.
 * If @p lhs and @p rhs are aliased, a controlled snapshot of @p rhs is taken and the operation
 * falls back to the lvalue path.
 * @p lhs is left valid but empty after the operation.
 */
Nfa concatenate_eps(
	Nfa&& lhs,
	const Nfa& rhs,
	const Symbol& epsilon,
	bool use_epsilon = false,
	StateRenaming* lhs_state_renaming = nullptr,
	StateRenaming* rhs_state_renaming = nullptr
);


/**
 * @brief Reduce NFA using (forward) simulation.
 *
 * @param[in] nfa NFA to reduce
 * @param[out] state_renaming Map mapping original states to the reduced states.
 */
Nfa reduce_simulation(const Nfa& nfa, StateRenaming& state_renaming);

/**
 * @brief Reduce NFA using residual construction.
 *
 * @param[in] nfa NFA to reduce.
 * @param[out] state_renaming Map mapping original states to the reduced states.
 * @param[in] type Type of the residual construction (values: "after", "with").
 * @param[in] direction Direction of the residual construction (values: "forward", "backward").
 */
Nfa reduce_residual(
	const Nfa& nfa, StateRenaming& state_renaming, const std::string& type, const std::string& direction
);

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

/**
 * @brief Exception thrown by @c reduce_sat() and @c reduce_qbf() when no external solver can be run.
 */
class ExternalSolverNotFound : public std::runtime_error {
  public:
	using std::runtime_error::runtime_error;
};

/**
 * @brief Reduce NFA to an automaton with the minimum number of states using an external SAT solver.
 *
 * Experimental. A counterexample-guided search: for k = 1, 2, ..., a SAT solver looks for an automaton with k states
 *  consistent with a set of sample words, the candidate is checked for equivalence with @p nfa, and a distinguishing
 *  word is added to the samples until the candidate is equivalent or no k-state automaton exists. The search is
 *  exponential in the worst case and is practical only for languages whose minimal automata have about ten states.
 *
 * Epsilon transitions over @c EPSILON are removed first. The result uses the symbols and the alphabet of @p nfa.
 *
 * @param[in] nfa NFA to reduce.
 * @param[in] type Kind of the resulting automaton (values: "nfa", "dfa"):
 *  - "nfa": an NFA with the minimum number of states. It never has more states than @p nfa reduced by simulation.
 *  - "dfa": a deterministic (not necessarily complete) automaton with the minimum number of states.
 * @param[in] solver Command running a SAT solver, as the solver's executable followed by its arguments separated by
 *  spaces. Parts containing spaces can be enclosed in single or double quotes; the command is not interpreted by a
 *  shell otherwise. The solver is given a DIMACS CNF file as its last argument and has to print the result in the
 *  format of the SAT competitions, that is, an "s SATISFIABLE" or "s UNSATISFIABLE" line and the model on "v" lines.
 *  If empty, the value of the environment variable @c MATA_SAT_SOLVER is used and, if that is not set, the first of
 *  @c cadical, @c kissat, @c cryptominisat5 and @c picosat found in @c PATH.
 * @return An automaton equivalent to @p nfa with the minimum number of states of the given @p type.
 * @throws ExternalSolverNotFound If no SAT solver can be found or run.
 * @throws std::runtime_error If the solver fails or its output cannot be understood.
 */
Nfa reduce_sat(const Nfa& nfa, const std::string& type = "nfa", const std::string& solver = "");

/**
 * @brief Reduce NFA to an NFA with the minimum number of states using an external QBF solver.
 *
 * Experimental. The same search as @c reduce_sat() with @c type "nfa", but the sample words that the NFA has to
 *  reject are encoded with universally quantified runs instead of the reachable states of their prefixes.
 *
 * @param[in] nfa NFA to reduce.
 * @param[in] solver Command running a QBF solver, given as for @c reduce_sat(). The solver is given a QDIMACS file as
 *  its last argument and has to print the result and the values of the outermost existential variables in the QDIMACS
 *  output format ("s cnf 1 ..." or "s cnf 0 ..." and "V" lines). If empty, the value of the environment variable
 *  @c MATA_QBF_SOLVER is used and, if that is not set, the first of @c depqbf and @c caqe found in @c PATH (run with
 *  @c --qdo, and DepQBF also with @c --no-dynamic-nenofex if it supports the option).
 * @return An NFA equivalent to @p nfa with the minimum number of states.
 * @throws ExternalSolverNotFound If no QBF solver can be found or run.
 * @throws std::runtime_error If the solver fails or its output cannot be understood.
 */
Nfa reduce_qbf(const Nfa& nfa, const std::string& solver = "");

} // Namespace mata::nfa::algorithms.

#endif // MATA_NFA_INTERNALS_HH_
