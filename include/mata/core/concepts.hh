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
 * Alongside them is the *key* vocabulary, which @c mata::Automaton itself never needs -- it
 *  transports keys without inspecting one (see the Plan, §3.7) -- but which the relation does, for
 *  the members that talk about epsilons and about symbols:
 *
 *  - @c KeyTraits, saying which symbols a key admits, and @c KeyDenotesSymbols for keys that admit
 *    any. A property of the key *type*, hence a traits specialised on it.
 *  - @c ReservedKeys, saying where a level's ordinary keys stop. A property of the *relation* --
 *    an NFA and an NFT share the key type and differ here -- hence a template argument.
 *  - @c ReservedKeysAtTail, the invariant that makes finding the epsilons by a backwards walk
 *    sound. @c PostLike is defined in terms of it.
 *
 * @section nesting Post structure
 *
 * A transition relation is @c Delta indexed by source state, over a chain of posts, ending in a
 *  set of targets. One post per key, so a relation with @c n keys is @c n posts deep:
 *
 * ```
 * Delta -> Post -> Post -> ... -> Targets
 *            key 0   key 1         (a state, or a state with a payload)
 * ```
 *
 * A *post* (@c PostLike, @c mata::posts::Post) is an ordered map from one key to the post nested
 *  under it. Iterating one yields its *entries* (@c PostEntryLike, @c mata::posts::PostEntry) --
 *  one key paired with the post beneath it. An entry is a detail of the post it belongs to, not a
 *  post in its own right.
 *
 * The classes are named for what they *are*, not for what the depth-2 relation calls them, because
 *  the same two classes are every level of the chain. @c mata::StatePost and @c mata::SymbolPost are
 *  **aliases** for the depth-2 instantiation, in the same way @c mata::Delta and
 *  @c mata::Transition are: they name the post a state maps to and its entry, which is what an NFA
 *  has and all it has. Nothing nests a "state post" inside a "symbol post".
 *
 * @section arity Counting posts
 *
 * @c key_arity is the number of keys between a source state and a target: 1 for an NFA (the
 *  symbol), 2 for a two-tape relation, and so on. It deliberately avoids the word "level", which is
 *  taken twice over. @c mata::Level and @c mata::nft::Levels are an NFT's tape levels, wholly
 *  unrelated to how deep a relation nests. And counting *containers* rather than keys gives a number
 *  that depends on where you start: the same structure was documented as "three-level" in
 *  @c mata/nfa/nfa.hh and "four-level" in @c mata/core/delta.hh, both describing this one. Both
 *  passages now count keys.
 *
 * Levels are named by key index throughout: `mata::posts::Delta::Key<I>` is level @p I's key,
 *  `mata::posts::Delta::Reserved<I>` its reserved-key convention, and
 *  `mata::posts::Delta::PostAt<I>` the post that far in. A *post* keeps the singular @c Key,
 *  because a post has exactly one and there is nothing to disambiguate; a relation spans every
 *  level, and there a singular name would have silently meant the outermost one.
 *
 * @section sortedness Sortedness
 *
 * Every post is sorted by key, and the innermost one by target. This is not an implementation
 *  detail that happens to hold -- lookups binary-search on it, and @c Post::first_epsilon_it()
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
#include <limits>
#include <type_traits>
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
 * @brief Which symbols a key admits.
 *
 * A key is not always a symbol. It is for an NFA, where a key *is* the one symbol it stands for,
 *  but a relation may key its transitions by an interval, a character class or a predicate, and
 *  then "the symbols used on the transitions" is not the set of keys under another name -- it is
 *  the union of their expansions. A different computation, so @c Delta::get_used_symbols() and its
 *  siblings are written against this traits in order to stay *correct* for such a key rather than
 *  merely compiling. See the Plan, §3.13.
 *
 * Unlike @c TargetTraits there is deliberately **no primary definition**: a key type has to say
 *  that it denotes symbols, because plenty do not. A weight-keyed or probability-keyed level has no
 *  answer to give and should get no member rather than a wrong one. The identity expansion is
 *  supplied for integral keys, which is what @c mata::Symbol needs.
 *
 * Specialise it to key transitions by something that is not a symbol:
 * ```cpp
 * template <> struct mata::KeyTraits<Interval> {
 *     using SymbolType = mata::Symbol;
 *     template <typename Fn> static void for_each_symbol(const Interval& key, Fn&& fn) {
 *         for (SymbolType s{key.lo}; s <= key.hi; ++s) { fn(s); }
 *     }
 *     static bool admits(const Interval& key, const SymbolType s) { return key.lo <= s && s <= key.hi; }
 * };
 * ```
 */
template <typename K> struct KeyTraits;

/**
 * @brief The identity expansion: an integral key *is* the single symbol it admits.
 *
 * @note An integral key that is *not* a symbol -- an integer weight, say -- satisfies
 *  @c KeyDenotesSymbols through this specialisation, and would get the symbol members with the
 *  weights answering as symbols. Distinguishing the two needs a distinct key type, which is the
 *  right way to spell it anyway; there is nothing in the type `unsigned long` to tell them apart.
 */
