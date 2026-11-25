#ifndef MATA_CORE_DELTA_HH
#define MATA_CORE_DELTA_HH

/** @file
 * @brief The transition relation: a mapping from states and keys to targets.
 */

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/core/concepts.hh"
#include "mata/core/cursor.hh"
#include "mata/core/moves.hh"
#include "mata/core/post.hh"
#include "mata/core/traits.hh"
#include "mata/core/transition.hh"
#include "mata/core/visits.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

namespace mata {
namespace posts {
/**
 * @brief The keyed writes and lookup of a relation: one member each, at every arity.
 *
 * The keys are a pack of this class, not of the member, so every parameter has a declared type and
 *  `add(0, 1, 2, 3)` converts its `int`s correctly at the call. Arity 1 is specialised below.
 *
 * @tparam D The relation deriving from this. A friend of it, for its posts.
 * @tparam P The relation's post chain.
 */
template <typename D, typename P, typename = std::make_index_sequence<arity_of<P>>> class KeyedAccess;

template <typename D, typename P, size_t... KeyIdx> class KeyedAccess<D, P, std::index_sequence<KeyIdx...>> {
	using Target = target_of<P>;
	using State = typename TargetTraits<Target>::State;
	using TargetArg = utils::ArgOf<Target>;
	using TargetSet = typename PostAt<P, sizeof...(KeyIdx)>::type;

	D& self() { return static_cast<D&>(*this); }
	const D& self() const { return static_cast<const D&>(*this); }

  public:
	/**
	 * @brief Add the transition from @p source under @p keys to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param keys Keys of the transition.
	 * @param target Target of the transition.
	 */
	void add(const State source, const KeyOf<P, KeyIdx>... keys, const TargetArg target) {
		self().resize_for_states(source, D::state_of(target));
		insert_target(self().mutable_state_post(source), target, keys...);
	}

	/**
	 * @brief Add the transition from @p source under @p keys to every one of @p targets.
	 *
	 * @param source Source state of the transition.
	 * @param keys Keys of the transition.
	 * @param targets Set of targets of the transition.
	 */
	void add(const State source, const KeyOf<P, KeyIdx>... keys, const TargetSet& targets) {
		const TargetSet snapshot{targets};
		for (const Target& target : snapshot) {
			self().resize_for_states(source, D::state_of(target));
			insert_target(self().mutable_state_post(source), target, keys...);
		}
	}

	/**
	 * @brief Remove the transition from @p source under @p keys to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param keys Keys of the transition.
	 * @param target Target of the transition.
	 * @throws Throws std::invalid_argument if @p source has no transition under @p keys. A @p source beyond the
	 *  relation, or a @p target missing under existing @p keys, is ignored.
	 */
	void remove(const State source, const KeyOf<P, KeyIdx>... keys, const TargetArg target) {
		if (source >= self().state_posts_.size()) { return; }
		erase_target(self().state_posts_[source], target, keys...);
	}

	/**
	 * @brief Check whether the relation contains the transition from @p source under @p keys to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param keys Keys of the transition.
	 * @param target Target of the transition.
	 * @return True if the relation contains the transition, false otherwise.
	 */
	bool contains(const State source, const KeyOf<P, KeyIdx>... keys, const TargetArg target) const {
		return source < self().state_posts_.size() && has_target_at(self().state_posts_[source], target, keys...);
	}
};

/**
 * @brief Specialisation of @c KeyedAccess for arity 1.
 *
 * Hand-written rather than the generic template: `add` has an append fast path
 *  (`back().key() < symbol`) that skips the search when a relation is built in sorted order, where
 *  @c insert_target searches first, and twice on a miss. Bodies in @c mata/core/delta.tpp.
 */
template <typename D, typename P> class KeyedAccess<D, P, std::index_sequence<0>> {
	using Target = target_of<P>;
	using State = typename TargetTraits<Target>::State;
	using TargetArg = utils::ArgOf<Target>;
	using Key = KeyOf<P, 0>;
	using Nested = typename P::Nested;

	D& self() { return static_cast<D&>(*this); }
	const D& self() const { return static_cast<const D&>(*this); }

  public:
	/**
	 * @brief Add a transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 */
	void add(State source, Key symbol, TargetArg target);

	/**
	 * @brief Add a transition from @p source over @p symbol to every one of @p targets.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param targets Set of targets of the transition.
	 */
	void add(State source, Key symbol, const Nested& targets);

	/**
	 * @brief Remove the transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 * @throws Throws std::invalid_argument if @p source has no transition over @p symbol. A @p source beyond the
	 *  relation, or a @p target missing under an existing @p symbol, is ignored.
	 */
	void remove(State source, Key symbol, TargetArg target);

	/**
	 * @brief Check whether the relation contains the transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 * @return True if the relation contains the transition, false otherwise.
	 */
	bool contains(State source, Key symbol, TargetArg target) const;
};

/**
 * @brief The generic transition relation: a vector of posts indexed by source state.
 *
 * A vector of posts indexed by source state, over a chain of posts ending in a set of targets.
 *  The depth is deduced from @p P as @c key_arity. Posts are named by key index, not by container count:
 *  @c Key<0> is the topmost symbol, and @c PostAt<key_arity> is the innermost post.
 *
 * @tparam P The post reached from one source state, itself parameterised down to the targets.
 * @tparam TT What one transition of this relation looks like and how to build one: a traits with a
 *  @c Type and a @c make(). Defaults to the symbol-named triple at arity 1 and to the tuple-keyed
 *  transition above it. @see mata::posts::DefaultTransitionTraits.
 * @see mata::DeltaLike.
 */
template <typename P, typename TT = DefaultTransitionTraits<P>>
class DeltaBase : public KeyedAccess<DeltaBase<P, TT>, P> {
  public:
	/// @name Post protocol
	///@{
	using PostType = P; ///< The post reached from one source state.
	using Target = typename PostType::Target; ///< What visiting the successors yields.
	using TargetArg = utils::ArgOf<Target>; ///< How the keyed members take a target.
	using State = typename TargetTraits<Target>::State; ///< What indexes this relation.
	static constexpr size_t key_arity{PostType::key_arity};

