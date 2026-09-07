/** @file
 * @brief The contracts a new automaton is written against.
 *
 * Three things live here, and between them they are everything someone needs to satisfy in order
 *  to build an automaton on @c mata::Automaton and get its structural operations -- reachability,
 *  Tarjan's SCC walk, useful states, distances, trimming, emptiness -- unchanged:
 *
 *  - @c DeltaLike, the transition relation. @c mata::Automaton reaches successors only through it.
 *  - @c TargetTraits, saying which state a target denotes. The one thing a payload target must
 *    supply.
 *  - @c AutomatonWithRuns, for the automata that report a counter-example run.
 *
 * @section nesting Post structure
 *
 * A transition relation is @c Delta indexed by source state, over a chain of posts, ending in a
 *  set of targets:
 *
 * ```
 * Delta -> Post<Symbol, Post<Symbol, ... Targets>>
 * ```
 *
 * A *post* (@c PostLike) is an ordered map from one key to the post nested under it: @c StatePost
 *  is exactly `Symbol -> targets`. One key adds one post, so a relation with @c n keys is @c n
 *  posts deep and the nesting reads off directly.
 *
 * Iterating a post yields its *entries* (@c PostEntryLike) -- @c SymbolPost is @c StatePost's
 *  entry type, pairing one key with the post under it. An entry is a detail of the post it belongs
 *  to, not a post in its own right.
 *
 * @section arity Counting posts
 *
 * @c key_arity is the number of keys between a source state and a target: 1 for an NFA (the
 *  symbol), 2 for a two-tape relation, and so on. It deliberately avoids the word "level", which
 *  is taken twice over: @c mata::Level and @c mata::nft::Levels are an NFT's tape levels, wholly
 *  unrelated to how deep a relation nests, and the older documentation uses it for two different
 *  structural counts (@c mata/nfa/nfa.hh says "three-level", @c mata/core/delta.hh says
 *  "four-level", both counting containers rather than keys).
 *
 * @section sortedness Sortedness
 *
 * Every post is sorted by key, and the innermost one by target. This is not an implementation
 *  detail that happens to hold -- lookups binary-search on it, and @c StatePost::first_epsilon_it()
 *  walks backwards relying on reserved keys forming a contiguous suffix. A post that does not
 *  maintain it will not fail to compile; it will silently return wrong iterators.
 *
 * Sortedness may be broken *temporarily*: @c push_back and @c emplace_back append without
 *  restoring order, which is faster when building a post from unordered input. The invariant has to
 *  be restored (by sorting) before any lookup, iteration order, or comparison is relied upon.
 *  A post advertises that it maintains the invariant with @c sorted_by_key / @c sorted_by_target,
 *  and exposes @c is_sorted() so debug builds can check it.
 */

#ifndef MATA_CORE_CONCEPTS_HH
#define MATA_CORE_CONCEPTS_HH

#include <concepts>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "mata/core/types.hh"
#include "mata/utils/utils.hh"