template <std::integral K> struct KeyTraits<K> {
	/// Spelled @c SymbolType, not @c Symbol, so that it cannot shadow @c mata::Symbol wherever a
	///  post or a relation re-exports it.
	using SymbolType = K;
	template <typename Fn> static void for_each_symbol(const K key, Fn&& fn) { fn(key); }
	static bool admits(const K key, const K symbol) { return key == symbol; }
};

/**
 * @brief A key that stands for a set of symbols, so that "which symbols are used" is a question it
 *  can answer.
 *
 * The guard on the symbol-specific members of @c mata::posts::Delta. @see KeyTraits.
 */
template <typename K>
concept KeyDenotesSymbols = requires(const K key) {
	typename KeyTraits<K>::SymbolType;
	{ KeyTraits<K>::for_each_symbol(key, [](typename KeyTraits<K>::SymbolType) {}) };
	{ KeyTraits<K>::admits(key, std::declval<typename KeyTraits<K>::SymbolType>()) }
		-> std::convertible_to<bool>;
};

/**
 * @brief Does @p A deal in the symbols that @p K's keys denote?
 *
 * An automaton holds two things that have to agree about what a symbol is: its relation, whose
 *  level-0 keys *denote* symbols, and its alphabet, which hands them out. Nothing checks that today
 *  because both are @c mata::Symbol by construction and cannot disagree. The moment either becomes
 *  configurable they can, and the failure is the silent kind: `translate_symb()` returns the
 *  alphabet's symbol type, which *implicitly converts* to the relation's key type, so mismatched
 *  widths truncate at some values and not others. No diagnostic, wrong automaton.
 *
 * Note it compares the symbol a key **denotes**, not the key itself. For an interval-keyed relation
 *  @c Key<0> is the interval while the symbol type is @c mata::Symbol, so
 *  `same_as<Key<0>, A::Symbol>` would be the wrong question. @see KeyTraits.
 *
 * Vacuously true for a key that denotes no symbols — a weight-keyed relation has no alphabet
 *  relationship to get wrong, and arguably no alphabet member either.
 *
 * @warning This is a check on the *type*, not on the *range*. An alphabet handing out @c EPSILON as
 *  an ordinary symbol satisfies it and is still wrong; @c ReservedKeys::max_ordinary is the number
 *  to validate values against, at runtime.
 */
namespace detail {
/// Vacuous when @p K denotes no symbols; the real comparison otherwise. Spelled as a class template
///  on a plain @c bool rather than as a disjunction, because `!KeyDenotesSymbols<K> || same_as<...>`
///  would have to form `KeyTraits<K>::SymbolType` to be a valid expression even when the left side
///  already settled it.
template <typename K, typename A, bool = KeyDenotesSymbols<K>> struct SymbolTypeAgrees : std::true_type {};
template <typename K, typename A>
struct SymbolTypeAgrees<K, A, true>
	: std::bool_constant<std::same_as<typename KeyTraits<K>::SymbolType, typename A::Symbol>> {};
} // namespace mata::detail.

/// @copydoc mata::detail::SymbolTypeAgrees
template <typename K, typename A>
concept SymbolTypeAgrees = detail::SymbolTypeAgrees<K, A>::value;

/**
 * @brief The guard on a relation member that only means anything for a key denoting symbols.
 *
 * Written over a *defaulted member template parameter* -- `template <typename K = Key> requires
 *  SymbolKeyOf<K, Key>` -- rather than as a plain `requires KeyDenotesSymbols<Key>` on the member,
 *  and the reason is not style. The return types of those members mention
 *  `KeyTraits<Key>::SymbolType`, and a member's declared type is formed when the *class* is
 *  instantiated, before any constraint on it is looked at. An explicit return type naming
 *  `KeyTraits<Key>` therefore makes @c mata::posts::Delta over a key that denotes no symbols fail
 *  to instantiate at all, rather than merely lack the member -- checked, and it does exactly that.
 *  Deferring the return type to the member's own parameter is what keeps it lazy.
 *
 * @c std::same_as pins that parameter back to the relation's own key, so it cannot be supplied by
 *  hand to ask a relation about somebody else's key type.
 */
template <typename K, typename Expected>
concept SymbolKeyOf = std::same_as<K, Expected> && KeyDenotesSymbols<K>;

/**
 * @brief Where one key level's ordinary keys stop and its reserved ones begin.
 *
 * Epsilon is not a property of the key *type*, which is why this is a descriptor passed as a
 *  template argument and not a traits specialised on @c K like @c KeyTraits. An NFA and an NFT both
 *  key by @c mata::Symbol and both call the largest value epsilon; what differs is that an NFT
 *  reserves a *wider tail* (`DONT_CARE = EPSILON - 1`). A traits keyed on the key type could not
 *  tell the two apart, because there is only one key type. The convention belongs to the relation.
 *
 * That is exactly what makes @c mata::posts::Post::moves_epsilons(),
 *  @c mata::posts::Post::moves_symbols() and @c mata::posts::Delta::epsilon_symbol_posts()
 *  resolve their defaults *per instantiation*, instead of each picking up whichever constant
 *  happened to be in scope where the default argument was written. See the Plan, §3.8.
 *
 * @tparam K The key type.
 * @tparam Epsilon The smallest reserved key: every key at or above it is an epsilon.
 * @tparam MaxOrdinary The largest key that is not reserved. Defaults to one below @p Epsilon, which
 *  is right whenever epsilon is the only reserved key -- and wrong for a wider reserved tail, which
 *  is the whole reason it is a separate parameter rather than computed.
 */
