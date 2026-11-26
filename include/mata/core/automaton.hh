/** @file
 * @brief The structural base class shared by all automata in Mata.
 *
 * @c mata::AutomatonBase holds the parts of an automaton that carry no language semantics.
 *  Every operation defined here is a pass over @c delta, @c initial and @c final, and is therefore
 *  meaningful for any transition relation satisfying @c mata::DeltaLike.
 */

#ifndef MATA_CORE_AUTOMATON_HH_
#define MATA_CORE_AUTOMATON_HH_

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

namespace mata {

/**
 * @brief A class representing the structural part of an automaton.
 *  Every operation defined here is a pass over @c delta, @c initial and @c final.
 *
 * @tparam D The transition relation. Every other type is read off @p D rather than passed
 *  alongside it, so the two cannot disagree. @see @c mata::DeltaLike for what @p D must provide.
 *
 * @note A data-owning mixin, not a polymorphic base: it owns @c delta, @c initial and @c final, and
 *  has no virtual functions. The members that need their most-derived type take an explicit object
 *  parameter (`deducing this`) instead.
 * @note @p D needs no keys: every operation here follows successors only, so the relation may be a plain graph.
 * @note The largest @c State is reserved as "no state", for @c initial and @c final as much as for @c delta: the
 *  algorithms count states in @c State and use the largest one as a marker.
 */
template <DeltaLike D> class AutomatonBase {
  public:
	using DeltaType = D; ///< The transition relation this automaton is built on.
	using State = typename D::State; ///< What indexes the automaton: @c delta, @c initial, @c final.
	using Target = typename D::Target; ///< The leaf payload visiting the successors yields. @see @c State.
	using StateSet = utils::OrdVector<State>;
	using StateRenaming = std::unordered_map<State, State>;

	D delta; ///< Transition relation of the automaton. delta[q] contains transitions from state q.
	utils::SparseSet<State> initial{}; ///< Set of initial states of the automaton.
	utils::SparseSet<State> final{}; ///< Set of final states of the automaton.

  public:
	/**
	 * @brief Construct a new AutomatonBase with optional @p delta, @p initial_states and @p final_states.
	 *
	 * @param[in] delta Transition relation of the automaton.
	 * @param[in] initial_states Set of initial states of the automaton.
	 * @param[in] final_states Set of final states of the automaton.
	 */
	explicit AutomatonBase(
		D delta = {}, utils::SparseSet<State> initial_states = {}, utils::SparseSet<State> final_states = {}
	)
		: delta(std::move(delta)),
		  initial(std::move(initial_states)),
		  final(std::move(final_states)) {}

	/**
	 * @brief Construct a new AutomatonBase with @p num_of_states states and optionally @p initial_states and @p
	 * final_states.
	 *
	 * @param[in] num_of_states Number of states for which to preallocate Delta.
	 * @param[in] initial_states Set of initial states of the automaton.
	 * @param[in] final_states Set of final states of the automaton.
	 */
	explicit AutomatonBase(
		const size_t num_of_states,
		utils::SparseSet<State> initial_states = {},
		utils::SparseSet<State> final_states = {}
	)
		: delta(num_of_states),
		  initial(std::move(initial_states)),
		  final(std::move(final_states)) {}

	AutomatonBase(const AutomatonBase& other) = default;

	/// Nothrow when the relation's move is: a relation whose move may throw is let throw, not terminate.
	AutomatonBase(AutomatonBase&& other) = default;

	AutomatonBase& operator=(const AutomatonBase& other) = default;
	AutomatonBase& operator=(
		AutomatonBase&& other
	) noexcept(std::is_nothrow_move_assignable_v<D> && std::is_nothrow_move_assignable_v<utils::SparseSet<State>>);

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
	bool is_state(const State& state_to_check) const { return state_to_check < num_of_states(); }

	/**
	 * @brief Get set of reachable states.
	 *
	 * Reachable states are states accessible from any initial state.
	 * @todo With the new get_useful_states, it might be useless now.
	 * @param[in] filter Optional filter function to apply to reachable states.
	 *  If provided, only states for which the filter returns true will be included in the result.
	 * @return Set of reachable states.
	 */
	StateSet get_reachable_states(const std::function<bool(State)>& filter = nullptr) const;

	/**
	 * @brief Get set of terminating states.
	 *
	 * Terminating states are states leading to any final state.
	 * @todo With the new get_useful_states, it might be useless now.
	 * @return Set of terminating states.
	 */
	StateSet get_terminating_states() const; ///< Answered by a backward search over predecessor lists.

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
		std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states = std::nullopt,
		std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states = std::nullopt
	) const;