namespace mata {

/**
 * @brief What a target is, and which state it denotes.
 *
 * A target is whatever the innermost post stores. For a plain automaton that is just a state; for
 *  a relation carrying a payload it might be a state paired with an output symbol, a weight, or a
 *  probability. The structural algorithms only ever need to know which *state* a target denotes,
 *  and they get that from here.
 *
 * This is a property of the target type, not of any post, so it lives in one specialisable traits
 *  template rather than being threaded as a member through every post of the nesting.
 *
 * Specialise it to introduce a payload target:
 * ```cpp
 * template <> struct mata::TargetTraits<MyPayload> {
 *     using State = mata::State;
 *     static State state_of(const MyPayload& t) { return t.state; }
 * };
 * ```
 */
template <typename T> struct TargetTraits {
	using State = T; ///< The state a target denotes. Equal to @c T when a target *is* a state.
	/// Identity for a plain target. Collapses to nothing at @c -O2.
	static State state_of(const T& target) { return target; }
};


/**
 * @brief A range that can be walked and whose emptiness can be tested.
 */
template <typename R>
concept WalkableRange = requires(const R r) {
	{ r.begin() } -> std::input_or_output_iterator;
	{ r.end() } -> std::sentinel_for<decltype(r.begin())>;
	{ r.empty() } -> std::convertible_to<bool>;
	{ r.size() } -> std::convertible_to<size_t>;
};

/**
 * @brief The innermost post: the targets reachable once every key has been supplied.
 *
 * @c Target is what a successor walk yields; @c State is what indexes the automaton (its
 *  @c initial and @c final sets, and @c Delta itself). They coincide for a plain automaton, and
 *  differ as soon as a target carries a payload, which is why @c state_of() exists: it is the only
 *  thing a payload target has to provide for every structural operation to keep working.
 */
template <typename T>
concept TargetSetLike = WalkableRange<T> && requires(const T t, const typename T::Target& target) {
	typename T::Target;
	{ T::key_arity } -> std::convertible_to<size_t>;
	requires T::key_arity == 0;
	/// @see @ref sortedness. Sorted by target.
	requires T::sorted_by_target;
	{ t.is_sorted() } -> std::convertible_to<bool>;
	/// Re-constructible by appending. Trimming and renumbering rebuild a post rather than mutating
	///  it, which needs only this and not a filter-and-rename pair — one requirement on an
	///  implementer instead of two. Both callers append in increasing order, so the sortedness
	///  invariant @c push_back would otherwise break is preserved. @see @ref sortedness.
	{ std::declval<T&>().push_back(std::declval<const typename T::Target&>()) };
	/// Reverting writes a target down a key path and lands here. @see mata::posts::insert_target.
	{ std::declval<T&>().insert(std::declval<const typename T::Target&>()) };
};

/**
 * @brief One entry of a post: a single key together with the post nested under it.
 *
 * What iterating a @c PostLike yields. Not a post itself -- it holds one key, where a post
 *  holds many.
 */
template <typename E>
concept PostEntryLike = requires(const E e) {
	typename E::Key;
	typename E::Nested;
	requires std::totally_ordered<typename E::Key>;
	{ e.key() } -> std::convertible_to<typename E::Key>;
	{ e.nested() } -> std::convertible_to<const typename E::Nested&>;
	/// Mutable too: writing a target down a key path descends through the entries.
	{ std::declval<E&>().nested() } -> std::same_as<typename E::Nested&>;
};

/**
 * @brief One post of the relation: an ordered map from a key to the post nested under it.
 *
 * Recursive: @c Nested is either another post or, at the innermost step, a @c TargetSetLike.
 *  @c key_arity counts the keys from here down, so it is one more than the nested post's.
 */
template <typename L>
concept PostLike = WalkableRange<L> && requires(const L l) {
	typename L::Entry;
	requires PostEntryLike<typename L::Entry>;
	typename L::Key;
	typename L::Nested;
	typename L::Target;
	{ L::key_arity } -> std::convertible_to<size_t>;
	requires L::key_arity >= 1;
	/// @see @ref sortedness. Ordered by the key of the contained entries.
	requires L::sorted_by_key;
	{ l.is_sorted() } -> std::convertible_to<bool>;
	/// Re-constructible by appending, for the same reason as @c TargetSetLike. Entries are appended
	///  in increasing key order, so sortedness holds.
	{ std::declval<L&>().push_back(std::declval<const typename L::Entry&>()) };
	/// Writing a key path creates the levels it passes through. @see mata::posts::insert_target.
	{ std::declval<L&>().find(std::declval<const typename L::Key&>()) };
	{ std::declval<L&>().insert(std::declval<const typename L::Entry&>()) };
};

/**
 * @brief An automaton that can report a run, as @c mata::Automaton::is_lang_empty() needs.
 *
 * Two requirements, both otherwise invisible until a template instantiation fails deep inside:
 *
 *  - @c Run::path is exactly @c std::vector of the automaton's own @c State, because the
 *    structural search writes into it directly. Something merely list-like will not do, and
 *    neither will a vector of some other state type.
 *  - the automaton can read one of its own runs as a word. What a path *reads* is not structural
 *    (flat for an NFA, interleaved by tape for an NFT, a tuple per step at higher key arities), so
 *    only the automaton itself can say.
 *
 * @c Run is taken from the automaton rather than fixed here, so each one says what a run means for
 *  it.
 */
template <typename A>
concept AutomatonWithRuns = requires(const A a, typename A::Run r) {
	typename A::Run;
	typename A::State;
	{ r.path } -> std::same_as<std::vector<typename A::State>&>;
	{ r.word = a.get_word_for_path(r).first.word };
};

/**
 * @brief A transition relation @c mata::AutomatonBase can be built on.
 *
 * One contract, deliberately whole. A relation either provides all of this and gets every
 *  structural operation -- reachability, Tarjan's SCC walk, useful states, distances in both
 *  directions, acyclicity, structural comparison and trimming -- or it does not satisfy the
 *  concept, and @c mata::AutomatonBase<D> is then rejected where it is *named*.
 *
 * Splitting the write side out into opt-in concepts (`RevertibleDeltaLike`, `TrimmableDeltaLike`)
 *  was tried and reverted; see the Plan, S3.11. It moved each failure from the instantiation to
 *  the first call, and a call to a structural operation is usually made from inside somebody
 *  else's template, which is the diagnostic S3.10 exists to avoid. It also bought nothing: no
 *  relation, in the tree or planned, can read but not write.
 *
 * The requirements are grouped by what needs them, so that anything added to
 *  @c mata::AutomatonBase which is not covered here is visible as a gap rather than as a
 *  compile error from inside a member.
 */
template <typename D>
concept DeltaLike = requires(
	D d,
	const D cd,
	const typename D::State s,
	const typename D::Target t,
	const typename D::Key k,
	const BoolVector& is_staying,
	const std::vector<typename D::State>& renaming
) {
	typename D::PostType;
	requires PostLike<typename D::PostType>;
	typename D::Target;
	typename D::State;
	typename D::Key;
	requires std::same_as<typename D::State, typename TargetTraits<typename D::Target>::State>;
	{ D::key_arity } -> std::convertible_to<size_t>;
	{ D::state_of(t) } -> std::convertible_to<typename D::State>;

	/**
	 * The cap: structure depth 4. @see §3.3 for the depth/arity conversion and §3.3b for why 4.
	 *
	 * Relaxed from `== 1` by T3.1-T3.3, which made every read generic — both walks, the cursor,
	 *  trimming, renumbering, structural equality and the transition count. Enforced here rather
	 *  than at the walks (the dropped T3.4) because @c mata::AutomatonBase needs the cursor
	 *  unconditionally: Tarjan drives @c get_useful_states(), @c is_acyclic(), @c is_lang_empty() and
	 *  @c trim() through it, so a relation past the cap would satisfy a walk-only concept and then
	 *  fail somewhere inside Tarjan.
	 */
	requires D::key_arity <= 3;

	// Structure.
	{ cd.num_of_states() } -> std::convertible_to<size_t>;
	{ cd.empty() } -> std::convertible_to<bool>;
	{ d.allocate(size_t{}) };
	{ d.clear() };

	/// Reverting writes into a fresh relation and needs a mutable post to write through.
	{ d.mutable_state_post(s) } -> std::same_as<typename D::PostType&>;

	// Traversal. These are the only ways the structural algorithms reach a successor.
	{ cd.for_each_successor(s, [](const typename D::Target&) {}) };
	{ cd.for_each_move(s, [](auto&&...) {}) };
	{ cd.successor_cursor(s) };
	{ cd.has_self_loop(s) } -> std::convertible_to<bool>;

	// Writing. @c reverted() rebuilds a relation one transition at a time; @c trim() works out
	//  which states stay and what they are renamed to, then leaves applying both to the relation,
	//  which is the only party that knows its own representation.
	{ d.add(s, k, s) };
	{ d.defragment(is_staying, renaming) };

	// Comparison, for @c is_identical().
	requires std::equality_comparable<D>;

	// Value semantics. @c mata::AutomatonBase holds a @p D by value: it stores one, default
	//  constructs one, moves them, and constructs one presized to a state count. The last is asked
	//  for as a constructor rather than routed through @c allocate() so that a relation able to
	//  build itself presized in one step may do so, and so that the postcondition does not rest on
	//  what @c allocate() happens to do when starting from zero states. Copyability is deliberately
	//  *not* required: a non-copyable relation merely leaves the defaulted copy constructor of
	//  @c mata::AutomatonBase deleted, which is not an error.
	requires std::default_initializable<D>;
	requires std::movable<D>;
	requires std::constructible_from<D, size_t>;
};

} // namespace mata.

#endif // MATA_CORE_CONCEPTS_HH
