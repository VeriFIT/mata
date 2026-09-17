/** @file
 * @brief The structural base class shared by all automata in Mata.
 *
 * @c mata::AutomatonBase holds the parts of an automaton that carry no language semantics.
 *  Every operation defined here is a walk over @c delta, @c initial and @c final, and is therefore
 *  meaningful for any transition relation satisfying @c mata::DeltaLike.
 *
 * @c mata::Automaton is the one specialization the in-tree automata are built on. It is a plain
 *  alias, so @c mata::nfa::Nfa and @c mata::nft::Nft keep deriving from a name, not from a
 *  template, and stay non-template classes themselves.
 */

#ifndef MATA_CORE_AUTOMATON_HH_
#define MATA_CORE_AUTOMATON_HH_

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
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
 *  Every operation defined here is a walk over @c delta, @c initial and @c final.
 *
 * @tparam D The transition relation. Every other type is read off @p D rather than passed
 *  alongside it, so the two cannot disagree; see @c mata::DeltaLike for what @p D must provide.
 *
 * @note A data-owning mixin, not a polymorphic base: it owns @c delta, @c initial and @c final, and
 *  has no virtual functions. The members that need their most-derived type take an explicit object
 *  parameter (`deducing this`) instead.
 * @note @p D is not constrained to any particular @c key_arity. Only the generic walks care how
 *  deep the relation nests, and they are constrained separately.
 */
