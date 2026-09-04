/** @file
 * @brief The structural base class shared by all automata in Mata.
 *
 * @c mata::Automaton holds the parts of an automaton that carry no language semantics.
 *  Every operation defined here is a walk over @c delta, @c initial and @c final,
 *  and is therefore meaningful for any specialization of @c Automaton.
 */

#ifndef MATA_AUTOMATON_HH_
#define MATA_AUTOMATON_HH_

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "mata/nfa/delta.hh"
#include "mata/nfa/types.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

namespace mata {

/**
 * @brief A class representing the structural part of an automaton.
 *  Every operation defined here is a walk over @c delta, @c initial and @c final.
 */
class Automaton {
  public:
	nfa::Delta delta; ///< Transition relation of the automaton. delta[q] contains transitions from state q.
	utils::SparseSet<nfa::State> initial{}; ///< Set of initial states of the automaton.
	utils::SparseSet<nfa::State> final{}; ///< Set of final states of the automaton.

  public:
	/**
	 * @brief Construct a new Automaton with optional @p delta, @p initial_states and @p final_states.
	 *
	 * @param[in] delta Transition relation of the automaton.
	 * @param[in] initial_states Set of initial states of the automaton.
	 * @param[in] final_states Set of final states of the automaton.
	 */
	explicit Automaton(
		nfa::Delta delta = {},
		utils::SparseSet<nfa::State> initial_states = {},
		utils::SparseSet<nfa::State> final_states = {}
	)
		: delta(std::move(delta)),
		  initial(std::move(initial_states)),
		  final(std::move(final_states)) {}

	/**
	 * @brief Construct a new Automaton with @p num_of_states states and optionally @p initial_states and @p
	 * final_states.
	 *
	 * @param[in] num_of_states Number of states for which to preallocate Delta.
	 * @param[in] initial_states Set of initial states of the automaton.
	 * @param[in] final_states Set of final states of the automaton.
	 */
	explicit Automaton(
		const size_t num_of_states,
		utils::SparseSet<nfa::State> initial_states = {},
		utils::SparseSet<nfa::State> final_states = {}
	)
		: delta(num_of_states),
		  initial(std::move(initial_states)),
		  final(std::move(final_states)) {}

	Automaton(const Automaton& other) = default;

	Automaton(Automaton&& other) noexcept
		: delta{std::move(other.delta)},
		  initial{std::move(other.initial)},
		  final{std::move(other.final)} {}

	Automaton& operator=(const Automaton& other) = default;
	Automaton& operator=(Automaton&& other) noexcept;

	/**
	 * @brief Get the current number of states in the whole automaton.
	 *
	 * This includes the initial and final states as well as states in the transition relation.
	 * @return The number of states.
	 */
	size_t num_of_states() const;

	/**
	 * @brief Check if a given state is a valid state in the automaton.
	 *
	 * @param[in] state_to_check The state to check.
	 * @return true if the state is valid, false otherwise.
	 */
	bool is_state(const nfa::State& state_to_check) const { return state_to_check < num_of_states(); }

	/**
	 * @brief Get set of reachable states.
	 *
	 * Reachable states are states accessible from any initial state.
	 * @todo With the new get_useful_states, it might be useless now.
	 * @param[in] filter Optional filter function to apply to reachable states.
	 *  If provided, only states for which the filter returns true will be included in the result.
	 * @return Set of reachable states.
	 */
	nfa::StateSet get_reachable_states(const std::function<bool(nfa::State)>& filter = nullptr) const;

	/**
	 * @brief Get set of terminating states.
	 *
	 * Terminating states are states leading to any final state.
	 * @todo With the new get_useful_states, it might be useless now.
	 * @return Set of terminating states.
	 */
	nfa::StateSet get_terminating_states() const;

	/**
	 * @brief Get the useful states using a modified Tarjan's algorithm.
	 *
	 * A state is useful if it is reachable from an initial state and can reach a final state.
	 *
	 * @param initial_states Optional set of initial states to consider when computing usefulness. If @c std::nullopt,
	 *  uses the automaton's initial states.
	 * @param final_states Optional set of final states to consider when computing usefulness. If @c std::nullopt, uses
	 *  the automaton's final states.
	 * @return BoolVector Bool vector whose `i`-th value is true iff the state `i` is useful.
	 */
	BoolVector get_useful_states(
		std::optional<std::reference_wrapper<const utils::SparseSet<nfa::State>>> initial_states = std::nullopt,
		std::optional<std::reference_wrapper<const utils::SparseSet<nfa::State>>> final_states = std::nullopt
	) const;

	/**
	 * @brief Structure for storing callback functions (event handlers) utilizing
	 * Tarjan's SCC discover algorithm.
	 */
	struct TarjanDiscoverCallback {
		// event handler for the first-time state discovery
		std::function<bool(nfa::State)> state_discover;
		// event handler for SCC discovery (together with the whole Tarjan stack)
		std::function<bool(const std::vector<nfa::State>&, const std::vector<nfa::State>&)> scc_discover;
		// event handler for state in SCC discovery
		std::function<void(nfa::State)> scc_state_discover;
		// event handler for visiting of the state successors
		std::function<void(nfa::State, nfa::State)> succ_state_discover;
	};