	/// The post at index @p I, counted from the outermost one.
	template <size_t I> using PostAt = typename posts::PostAt<PostType, I>::type;
	/// The key at index @p I, counted from the outermost one.
	template <size_t I> using Key = typename PostAt<I>::Key;
	/// The reserved keys at index @p I, counted from the outermost one.
	template <size_t I> using Reserved = typename PostAt<I>::Reserved;
	using TargetSet = PostAt<key_arity>;
	using KeyedSuccessors = typename PostType::KeyedSuccessors; ///< The result type of the keyed @c get_successors.
	/// The result type of the unkeyed @c get_successors: whatever the post returns, e.g. a @c StateSet for
	///  @c mata::StatePost.
	using Successors = decltype(std::declval<const PostType&>().get_successors());

	/**
	 * @brief Get the state a target denotes, whatever it carries.
	 *
	 * @param target Target to get the state of.
	 * @return State denoted by @p target.
	 */
	static State state_of(const Target& target) { return TargetTraits<Target>::state_of(target); }

	using Entry = typename PostType::Entry;
	using Nested = typename PostType::Nested;
	using TransitionTraits = TT;
	using TransitionType = typename TT::Type;
	using CursorType = posts::SuccessorCursor<PostType>;
	///@}

	/// A constant returned when a state post is requested for a source that has no allocated post.
	inline static const PostType empty_state_post;

	DeltaBase() : state_posts_{} {}
	DeltaBase(const DeltaBase& other) = default;
	DeltaBase(DeltaBase&& other) = default;
	explicit DeltaBase(const size_t n) : state_posts_{} { allocate(n); }

	DeltaBase& operator=(const DeltaBase& other) = default;
	DeltaBase& operator=(DeltaBase&& other) = default;

	bool operator==(const DeltaBase& other) const;

	/**
	 * @brief Reserve space for @p n states in the relation.
	 *
	 * @param n Number of states to reserve space for.
	 */
	void reserve(const size_t n) { state_posts_.reserve(n); };

	/**
	 * @brief Get constant reference to the state post of @p source.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, an @c empty_post is returned. Hence, the function has no side
	 *  effects (no allocation is performed and iterators remain valid).
	 *
	 * @param source Source state of a state post to access.
	 * @return State post of @p source.
	 */
	const PostType& state_post(const State source) const {
		if (source >= num_of_states()) { return empty_state_post; }
		return state_posts_[source];
	}
	const PostType& operator[](const State source) const { return state_post(source); }