template <DeltaLike D> class AutomatonBase {
  public:
	using DeltaType = D; ///< The transition relation this automaton is built on.
	using State = typename D::State; ///< What indexes the automaton: @c delta, @c initial, @c final.
	using Target = typename D::Target; ///< The leaf payload a successor walk yields. @see @c State.
	/// Number of keys between a source state and a target. @see @ref arity.
	static constexpr size_t key_arity{D::key_arity};

	/// Spelled in terms of @c State rather than taken from @c mata::, so that a relation with a
	///  divergent state type cannot leave these signatures behind. Both are the same type as
	///  @c mata::StateSet and @c mata::StateRenaming for @c mata::Automaton.
	///
	/// @note A plain set of states, deliberately not the relation's innermost post type: the
	///  structural operations return *states*, and a relation's targets may carry a payload. The
	///  member alias hides the namespace-scope @c mata::nfa::StateSet inside @c mata::nfa::Nfa, so it
	///  must be the same template that one names, or an @c Nfa member declared with one and defined
	///  with the other stops matching.
	using StateSet = utils::OrdVector<State>;
	using StateRenaming = std::unordered_map<State, State>;

	/// Deliberately no key type at all, not even an alias. This class transports keys (in
	///  @c reverted(), as `const auto&`) but never inspects, compares or stores one, and at a
	///  @c key_arity above 1 there is no single "the key" to name anyway. Kept spelled this way so
	///  that grepping this file for a key type stays a meaningful check.

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
		D delta = {},
		utils::SparseSet<State> initial_states = {},
		utils::SparseSet<State> final_states = {}
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

	AutomatonBase(AutomatonBase&& other) noexcept
		: delta{std::move(other.delta)},
		  initial{std::move(other.initial)},
		  final{std::move(other.final)} {}

	AutomatonBase& operator=(const AutomatonBase& other) = default;
	AutomatonBase& operator=(AutomatonBase&& other) noexcept;

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
	StateSet get_terminating_states() const; ///< Answered by reverting. @see @c reverted().

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
	 * @param callback Callback class to instantiate callbacks for the Tarjan's algorithm.
	 * @param initial_states Optional set of initial states to consider when computing SCCs. If @c std::nullopt, uses
	 * all states in the automaton.
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
	std::vector<State> distances_to_final() const; ///< Answered by reverting. @see @c reverted().


	/**
	 * @brief Is the accepted language (or relation) empty, optionally with a counter-example?
	 *
	 * @note The run type comes from @p Self.
	 * @note Unifying the two searches -- having the Tarjan walk emit a witness on demand -- would
	 *  save a traversal on the counter-example path, which @c is_included() and @c is_universal()
	 *  take whenever the caller wants one. It would also stop the witness being *shortest*, and
	 *  callers do depend on which one comes back (see the exact-word check in the
	 *  "mata::nfa::Nfa::get_word()" tests). Worth doing only together with a decision about that.
	 * @param[out] cex Counter-example run, filled when an accepting path exists.
	 * @return true iff no accepting path exists.
	 */
	template <AutomatonWithRuns Self> bool is_lang_empty(this const Self& self, typename Self::Run* cex = nullptr) {
		if (cex == nullptr) { return self.has_no_accepting_path_scc_(); }
		if (!self.find_accepting_path_(cex->path)) { return true; }
		cex->word = self.get_word_for_path(*cex).first.word;
		return false;
	}

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
	template <typename Self> Self& trim(this Self& self, StateRenaming* state_renaming = nullptr);


  private:
	/// Bool array indexed by state. Distinct from @c BoolVector, which is a @c std::vector<uint8_t>.
	using StateBoolArray = std::vector<bool>;

	/// The cursor @p D hands out over the successors of one state, and its iterator. Derived rather
	///  than named, so that @c mata::DeltaLike does not have to require a particular spelling.
	using SuccessorCursorType = decltype(std::declval<const D&>().successor_cursor(std::declval<State>()));
	using SuccessorIterator = decltype(std::declval<const SuccessorCursorType&>().begin());

	/**
	 * @brief Metadata for one state during the computation of useful states.
	 *
	 * Tarjan's own per-node data, plus the position of the iteration through the state's successors,
	 *  so that a simulated recursive call can be resumed where it left off.
	 */
	struct TarjanNodeData {
		SuccessorIterator current_successor_it{};
		// index of a node (corresponds to the time of discovery)
		unsigned long index{0};
		// index of a lower node in the same SCC
		unsigned long lowlink{0};
		// was the node already initialized (=the initial phase of the Tarjan's recursive call was executed)
		bool initilized{false};
		// is node on Tarjan's stack?
		bool on_stack{false};

		TarjanNodeData() = default;

		TarjanNodeData(const State q, const D& delta, const unsigned long index)
			: current_successor_it(delta.successor_cursor(q).begin()),
			  index(index),
			  lowlink(index),
			  initilized(true),
			  on_stack(true) {}
	};

	/**
	 * @brief Compute reachability of states considering only specified states.
	 *
	 * @param[in] states_to_consider States to consider as potentially reachable. If @c std::nullopt is used, all
	 *  states are considered as potentially reachable.
	 * @return Bool array for reachable states (from initial states): true for reachable, false for unreachable states.
	 */
	StateBoolArray reachable_states_(const std::optional<const StateBoolArray>& states_to_consider = std::nullopt
	) const;

	/**
	 * @brief Check if @c this has no accepting path using Tarjan's SCC discover algorithm.
	 *
	 * @return true iff no accepting path exists.
	 */
	bool has_no_accepting_path_scc_() const;

	/**
	 * @brief Check if @c this has no accepting path using BFS.
	 *
	 * @return true iff no accepting path exists.
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
	 * @brief Reverse the automaton structurally: reverse every transition and swap initial and final states.
	 *
	 * @note Kept as a protected helper so that @c get_terminating_states() and @c distances_to_final() do not
	 *  have to go through a leaf-specific `revert()` free function.
	 * @note One of the two structural operations here that *write* to a relation (the other is
	 *  @c trim_impl()), and the only one that has to take a transition apart and put it back.
	 * @note **Generic in the key arity.** It writes through @c mata::posts::insert_target rather than
	 *  through @c Delta::add, which is what makes that possible: a pack has to come last to deduce,
	 *  so `add(source, keys..., target)` is not declarable at all, and putting the target first
	 *  instead — `insert_target(post, target, keys...)` — is. The keys are transported as a pack and
	 *  never inspected, which is why @c mata::DeltaLike asks for no key type. Reverting swaps
	 *  source and target and leaves the key order alone, so a key path means the same thing in both
	 *  directions at any arity.
	 * @return A new automaton with reversed transitions and swapped initial/final states.
	 */
	AutomatonBase reverted() const;

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
	Self& trim_impl(this Self& self, const BoolVector& useful_states, StateRenaming* state_renaming);
}; // class AutomatonBase.
} // namespace mata.

#include "mata/core/automaton.tpp"

#endif // MATA_CORE_AUTOMATON_HH_
