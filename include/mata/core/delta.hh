/** @file
 * @brief The transition relation: a mapping from states and keys to targets.
 *
 * @c DeltaBase is a vector of posts indexed by source state. The rest of the relation lives in the
 *  headers below it, each reading only the ones above it in this list:
 *  - @c mata/core/traits.hh -- what a target denotes, the reserved keys, how deep a chain is (@ref arity)
 *  - @c mata/core/concepts.hh -- the contracts, @c DeltaLike among them
 *  - @c mata/core/transition.hh -- one transition, its traits, and @c DeltaTransitions
 *  - @c mata/core/walks.hh -- the free functions walking a chain of posts at any depth
 *  - @c mata/core/moves.hh -- @c Move and @c PostMoves
 *  - @c mata/core/post.hh -- @c PostEntry, @c Post and @c PostChain (@ref nesting, @ref sortedness)
 *  - @c mata/core/cursor.hh -- @c SuccessorCursor, hand-written per arity
 *
 * Including this header includes all of them. The member bodies are in @c mata/core/delta.tpp, and
 *  the ones for the keys in use in @c mata/core/delta-keys.tpp.
 */

#ifndef MATA_CORE_DELTA_HH
#define MATA_CORE_DELTA_HH

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
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
#include "mata/core/walks.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