	/**
	 * @brief Get mutable (non-constant) reference to the state post of @p source.
	 *
	 * The function allows modifying the state post. BEWARE, IT HAS A SIDE EFFECT.
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, a new state post for @p source will be allocated along with
	 *  all state posts for all previous states. This in turn may cause that the entire post data structure is
	 *  re-allocated. Iterators into the relation will get invalidated.
	 * @param source Source state of a state post to access.
	 * @return State post of @p source.
	 */
	PostType& mutable_state_post(State source);

	/**
	 * @brief Defragment the relation.
	 *
	 * This function removes all state posts which are not in @p is_staying and
	 *  renames the remaining state posts according to @p renaming.
	 *
	 * @param[in] is_staying Boolean vector indicating which states are staying in the relation.
	 * @param[in] renaming Vector of states to rename the remaining state posts to.
	 * @return Self with defragmented delta.
	 */
	DeltaBase& defragment(const BoolVector& is_staying, const std::vector<State>& renaming);

	template <typename... Args> PostType& emplace_back(Args&&... args) {
		// Forwarding the variadic template pack of arguments to the emplace_back() of the underlying container.
		return state_posts_.emplace_back(std::forward<Args>(args)...);
	}

	void clear() { state_posts_.clear(); }

	/**
	 * @brief Allocate state posts up to @p num_of_states states,
	 *  creating empty @c PostType for yet unallocated state posts.
	 *
	 * @param[in] num_of_states Number of states in the relation to allocate state posts for.
	 *  Have to be at least num_of_states() + 1.
	 */
	void allocate(const size_t num_of_states) {
		MATA_ASSERT(num_of_states >= this->num_of_states());
		check_num_of_states_(num_of_states);
		state_posts_.resize(num_of_states);
	}

	/**
	 * @return Number of states in the whole relation, including both source and target states.
	 */
	size_t num_of_states() const { return state_posts_.size(); }

	/**
	 * Check whether the @p state is used in the relation.
	 */
	bool uses_state(const State state) const { return state < num_of_states(); }

	/**
	 * @return Number of transitions in the relation.
	 */
	size_t num_of_transitions() const;

	/// @name Keyed writes and lookup
	/// One member each, at every arity. @see mata::posts::KeyedAccess.
	///@{
	friend class KeyedAccess<DeltaBase, P>;
	using KeyedAccess<DeltaBase, P>::add;
	using KeyedAccess<DeltaBase, P>::remove;
	using KeyedAccess<DeltaBase, P>::contains;
	///@}

	/// @name Transition-shaped members
	/// Generic in the arity: a transition is taken apart with its type's @c parts() and the pieces
	///  are forwarded to the keyed overload of the matching arity. What a transition *is* comes from
	///  the traits. Only @c Transitions, which has to carry the keys, is still arity 1.
	///@{
	/**
	 * @brief Add a transition to the relation.
	 *
	 * @param transition Transition to add.
	 */
	void add(const TransitionType& transition) {
		std::apply([this](const auto&... p) { this->add(p...); }, TransitionType::parts(transition));
	}

	/**
	 * @brief Remove a transition from the relation.
	 *
	 * @param transition Transition to remove.
	 */
	void remove(const TransitionType& transition) {
		std::apply([this](const auto&... p) { this->remove(p...); }, TransitionType::parts(transition));
	}

	/**
	 * @brief Check whether the relation contains a transition.
	 *
	 * @param transition Transition to check.
	 * @return True if the relation contains the transition, false otherwise.
	 */
	bool contains(const TransitionType& transition) const {
		return std::apply([this](const auto&... p) { return this->contains(p...); }, TransitionType::parts(transition));
	}
	///@}

	/**
	 * @brief Check whether automaton contains no transitions.
	 *
	 * @return True if there are no transitions in the automaton, false otherwise.
	 */
	bool empty() const;

	/**
	 * @brief Append post vector to the delta.
	 *
	 * @param post_vector Vector of posts to be appended.
	 */
	void append(const std::vector<PostType>& post_vector) {
		for (const PostType& pst : post_vector) { this->state_posts_.push_back(pst); }
	}