template <typename K, K Epsilon = std::numeric_limits<K>::max(), K MaxOrdinary = Epsilon - 1>
struct ReservedKeys {
	using Key = K;
	static constexpr K epsilon{Epsilon}; ///< The smallest reserved key.
	static constexpr K max_ordinary{MaxOrdinary}; ///< The largest key that is not reserved.
	/// True when no key can sort above @c epsilon, so at most one entry can carry it and that entry
	///  is the last one. @c mata::posts::Delta::epsilon_symbol_posts() takes an O(1) path on it
	///  instead of searching. Conservatively false for a key type without a known maximum.
	static constexpr bool epsilon_is_greatest{Epsilon == std::numeric_limits<K>::max()};

	static_assert(MaxOrdinary < Epsilon, "the ordinary keys must stop below the reserved tail");
};

/**
 * @brief A reserved-key convention: @see ReservedKeys.
 */
template <typename R>
concept ReservedKeysLike = requires {
	typename R::Key;
	requires std::totally_ordered<typename R::Key>;
	{ R::epsilon } -> std::convertible_to<typename R::Key>;
	{ R::max_ordinary } -> std::convertible_to<typename R::Key>;
	{ R::epsilon_is_greatest } -> std::convertible_to<bool>;
	/// The reserved keys are the *top* of the key order. @see ReservedKeysAtTail.
	requires R::max_ordinary < R::epsilon;
};

/**
 * @brief A post whose reserved keys form a contiguous suffix.
 *
 * @c mata::posts::Post::first_epsilon_it() finds the smallest epsilon by walking *backwards*
 *  from the end until it drops below the threshold, and returns the position after that. This is
 *  the right answer only if the keys at or above the threshold are exactly the last ones, which
 *  takes two separate things -- and neither of them fails to compile on its own:
 *
 *  - the post is ordered by key (@c sorted_by_key, @ref sortedness), so a backwards walk sees keys
 *    in decreasing order; and
 *  - the reserved keys are the *top* of the key order (@c max_ordinary below @c epsilon), so that
 *    "reserved" and "at the end" mean the same thing.
 *
 * A descriptor with the two the wrong way round would hand @c moves_epsilons() and
 *  @c moves_symbols() each other's ranges, silently and with no diagnostic anywhere. Hence a
 *  concept. @c PostLike is defined in terms of it, so every post of the relation is checked where
 *  it is named.
 */
template <typename L>
concept ReservedKeysAtTail = requires {
	typename L::Key;
	typename L::Reserved;
	requires ReservedKeysLike<typename L::Reserved>;
	requires std::same_as<typename L::Key, typename L::Reserved::Key>;
	requires L::sorted_by_key;
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
concept PostLike = WalkableRange<L> && ReservedKeysAtTail<L> && requires(const L l) {
	typename L::Entry;
	requires PostEntryLike<typename L::Entry>;
	typename L::Key;
	typename L::Nested;
	typename L::Target;
	{ L::key_arity } -> std::convertible_to<size_t>;
	requires L::key_arity >= 1;
	/// @c sorted_by_key and the reserved-key convention both come from @c ReservedKeysAtTail above:
	///  separately they are two unrelated-looking requirements, and together they are the one
	///  invariant that makes the epsilon lookups sound. @see @ref sortedness.
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
	const BoolVector& is_staying,
	const std::vector<typename D::State>& renaming
) {
	typename D::PostType;
	requires PostLike<typename D::PostType>;
	typename D::Target;
	typename D::State;
	/// **No key type is asked for.** @c mata::AutomatonBase transports keys — @c reverted() takes a
	///  move apart and writes it back — but never inspects, compares or stores one, so requiring a
	///  key here would be requiring something nothing uses. It would also have to pick a *level*,
	///  and at a @c key_arity above one there is no "the key". See the Plan, §3.7, and
	///  @c mata::posts::Delta::Key for the indexed spelling a relation offers its own users.
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
	//
	// @c add() is deliberately **not** required. It used to be, and it was wrong twice over: nothing
	//  in @c mata::AutomatonBase calls it (@c reverted() writes through
	//  @c mata::posts::insert_target and @c mutable_state_post above), and `add(source, key, target)`
	//  names exactly one key, so requiring it here promised a member that a relation of arity 2 or 3
	//  could satisfy in its *declaration* and then fail inside — the deep-instantiation diagnostic
	//  §3.10 exists to avoid. Writing a key path is the post chain's job, and its requirements are
	//  already above: @c PostLike's @c find and @c insert, @c PostEntryLike's mutable @c nested(),
	//  and @c TargetSetLike's @c insert.
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