namespace mata {
namespace posts {
/**
 * @brief The generic transition relation: a vector of posts indexed by source state.
 *
 * The template behind @c mata::Delta, as @c mata::AutomatonBase is behind @c mata::Automaton; the
 *  shipped relation derives from `DeltaBase<StatePost>` in @c mata/relation.hh.
 *
 * A vector of posts indexed by source state, over a chain of posts ending in a set of targets. For
 *  an NFA that chain is one key deep — a symbol — so it is this class, one @c Post, targets, and the
 *  aliases @c mata::StatePost and @c mata::SymbolPost name that one post and its entry. The depth is
 *  not baked in here: @p P is the whole chain, and @c key_arity is read off it rather than declared;
 *  going deeper adds another @c Post, not another kind of class. Posts are named by key index, not by container count:
 *  @c Key<0> is the symbol an NFA keys by, and @c PostAt<key_arity> is the innermost post.
 *  @see @ref arity for why counting keys and not containers.
 *
 * @tparam P The post reached from one source state, itself parameterised down to the targets. See
 *  @ref nesting.
 * @tparam TT What one transition of this relation looks like and how to build one: a traits with a
 *  @c Type and a @c make(). Defaults to the symbol-named triple at arity 1 and to the tuple-keyed
 *  transition above it. @see mata::posts::DefaultTransitionTraits.
 * @see mata::DeltaLike.
 */
template <typename P, typename TT = DefaultTransitionTraits<P>> class DeltaBase {
  public:
	/// @name Post protocol
	/// @see mata::DeltaLike -- the only way @c mata::Automaton reaches a successor.
	///@{
	using PostType = P; ///< The post reached from one source state.
	using Target = typename PostType::Target; ///< What a successor walk yields.
	/// How the keyed members take a target. @see mata::utils::ArgOf.
	using TargetArg = utils::ArgOf<Target>;
	/// What indexes this relation and the automaton's state sets. Derived from the target type
	///  rather than propagated through the posts: which state a target denotes is a property of
	///  the target, not of any post above it. @see mata::TargetTraits.
	using State = typename TargetTraits<Target>::State;
	static constexpr size_t key_arity{PostType::key_arity};

	/// The post @p I steps in: @c PostAt<0> is the whole chain and @c PostAt<key_arity> is the
	///  innermost post, the set of targets.
	template <size_t I> using PostAt = typename posts::PostAt<PostType, I>::type;
	/**
	 * @brief The key at index @p I, counted from the source state.
	 *
	 * Indexed rather than singular because a relation spans every post: at @c key_arity 2 there is
	 *  no "the key", and a member named @c Key would have meant the outermost one — right at arity 1
	 *  and quietly wrong above it. Key 0 is the one an NFA calls the symbol.
	 *
	 * @c mata::AutomatonBase never names this. It transports keys (in @c reverted(), as
	 *  `const auto&`) without inspecting one, which is why @c mata::DeltaLike does not ask for a key
	 *  type at all.
	 */
	template <size_t I> using Key = typename PostAt<I>::Key;
	/// Where key @p I's ordinary keys stop. Indexed for the same reason as @c Key: each post
	///  carries its own convention, on its own entry. @see mata::ReservedKeys.
	template <size_t I> using Reserved = typename PostAt<I>::Reserved;
	/// The innermost post: what a successor walk collects into. @c PostAt<key_arity> spelled for the
	///  one post that has a name of its own.
	using TargetSet = PostAt<key_arity>;
	/// @copydoc mata::posts::Post::KeyedSuccessors
	using KeyedSuccessors = typename PostType::KeyedSuccessors;
	/// @copydoc mata::TargetTraits::state_of
	static State state_of(const Target& target) { return TargetTraits<Target>::state_of(target); }
	/// The targets under one fully-supplied key, and a transition of this relation.
	using Entry = typename PostType::Entry;
	using Nested = typename PostType::Nested;
	/// The transition traits this relation was instantiated with, and the type they name. What the
	///  shape *is*, and what its fields are called, comes from @p TT so a module can name its own.
	///  @see mata::posts::DefaultTransitionTraits.
	using TransitionTraits = TT;
	using TransitionType = typename TT::Type;
	using CursorType = posts::SuccessorCursor<PostType>;
	///@}

	inline static const PostType empty_state_post; // When posts[q] is not allocated, then delta[q] returns this.

	DeltaBase() : state_posts_{} {}
	DeltaBase(const DeltaBase& other) = default;
	DeltaBase(DeltaBase&& other) = default;
	explicit DeltaBase(const size_t n) : state_posts_{n} {}

	DeltaBase& operator=(const DeltaBase& other) = default;
	DeltaBase& operator=(DeltaBase&& other) = default;

	bool operator==(const DeltaBase& other) const;

	void reserve(const size_t n) { state_posts_.reserve(n); };

	/**
	 * @brief Get constant reference to the state post of @p source.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, an @c empty_post is returned. Hence, the function has no side
	 *  effects (no allocation is performed; iterators remain valid).
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	const PostType& state_post(const State source) const {
		if (source >= num_of_states()) { return empty_state_post; }
		return state_posts_[source];
	}

	/**
	 * @brief Get constant reference to the state post of @p source.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, an @c empty_post is returned. Hence, the function has no side
	 *  effects (no allocation is performed; iterators remain valid).
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	const PostType& operator[](const State source) const { return state_post(source); }

	/**
	 * @brief Get mutable (non-constant) reference to the state post of @p source.
	 *
	 * The function allows modifying the state post.
	 *
	 * BEWARE, IT HAS A SIDE EFFECT.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, a new state post for @p source will be allocated along with
	 *  all state posts for all previous states. This in turn may cause that the entire post data structure is
	 *  re-allocated. Iterators into the relation will get invalidated.
	 * Use the constant 'state_post()' is possible. Or, to prevent the side effect from causing issues, one might want
	 *  to make sure that posts of all states in the automaton are allocated, e.g., write an NFA method that allocate
	 *  the relation for all states of the NFA.
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	PostType& mutable_state_post(State source);

	/**
	 * @brief Defragment the relation.
	 *
	 * This function removes all state posts which are not in @p is_staying and renames the remaining state posts
	 * according to @p renaming.
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
	 * @brief Allocate state posts up to @p num_of_states states, creating empty @c PostType for yet unallocated state
	 *  posts.
	 *
	 * @param[in] num_of_states Number of states in the relation to allocate state posts for. Have to be at least
	 *  num_of_states() + 1.
	 */
	void allocate(const size_t num_of_states) {
		MATA_ASSERT(num_of_states >= this->num_of_states());
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

	void add(State source, Key<0> symbol, TargetArg target)
		requires(P::key_arity == 1);

	/// @name Keyed writes above arity 1
	///
	/// One overload per supported arity rather than one variadic member, for the same reason the
	/// cursor is hand-written per arity: it is the only shape that keeps *declared* parameter types.
	/// A trailing pack would be deduced from the caller, so `add(0, 1, 2, 3)` would carry `int` down
	/// and convert it to the key type once per post inside @c insert_target — a narrowing and a
	/// sign-conversion warning each, which `-Werror` rejects. Declared parameters convert at the
	/// call, exactly as the arity-1 overload above does. The cap is @c key_arity 3, so this is three
	/// overloads, not an open-ended family.
	///
	/// Each is a member template so that its parameter types depend on its own @p Q: `KeyOf<P, 1>`
	/// at @c key_arity 1 would name a post that does not exist, and a member's declared type is
	/// formed when the *class* is instantiated, before any constraint on it is looked at.
	///
	/// The arity-1 overloads are deliberately **not** routed through these recursions. `add` there
	/// has an append fast path (`back().key() < symbol`) that skips the search entirely when a
	/// relation is built in sorted order, which is the common case; @c insert_target always searches
	/// first, and searches twice on a miss.
	///@{
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void add(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) {
		resize_for_states(source, state_of(target));
		posts::insert_target(mutable_state_post(source), target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void add(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) {
		resize_for_states(source, state_of(target));
		posts::insert_target(mutable_state_post(source), target, k0, k1, k2);
	}

	/// @copydoc remove(State, Key<0>, State)
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void remove(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) {
		if (source >= state_posts_.size()) { return; }
		posts::erase_target(state_posts_[source], target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void remove(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) {
		if (source >= state_posts_.size()) { return; }
		posts::erase_target(state_posts_[source], target, k0, k1, k2);
	}

	/// @copydoc contains(State, Key<0>, State) const
	template <typename Q = P>
		requires(Q::key_arity == 2)
	bool contains(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) const {
		return source < state_posts_.size() && posts::has_target_at(state_posts_[source], target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	bool contains(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) const {
		return source < state_posts_.size() && posts::has_target_at(state_posts_[source], target, k0, k1, k2);
	}
	///@}

	/// @name Transition-shaped members
	/// Generic in the arity: a transition is taken apart with its type's @c parts() and the pieces
	///  are forwarded to the keyed overload of the matching arity. What a transition *is* comes from
	///  the traits; only @c Transitions, which has to walk the keys, is still arity 1.
	///@{
	void add(const TransitionType& transition) {
		std::apply([this](const auto&... p) { add(p...); }, TransitionType::parts(transition));
	}
	void remove(State source, Key<0> symbol, TargetArg target)
		requires(P::key_arity == 1);
	void remove(const TransitionType& transition) {
		std::apply([this](const auto&... p) { remove(p...); }, TransitionType::parts(transition));
	}
	/// Check whether the relation contains a passed transition.
	bool contains(State source, Key<0> symbol, TargetArg target) const
		requires(P::key_arity == 1);
	bool contains(const TransitionType& transition) const {
		return std::apply([this](const auto&... p) { return contains(p...); }, TransitionType::parts(transition));
	}
	///@}

	/**
	 * Check whether automaton contains no transitions.
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
	 * @brief Copy posts of delta and apply a lambda update function on each state from
	 * targets.
	 *
	 * IMPORTANT: In order to work properly, the lambda function needs to be
	 * monotonic, that is, the order of states in targets cannot change.
	 *
	 * @param target_renumberer Monotonic lambda function mapping states to different states.
	 * @return std::vector<Post> Copied posts.
	 */
	std::vector<PostType> renumber_targets(const std::function<State(State)>& target_renumberer) const;

	/**
	 * @brief Add transitions to multiple destinations
	 *
	 * @param source From
	 * @param symbol The key of the transition. Named for an NFA's sake, like the field.
	 * @param targets Set of states to
	 */
	void add(State source, Key<0> symbol, const Nested& targets)
		requires(P::key_arity == 1);
	/// @copydoc add(State,Key<0>,const Nested&)
	/// One key per post, then the whole set of targets, at arity 2 and 3.
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void add(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, const TargetSet& targets) {
		for (const Target& target : targets) {
			resize_for_states(source, state_of(target));
			posts::insert_target(mutable_state_post(source), target, k0, k1);
		}
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void add(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		const TargetSet& targets
	) {
		for (const Target& target : targets) {
			resize_for_states(source, state_of(target));
			posts::insert_target(mutable_state_post(source), target, k0, k1, k2);
		}
	}

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
	 * @brief Does @p source have @p target among its successors, under any key?
	 *
	 * @param[in] source Source state to look from.
	 * @param[in] target Target state to look for.
	 */
	bool is_successor(const State source, const State target) const {
		// By the state a target denotes, so a payload target counts whatever it carries.
		return any_target(state_post(source), [target](const Target& t) { return state_of(t) == target; });
	}

	/**
	 * @brief Does @p state have a transition back to itself under any key?
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

	/// The transitions of this relation, one @c TransitionType per (source, key path, target).
	///  @see mata::posts::DeltaTransitions.
	using Transitions = DeltaTransitions<DeltaBase>;

	/**
	 * Iterator over transitions represented as @c Transition instances.
	 */
	Transitions transitions() const { return Transitions{this}; }

	/**
	 * Get transitions leading to @p state_to.
	 * @param state_to[in] Target state for transitions to get.
	 * @return Transitions leading to @p state_to.
	 *
	 * Operation is slow, traverses over every entry of every post.
	 */
	std::vector<TransitionType> get_transitions_to(State state_to) const;

	/**
	 * Get transitions from @p state_from to @p state_to.
	 * @param state_from[in] Source state.
	 * @param state_from[in] Target state.
	 * @return Transitions from @p source to @p state_to.
	 *
	 * Operation is slow, traverses over every entry of every post.
	 */
	std::vector<TransitionType> get_transitions_between(State state_from, State state_to) const;

	/**
	 * @brief Resize the delta to fit the given @p states.
	 * @tparam States A variadic parameter pack of states to resize the delta for.
	 * @param states States to resize the delta for.
	 */
	template <typename... States>
		requires utils::AllOfType<State, States...>
	DeltaBase& resize_for_states(States... states) {
		if constexpr (sizeof...(states) > 0) {
			if (const State max_state{std::max({static_cast<State>(states)...})}; max_state >= num_of_states()) {
				reserve_on_insert(state_posts_, max_state);
				state_posts_.resize(max_state + 1);
			}
		}
		return *this;
	}

	/**
	 * Get the set of states that are successors of the given @p state.
	 * @param[in] state State from which successors are checked.
	 * @return Set of states that are successors of the given @p state.
	 */
	TargetSet get_successors(State state) const;

	/**
	 * @brief The targets reachable from @p state under @p key.
	 *
	 * The *targets*, at every arity. Reaching one step down is @c PostType::find()'s job and
	 *  walking the posts between is @c for_each_move()'s; this answers "which states can I reach
	 *  over this key", which means the same thing however many keys are left below it.
	 *
	 * @see PostType::KeyedSuccessors for why the return type follows the arity — a reference into the
	 *  relation at arity 1, a fresh set above it.
	 */
	KeyedSuccessors get_successors(State state, Key<0> symbol) const;

	/**
	 * Iterate over @p epsilon symbol posts under the given @p state.
	 * @param[in] state State from which epsilon transitions are checked.
	 * @param[in] epsilon User can define his favourite epsilon or used default.
	 * @return An iterator to @c PostEntry with epsilon symbol. End iterator when there are no epsilon transitions.
	 */
	PostType::const_iterator epsilon_symbol_posts(State state, Key<0> epsilon = Reserved<0>::min_epsilon) const;

	/**
	 * Iterate over @p epsilon symbol posts under the given @p state_post.
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
	/// The three variants indexed *by* the key's value need an integer for it.
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
		requires std::integral<typename P::Key>;
	/// @note Indexed *by* key, so it allocates up to the largest key used. Already unusable when
	///  the automaton has epsilons.
	std::vector<bool> get_used_keys_bv() const
		requires std::integral<typename P::Key>;
	/// @copydoc get_used_keys_bv
	BoolVector get_used_keys_chv() const
		requires std::integral<typename P::Key>;

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

	/// Append every transition from @p from to @p to. At arity 1 over bare states each entry's targets
	///  are searched directly; otherwise the key paths are walked and the target checked at the bottom.
	void collect_transitions_to_(const State from, const State to, std::vector<TransitionType>& out) const {
		if constexpr (key_arity == 1 && std::same_as<Target, State>) {
			for (const Entry& entry : state_post(from)) {
				if (entry.targets.find(to) != entry.targets.end()) { out.push_back(TT::make(from, entry.key(), to)); }
			}
		} else {
			state_post(from).for_each_move([&](const auto&... move) {
				if (state_of(std::get<sizeof...(move) - 1>(std::forward_as_tuple(move...))) == to) {
					out.push_back(TT::make(from, move...));
				}
			});
		}
	}

	std::vector<PostType> state_posts_;
}; // class mata::posts::DeltaBase.

/// A whole relation from its keys, spelled in terms of @c PostChain:
///  `RelationOf<Symbol, StateSet>` is @c mata::Delta.
template <typename... Ts> using RelationOf = DeltaBase<PostChain<Ts...>>;

/**
 * @brief The contract every concrete relation owes, stated once.
 *
 * Each check is its own @c static_assert rather than one concept, so that a failure names the check
 *  that failed. A module asks for all of them with a single line,
 *  `static_assert(posts::relation_contract_holds<Delta>());`, which may be repeated in every
 *  translation unit that includes the module's header -- unlike an explicit instantiation
 *  definition, which may appear only once in a program. Checks that belong to one module (its epsilon
 *  constant, its argument-passing policy) stay beside that module's relation.
 */
template <typename D> constexpr bool relation_contract_holds() {
	using P = typename D::PostType;
	static_assert(PostLike<P>, "the relation's PostType must be one post of the relation.");
	static_assert(PostEntryLike<typename D::Entry>, "the relation's Entry must be its PostType's entry type.");
	static_assert(
		ReservedKeysAtTail<P>,
		"the epsilon lookups walk back from the end of a post, which needs the reserved keys to be the last ones."
	);
	static_assert(TargetSetLike<typename D::TargetSet>, "the innermost post must be a set of targets.");
	static_assert(DeltaLike<D>, "the relation must satisfy the contract mata::Automaton is written against.");
	// A relation class deriving from DeltaBase exists only to shorten a name in diagnostics and must
	//  add nothing: the empty-base rules then make it the same size as its base, and the base converts
	//  to it without slicing. Compared against the base with the relation's *own* traits, not the
	//  default ones, or a relation naming other traits would be measured against a different type.
	using Base = DeltaBase<P, typename D::TransitionTraits>;
	static_assert(
		sizeof(D) == sizeof(Base) && alignof(D) == alignof(Base),
		"a relation class must add no members to DeltaBase; it exists only to shorten a name in diagnostics."
	);
	// Already required by DeltaLike; repeated for the message. The cursor is hand-written per arity.
	static_assert(
		D::key_arity <= 3,
		"a relation is capped at key_arity 3 (structure depth 4), because SuccessorCursor is hand-written "
		"per arity and three is where that stops paying. Past it, add a specialisation."
	);
	return true;
}

} // namespace mata::posts.

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
DeltaBase<P, TT> defragment(const DeltaBase<P, TT>& delta, const BoolVector& is_staying,
                    const std::vector<typename DeltaBase<P, TT>::State>& renaming);
} // namespace mata::posts.

/// Callers name this @c mata::defragment. Declared once, in @c posts, and re-exported here: having
///  it in both namespaces makes every unqualified call ambiguous through ADL.
using posts::defragment;

} // namespace mata.

#include "mata/core/delta.tpp"
#include "mata/core/delta-keys.tpp"

#endif // MATA_CORE_DELTA_HH