	/**
	 * @brief Copy posts of delta and apply a lambda update function on each state from targets.
	 *
	 * The copied targets stay sorted and duplicate-free, also when @p target_renumberer merges states.
	 *  A strictly increasing renumberer copies in linear time.
	 *
	 * @param target_renumberer Lambda function mapping states to different states.
	 * @return std::vector<Post> Copied posts.
	 */
	std::vector<PostType> renumber_targets(const std::function<State(State)>& target_renumberer) const;

	/**
	 * @brief Apply @p fn to every target reachable from @p source, under any key.
	 */
	template <typename Fn> void for_each_successor(const State source, Fn&& fn) const {
		state_post(source).for_each_target(fn);
	}

	/**
	 * @brief Apply @p fn to every @c Move leaving @p source, as a (key, target) pair.
	 */
	template <typename Fn> void for_each_move(const State source, Fn&& fn) const {
		state_post(source).for_each_move(fn);
	}

	/**
	 * @brief Checks if @p source have @p target among its successors, under any key.
	 *
	 * @param[in] source Source state to look from.
	 * @param[in] target Target state to look for.
	 * @return True if @p target is among the successors of @p source, false otherwise.
	 */
	bool is_successor(const State source, const State target) const {
		if constexpr (std::same_as<Target, State>) {
			// A bare state is the target itself, so the post can binary-search for it.
			return state_post(source).has_target(target);
		} else {
			// A target with a payload is found by its state, which its order need not lead with.
			return any_target(state_post(source), [target](const Target& t) { return state_of(t) == target; });
		}
	}

	/**
	 * @brief Checks if @p state have a transition back to itself under any key.
	 *
	 * @param[in] state State to check for self-loop.
	 * @return True if @p state has a transition back to itself under any key, false otherwise.
	 */
	bool has_self_loop(const State state) const { return is_successor(state, state); }

	/**
	 * @brief A resumable cursor over the successors of @p source. See @c SuccessorCursor.
	 *
	 * @param source Source state to get the successors of.
	 * @return A resumable cursor over the successors of @p source.
	 */
	CursorType successor_cursor(const State source) const { return CursorType{state_post(source)}; }

	using const_iterator = std::vector<PostType>::const_iterator;
	const_iterator cbegin() const { return state_posts_.cbegin(); }
	const_iterator cend() const { return state_posts_.cend(); }
	const_iterator begin() const { return state_posts_.begin(); }
	const_iterator end() const { return state_posts_.end(); }

	using Transitions = DeltaTransitions<DeltaBase>;

	/**
	 * @brief Get iterator over transitions represented as @c Transition instances.
	 */
	Transitions transitions() const { return Transitions{this}; }

	/**
	 * @brief Get transitions leading to @p state_to.
	 *
	 * @warning Operation is slow, traverses over every transition in the relation.
	 * @param state_to Target state for transitions to get.
	 * @return Transitions leading to @p state_to.
	 */
	std::vector<TransitionType> get_transitions_to(State state_to) const;

	/**
	 * @brief Get transitions from @p state_from to @p state_to.
	 *
	 * @warning Operation is slow, traverses over every transition in the relation.
	 * @param state_from Source state.
	 * @param state_from Target state.
	 * @return Transitions from @p source to @p state_to.
	 */
	std::vector<TransitionType> get_transitions_between(State state_from, State state_to) const;

	/**
	 * @brief Resize the delta to fit the given @p states.
	 *
	 * @tparam States A variadic parameter pack of states to resize the delta for.
	 * @param states States to resize the delta for.
	 */
	template <typename... States>
		requires utils::AllOfType<State, States...>
	DeltaBase& resize_for_states(States... states) {
		if constexpr (sizeof...(states) > 0) {
			if (const State max_state{std::max({static_cast<State>(states)...})}; max_state >= num_of_states()) {
				utils::reserve_on_insert(state_posts_, max_state);
				state_posts_.resize(num_of_states_for_(max_state));
			}
		}
		return *this;
	}

	/**
	 * @brief Get the set of states that are successors of the given @p state.
	 * @param[in] state State from which successors are checked.
	 * @return Set of states that are successors of the given @p state.
	 */
	Successors get_successors(State state) const;

