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
#include <vector>

#include "mata/core/types.hh"

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
};

/**
 * @brief An automaton that can report a run, as @c mata::Automaton::is_lang_empty() needs.
 *
 * Two requirements, both otherwise invisible until a template instantiation fails deep inside:
 *
 *  - @c Run::path is exactly @c std::vector<State>, because the structural search writes into it
 *    directly. Something merely list-like will not do.
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
	{ r.path } -> std::same_as<std::vector<State>&>;
	{ r.word = a.get_word_for_path(r).first.word };
};

/**
 * @brief A transition relation @c mata::Automaton can be built on.
 *
 * The listed operations are exactly what the structural algorithms use; a relation providing them
 *  gets reachability, Tarjan's SCC walk, useful states, distances and trimming for free.
 */
template <typename D>
concept DeltaLike = requires(D d, const D cd, const typename D::State s, const typename D::Target t) {
	typename D::PostType;
	requires PostLike<typename D::PostType>;
	typename D::Target;
	typename D::State;
	requires std::same_as<typename D::State, typename TargetTraits<typename D::Target>::State>;
	{ D::key_arity } -> std::convertible_to<size_t>;
	{ D::state_of(t) } -> std::convertible_to<typename D::State>;

	// Structure.
	{ cd.num_of_states() } -> std::convertible_to<size_t>;
	{ cd.empty() } -> std::convertible_to<bool>;
	{ d.allocate(size_t{}) };
	{ d.clear() };

	// Traversal. These are the only ways the structural algorithms reach a successor.
	{ cd.for_each_successor(s, [](const typename D::Target&) {}) };
	{ cd.for_each_move(s, [](auto&&...) {}) };
	{ cd.successor_cursor(s) };
	{ cd.has_self_loop(s) } -> std::convertible_to<bool>;
};

} // namespace mata.

#endif // MATA_CORE_CONCEPTS_HH