	/**
	 * @brief Tarjan's SCC discover algorithm.
	 *
	 * @param callback Callback class to instantiate callbacks for the Tarjan's algorithm.
	 * @param initial_states Optional set of initial states to consider when computing SCCs. If @c std::nullopt, uses
	 * all states in the automaton.
	 */
	void tarjan_scc_discover(
		const TarjanDiscoverCallback& callback,
		std::optional<std::reference_wrapper<const utils::SparseSet<nfa::State>>> initial_states = std::nullopt
	) const;

	/**
	 * @brief Returns vector ret where ret[q] is the length of the shortest path from any initial state to q
	 */
	std::vector<nfa::State> distances_from_initial() const;

	/**
	 * @brief Returns vector ret where ret[q] is the length of the shortest path from q to any final state
	 */
	std::vector<nfa::State> distances_to_final() const;

	/**
	 * @brief Is no final state reachable from any initial state?
	 *
	 * Uses Tarjan's SCC discover algorithm. This is a pure graph property; whether it coincides with the emptiness
	 *  of the accepted language (or relation) is a fact about the concrete automaton class, so the leaves expose it
	 *  under their own names (e.g. @c mata::nfa::Nfa::is_lang_empty_scc()).
	 *
	 * @return true <-> no accepting path exists.
	 */
	bool has_no_accepting_path() const;

	/**
	 * @brief Is the automaton graph acyclic?
	 *
	 * @return true <-> Automaton graph is acyclic.
	 */
	bool is_acyclic() const;

	/**
	 * @brief Check if @c this is structuralry identical to @p other.
	 *
	 * Compares only @c delta, @c initial and @c final. This is exact structural
	 *  equality, including state numbering (so even stronger than isomorphism),
	 *  essentially only useful for testing purposes.
	 *
	 * @note Templated on @p Self (`deducing this`) so that only same-type comparisons compile.
	 * @param[in] other The other automaton to compare with.
	 * @return true iff the structural parts of @c this and @p other are identical.
	 */
	template <typename Self> bool is_identical(this const Self& self, const Self& other);

	/**
	 * @brief Remove unreachable and non-terminating states in-place.
	 *  Remaining states are renumbered densely in ascending order.
	 *
	 * @note A state is reachable when the state is the endpoint of a path starting from an initial state.
	 *  A state is terminating when the state is the starting point of a path ending in a final state.
	 * @note Reached only through a leaf's own one-line @c trim(), on @c *this, so an lvalue @p Self suffices here.
	 * @param[out] state_renaming Mapping of trimmed states to new states.
	 * @return @c this after trimming.
	 */
	template <typename Self> Self& trim(this Self& self, nfa::StateRenaming* state_renaming = nullptr);

	/**
	 * Check whether no accepting path exists, recording a witness in @p cex when one does.
	 *
	 * @param[out] cex Counter-example path (and, via the leaf's @c get_word_for_path(), word) for a case an
	 *  accepting path exists.
	 * @return true if no accepting path exists, false otherwise.
	 */
	template <typename Self> bool is_lang_empty(this const Self& self, nfa::Run* cex = nullptr);

  protected:
	/**
	 * Add a new (fresh) state to the automaton.
	 *
	 * @note Protected on purpose. Growing the state space is a leaf-class concern
	 *  Each leaf publishes its own @c add_state() that maintains its invariants.
	 * @return The newly created state.
	 */
	nfa::State add_state();

	/**
	 * Add state @p state to @c delta if @p state is not in @c delta yet.
	 *
	 * @note Protected on purpose. See @c add_state().
	 * @return The requested @p state.
	 */
	nfa::State add_state(nfa::State state);

	/**
	 * @brief Clear @c delta, @c initial and @c final.
	 *
	 * @note Protected on purpose. See @c add_state().
	 *  A leaf has to clear its own per-state data as well.
	 */
	void clear();

	/**
	 * @brief Reverse the automaton structurally: reverse every transition and swap initial and final states.
	 *
	 * @note Kept as a protected helper so that @c get_terminating_states() and @c distances_to_final() do not
	 *  have to go through a leaf-specific `revert()` free function.
	 * @return A new automaton with reversed transitions and swapped initial/final states.
	 */
	Automaton reverted() const;

	/**
	 * @brief Structural part of `trim()` for a precomputed @p useful_states.
	 *
	 * Lets a leaf class compute the useful states once, adjust its own per-state data (such as
	 *  @c mata::nft::Nft::levels) and then hand the same bool vector over, instead of running Tarjan twice.
	 *  Any renaming that happens after the trim follows ascending order of the original state numbers.
	 *
	 * @note Kept protected: it assumes @p useful_states was computed for @c self and gives no guarantee about a
	 *  mismatched one. Templated on @p Self (`deducing this`) purely so it can return @p Self& for @c trim() to
	 *  return directly; a leaf that redeclares its own @c trim() (hiding this by name, since both share the name
	 *  `trim`) still reaches it unqualified as `trim_impl(...)`, since only `trim` is redeclared, not `trim_impl`.
	 * @param useful_states A @c BoolVector indicating which states are useful (true) and which are not (false).
	 * @param state_renaming Optional pointer to a @c StateRenaming map to fill with the renaming of states
	 *  after trimming. If provided, the map will be filled with the mapping from old state numbers to
	 *  new state numbers after trimming.
	 * @return @c self after trimming.
	 */
	template <typename Self>
	Self& trim_impl(this Self& self, const BoolVector& useful_states, nfa::StateRenaming* state_renaming);
}; // class Automaton.

} // namespace mata.

#include "mata/automaton.tpp"

#endif // MATA_AUTOMATON_HH_