	/**
	 * @brief Structure for storing callback functions (event handlers) utilizing
	 * Tarjan's SCC discover algorithm.
	 */
	struct TarjanDiscoverCallback {
		// event handler for the first-time state discovery
		std::function<bool(State)> state_discover;
		// event handler for SCC discovery (together with the whole Tarjan stack)
		std::function<bool(const std::vector<State>&, const std::vector<State>&)> scc_discover;
		// event handler for state in SCC discovery
		std::function<void(State)> scc_state_discover;
		// event handler for visiting of the state successors
		std::function<void(State, State)> succ_state_discover;
	};

	/**
	 * @brief Tarjan's SCC discover algorithm.
	 *
	 * Only the states reachable from @p initial_states are visited, so an SCC no initial state reaches is not reported.
	 *
	 * @param callback Callback class to instantiate callbacks for the Tarjan's algorithm.
	 * @param initial_states Optional set of states to start the search from. If @c std::nullopt, uses the automaton's
	 *  initial states.
	 */
	void tarjan_scc_discover(
		const TarjanDiscoverCallback& callback,
		std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states = std::nullopt
	) const;

	/**
	 * @brief Returns vector ret where ret[q] is the length of the shortest path from any initial state to q
	 */
	std::vector<State> distances_from_initial() const;

	/**
	 * @brief Returns vector ret where ret[q] is the length of the shortest path from q to any final state
	 */
	std::vector<State> distances_to_final() const; ///< Answered by a backward BFS over predecessor lists.

	/**
	 * @brief Is the accepted language (or relation) empty?
	 *
	 * Structural, so every automaton has it, also one without runs. @see has_no_accepting_path().
	 * @return true iff no accepting path exists.
	 */
	bool is_lang_empty() const { return has_no_accepting_path(); }

	/**
	 * @brief Is the accepted language (or relation) empty, with a counter-example?
	 *
	 * @note Only for an automaton that has runs: the run type comes from @p Self.
	 * @note Unifying the two searches -- having the Tarjan search emit a witness on demand -- would
	 *  save a traversal on the counter-example path, which @c is_included() and @c is_universal()
	 *  take whenever the caller wants one. It would also stop the witness being *shortest*, and
	 *  callers do depend on which one comes back (see the exact-word check in the
	 *  "mata::nfa::Nfa::get_word()" tests). Worth doing only together with a decision about that.
	 * @param[out] cex Counter-example run, filled when an accepting path exists. With @c nullptr, none is built.
	 * @return true iff no accepting path exists.
	 */
// Sphinx cannot parse an explicit object parameter, so Doxygen sees the call-site form.
#ifdef BUILDING_DOX
	template <AutomatonWithRuns Self> bool is_lang_empty(typename Self::Run* cex) const;
#else
	template <AutomatonWithRuns Self> bool is_lang_empty(this const Self& self, typename Self::Run* cex) {
		if (cex == nullptr) { return self.has_no_accepting_path(); }
		if (!self.find_accepting_path_(cex->path)) { return true; }
		cex->word = self.get_word_for_path(*cex).first.word;
		return false;
	}
#endif

	/**
	 * @brief Is the part of the automaton graph reachable from the initial states acyclic?
	 *
	 * A cycle no initial state reaches does not count.
	 *
	 * @return true iff no cycle is reachable from an initial state.
	 */
	bool is_acyclic() const;