	/**
	 * @brief The targets reachable from @p state under @p key.
	 *
	 * @param[in] state State from which successors are checked.
	 * @param[in] symbol Key under which successors are checked.
	 * @return Set of targets reachable from @p state under @p key.
	 */
	KeyedSuccessors get_successors(State state, Key<0> symbol) const;

	/**
	 * @brief Iterate over @p epsilon symbol posts under the given @p state.
	 *
	 * @param[in] state State from which epsilon transitions are checked.
	 * @param[in] epsilon User can define his favourite epsilon or used default.
	 * @return An iterator to @c PostEntry with epsilon symbol. End iterator when there are no epsilon transitions.
	 */
	PostType::const_iterator epsilon_symbol_posts(State state, Key<0> epsilon = Reserved<0>::min_epsilon) const;

	/**
	 * @brief Iterate over @p epsilon symbol posts under the given @p state_post.
	 *
	 * @param[in] state_post State post from which epsilon transitions are checked.
	 * @param[in] epsilon User can define his favourite epsilon or used default.
	 * @return An iterator to @c PostEntry with epsilon symbol. End iterator when there are no epsilon transitions.
	 */
	static PostType::const_iterator
		epsilon_symbol_posts(const PostType& state_post, Key<0> epsilon = Reserved<0>::min_epsilon);

  protected:
	/// @name Keys on the transitions
	///
	/// Which outermost keys are in use. They ask nothing of a key beyond the ordering the posts already
	///  rely on, so a relation keyed by an interval or a weight has them too; what those keys *stand
	///  for* is that relation's business. Protected for the same reason: a relation re-exports the
	///  ones it wants under the names that fit its keys, with a using-declaration or a forwarder.
	///  @c mata::Delta does the latter, as symbols, since for the shipped relation a key is one.
	///
	/// The three variants indexed *by* the key's value need an unsigned integer for it.
	///@{

	/**
	 * @brief Expand @p target_alphabet by the keys of this relation, as the symbols it hands out.
	 *
	 * Only for an alphabet dealing in this relation's key type, and only for a key that can print
	 *  itself (@c mata::Printable): a new symbol is named by its printed form, and a key without one
	 *  is refused here rather than every symbol getting the same placeholder. The value of the
	 *  already existing symbols will NOT be overwritten.
	 */
	template <ExtensibleAlphabet A>
		requires std::same_as<typename A::Symbol, typename P::Key> && Printable<typename P::Key>
	void add_keys_to(A& target_alphabet) const;

	/**
	 * @brief Get the set of keys used on the transitions.
	 *
	 * Does not necessarily have to equal the set of symbols in the alphabet used by the automaton.
	 */
	utils::OrdVector<typename P::Key> get_used_keys() const
		requires std::totally_ordered<typename P::Key>;
	utils::OrdVector<typename P::Key> get_used_keys_vec() const
		requires std::totally_ordered<typename P::Key>;
	std::set<typename P::Key> get_used_keys_set() const
		requires std::totally_ordered<typename P::Key>;
	utils::SparseSet<typename P::Key> get_used_keys_sps() const
		requires std::unsigned_integral<typename P::Key>;
	/// @note Indexed *by* key, so it allocates up to the largest key used. Already unusable when the automaton has
	/// epsilons.
	/// @throws std::length_error if a key is too large to index a vector.
	std::vector<bool> get_used_keys_bv() const
		requires std::unsigned_integral<typename P::Key>;
	/// @note Indexed *by* key, so it allocates up to the largest key used. Already unusable when the automaton has
	/// epsilons.
	/// @throws std::length_error if a key is too large to index a vector.
	BoolVector get_used_keys_chv() const
		requires std::unsigned_integral<typename P::Key>;

	/**
	 * @brief The greatest key used, or nothing when there are no transitions.
	 *
	 * Over *every* key, epsilons included. That is what the one caller needs: NFT's simulation mints
	 *  fresh symbols at `max + 1` and above, which has to clear the epsilons too or the fresh
	 *  symbols collide with real transitions.
	 */
	std::optional<typename P::Key> get_max_key() const
		requires std::totally_ordered<typename P::Key>;
	///@}