	/**
	 * @brief Is no final state reachable from any initial state?
	 *
	 * A plain breadth-first search over a visited bit vector that stops at the first final state. This is a pure
	 *  graph property; whether it coincides with the emptiness of the accepted language (or relation) is a fact
	 *  about the concrete automaton class.
	 *
	 * @return true <-> no accepting path exists.
	 */
	bool has_no_accepting_path() const;

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
// Sphinx cannot parse an explicit object parameter, so Doxygen sees the call-site form.
#ifdef BUILDING_DOX
	template <typename Self> bool is_identical(const Self& other) const;
#else
	template <typename Self> bool is_identical(this const Self& self, const Self& other);
#endif

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
// Sphinx cannot parse an explicit object parameter, so Doxygen sees the call-site form.
#ifdef BUILDING_DOX
	template <typename Self> Self& trim(StateRenaming* state_renaming = nullptr);
#else
	template <typename Self> Self& trim(this Self& self, StateRenaming* state_renaming = nullptr);
#endif

  private:
	/// Bool array indexed by state: a byte per state, faster to test and set than the bits of @c std::vector<bool>.
	using StateBoolArray = BoolVector;

	/**
	 * @brief Compute reachability of states considering only specified states.
	 *
	 * @note Inlining this function improves performance of related functions by ~3%.
	 *
	 * @param[in] initial_states States to start the traversal from.
	 * @param[in] states_to_consider States to consider as potentially reachable. If @c std::nullopt is used, all
	 *  states are considered as potentially reachable.
	 * @return Bool array for reachable states (from @p initial_states): true for reachable, false for unreachable
	 * states.
	 */
	StateBoolArray reachable_states_(
		const utils::SparseSet<State>& initial_states,
		const std::optional<const StateBoolArray>& states_to_consider = std::nullopt
	) const;

	/**
	 * @brief Find a shortest accepting path by BFS.
	 *
	 * @param[out] path The states of the path, from an initial to a final state, when one exists.
	 * @return true iff an accepting path exists.
	 */
	bool find_accepting_path_(std::vector<State>& path) const;

  protected:
	/**
	 * Add a new (fresh) state to the automaton.
	 *
	 * @note Protected on purpose. Growing the state space is a leaf-class concern
	 *  Each leaf publishes its own @c add_state() that maintains its invariants.
	 * @return The newly created state.
	 */
	State add_state();

	/**
	 * Add state @p state to @c delta if @p state is not in @c delta yet.
	 *
	 * @note Protected on purpose. See @c add_state().
	 * @return The requested @p state.
	 */
	State add_state(State state);

	/**
	 * @brief Clear @c delta, @c initial and @c final.
	 *
	 * @note Protected on purpose. See @c add_state().
	 *  A leaf has to clear its own per-state data as well.
	 */
	void clear();

	/**
	 * @brief Structural part of `trim()` for a precomputed @p useful_states.
	 *
	 * Lets a leaf class compute the useful states once, adjust its own per-state data
	 *  and then hand the same bool vector over, instead of running Tarjan twice.
	 *  Any renaming that happens after the trim follows ascending order of the original state numbers.
	 *
	 * @param useful_states A @c BoolVector indicating which states are useful (true) and which are not (false).
	 * @param state_renaming Optional pointer to a @c StateRenaming map to fill with the renaming of states
	 *  after trimming. If provided, the map will be filled with the mapping from old state numbers to
	 *  new state numbers after trimming.
	 * @return @c self after trimming.
	 */
// Sphinx cannot parse an explicit object parameter, so Doxygen sees the call-site form.
#ifdef BUILDING_DOX
	template <typename Self> Self& trim_impl(const BoolVector& useful_states, StateRenaming* state_renaming);
#else
	template <typename Self>
	Self& trim_impl(this Self& self, const BoolVector& useful_states, StateRenaming* state_renaming);
#endif
}; // class AutomatonBase.
} // namespace mata.

#include "mata/core/automaton.tpp"

#endif // MATA_CORE_AUTOMATON_HH_