	/**
	 * @brief Collect all transitions from @p from to @p to into @p out.
	 *
	 * @param[in] from Source state of the transitions to collect.
	 * @param[in] to Target state of the transitions to collect.
	 * @param[out] out Vector to collect the transitions into.
	 */
	void collect_transitions_to_(const State from, const State to, std::vector<TransitionType>& out) const {
		if constexpr (key_arity == 1 && std::same_as<Target, State>) {
			// The arity-1, bare-state case is the only one that can be done without a search.
			for (const Entry& entry : state_post(from)) {
				if (entry.targets.contains(to)) { out.push_back(TT::make(from, entry.key(), to)); }
			}
		} else {
			state_post(from).for_each_move([&](const auto&... move) {
				if (state_of(std::get<sizeof...(move) - 1>(std::forward_as_tuple(move...))) == to) {
					out.push_back(TT::make(from, move...));
				}
			});
		}
	}

	/// The largest @c State is the "no state" sentinel: states are 0, ..., max - 1.
	static constexpr State no_state_{std::numeric_limits<State>::max()};
	/// Throws @c std::overflow_error unless @p num_of_states states can be numbered by @c State.
	static void check_num_of_states_(const size_t num_of_states) {
		if constexpr (sizeof(State) < sizeof(size_t)) {
			if (num_of_states > static_cast<size_t>(no_state_)) [[unlikely]] {
				throw std::overflow_error("mata: the number of states does not fit below the no-state sentinel");
			}
		}
	}
	/// The number of states needed to contain @p state, throwing @c std::overflow_error for the sentinel.
	static size_t num_of_states_for_(const State state) {
		if (state == no_state_) [[unlikely]] {
			throw std::overflow_error("mata: the no-state sentinel cannot become a state");
		}
		return static_cast<size_t>(state) + 1;
	}

	std::vector<PostType> state_posts_;
}; // class mata::posts::DeltaBase.

/// A whole relation from its keys, spelled in terms of @c PostChain:
///  `RelationOf<Symbol, StateSet>` is @c mata::Delta.
template <typename... Ts> using RelationOf = DeltaBase<PostChain<Ts...>>;

/**
 * @brief The contract every concrete relation owes.
 *
 * Each check is its own @c static_assert rather than one concept, so that a failure names the check
 *  that failed. A module asks for all of them with a single line,
 *  `static_assert(posts::relation_contract_holds<Delta>());`, which may be repeated in every
 *  translation unit that includes the module's header.
 */
template <typename D> constexpr bool relation_contract_holds() {
	using P = typename D::PostType;
	static_assert(PostLike<P>, "the relation's PostType must be one post of the relation.");
	static_assert(PostEntryLike<typename D::Entry>, "the relation's Entry must be its PostType's entry type.");
	static_assert(TargetSetLike<typename D::TargetSet>, "the innermost post must be a set of targets.");
	static_assert(DeltaLike<D>, "the relation must satisfy the contract mata::Automaton is written against.");

	using Base = DeltaBase<P, typename D::TransitionTraits>;
	static_assert(
		sizeof(D) == sizeof(Base) && alignof(D) == alignof(Base),
		"a relation class must add no members to DeltaBase. It exists only to shorten a name in diagnostics."
	);
	return true;
}

} // namespace posts

namespace posts {
/**
 * @brief Defragment the relation.
 *
 * Removes all state posts which are not in @p is_staying and renames the remaining ones according to
 *  @p renaming. Uses only the public interface, so it needs no friendship.
 *
 * @param[in] delta Relation to defragment.
 * @param[in] is_staying Boolean vector indicating which states are staying in the relation.
 * @param[in] renaming Vector of states to rename the remaining state posts to.
 * @return The defragmented relation.
 */
template <typename P, typename TT>
DeltaBase<P, TT> defragment(
	const DeltaBase<P, TT>& delta,
	const BoolVector& is_staying,
	const std::vector<typename DeltaBase<P, TT>::State>& renaming
);
} // namespace posts

// Makes @c mata::defragment another name for @c posts::defragment, not a second template.
// A second template in @c mata would make unqualified calls ambiguous, as ADL finds both.
using posts::defragment;

} // namespace mata.

#include "mata/core/delta-keys.tpp"
#include "mata/core/delta.tpp"

#endif // MATA_CORE_DELTA_HH
