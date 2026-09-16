/** @file
 * @brief The contracts a new automaton is written against.
 */

#ifndef MATA_CORE_CONCEPTS_HH
#define MATA_CORE_CONCEPTS_HH

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/utils/utils.hh"

namespace mata {
/**
 * @brief A traits specifying which state a target denotes.
 *
 * A target is whatever the innermost post stores: a state, or a state with a payload.
 *  The structural algorithms need two things from it: @c state_of(target) to know the state it denotes,
 *  and @c with_state(target, state) to rebuild a target with a new state when reverting, renumbering or
 *  trimming. Both are the identity for a bare state.
 *
 * Specialise it for a payload target:
 * ```cpp
 * template <> struct mata::TargetTraits<MyPayload> {
 *     using State = mata::State;
 *     static State state_of(const MyPayload& t) { return t.state; }
 *     static MyPayload with_state(const MyPayload& t, State s) { return {s, t.payload}; }
 * };
 * ```
 */
template <typename T> struct TargetTraits {
	using State = T; ///< The state a target denotes. Equal to @c T when a target *is* a state.
	static constexpr State state_of(const T& target) { return target; }
	static constexpr T with_state(const T& /* target */, const State state) { return state; }
};

/**
 * @brief A trait specifying what the alphabet elements are.
 *
 * Defaults to the alphabet's own member alias, which @c mata::Alphabet supplies.
 *  The module seams assert that it is the relation's level-0 key type, since that
 *  is what an alphabet hands out and a relation stores.
 */
template <typename A> struct AlphabetTraits {
	using Symbol = typename A::Symbol;
};

/**
 * @brief A concept for an alphabet that can be *extended* with symbols it has not seen.
 *
 * Only some alphabets can: a fixed @c EnumAlphabet cannot grow, an @c OnTheFlyAlphabet can.
 *  @c mata::posts::DeltaBase::add_keys_to() needs the growing kind.
 */
template <typename A>
concept ExtensibleAlphabet = requires(A& alphabet, typename AlphabetTraits<A>::Symbol symbol) {
	{ alphabet.update_next_symbol_value(symbol) };
	{ alphabet.try_add_new_symbol(std::string{}, symbol) };
};

/**
 * @brief A concept for a value with a printed form.
 */
template <typename T>
concept Printable = std::is_arithmetic_v<T> || std::formattable<T, char>;

namespace detail {
// The default epsilon reserved at the end of the key order can exit only for integral keys.
template <typename K> consteval K default_epsilon() {
	static_assert(
		std::integral<K>,
		"ReservedKeys<K>: only an integral key has a default reserved tail (epsilon = max, "
		"max_ordinary = epsilon - 1). For any other key spell all three arguments: "
		"ReservedKeys<K, Epsilon, MaxOrdinary>."
	);
	if constexpr (std::integral<K>) { return std::numeric_limits<K>::max(); }
	else { return K{}; }
}

// The default maximum ordinary key is just below the epsilon.
template <typename K> consteval K default_max_ordinary(const K epsilon) {
	if constexpr (std::integral<K>) { return epsilon - 1; }
	else { return K{}; }
}
} // namespace mata::detail.

/**
 * @brief The structure defines where the ordinary keys stop and the reserved tail begins.
 *
 * @tparam K The key type.
 * @tparam Epsilon The smallest reserved key: every key at or above it is an epsilon. Defaults to
 *  the largest value. A non-integral key has no default and must be spelled explicitly.
 * @tparam MaxOrdinary The largest key that is not reserved. Defaults to one below @p Epsilon.
 *  Can be defined explicitly to a smaller value to create a gap between the ordinary keys and the reserved tail.
 *  A non-integral key has no default and must be spelled explicitly.
 */
template <typename K, K Epsilon = detail::default_epsilon<K>(), K MaxOrdinary = detail::default_max_ordinary<K>(Epsilon)>
struct ReservedKeys {
	using Key = K;
	static constexpr K epsilon{Epsilon}; ///< The smallest reserved key.
	static constexpr K max_ordinary{MaxOrdinary}; ///< The largest key that is not reserved.
	/// When true, Epsilon will alway be at the end of any sorted containers (simplifying the search for it).
	static constexpr bool epsilon_is_greatest{Epsilon == std::numeric_limits<K>::max()};

	static_assert(MaxOrdinary < Epsilon, "The ordinary keys must stop below the reserved keys.");
};

/**
 * @brief A concept for a reserved-key convention (see @c ReservedKeys).
 */
template <typename R>
concept ReservedKeysLike = requires {
	typename R::Key;
	requires std::totally_ordered<typename R::Key>;
	{ R::epsilon } -> std::convertible_to<typename R::Key>;
	{ R::max_ordinary } -> std::convertible_to<typename R::Key>;
	{ R::epsilon_is_greatest } -> std::convertible_to<bool>;
	typename std::bool_constant<R::epsilon_is_greatest>;
	requires R::max_ordinary < R::epsilon;
};

/**
 * @brief A concept for a post whose reserved keys form a contiguous suffix.
 *
 * @c mata::posts::Post::first_epsilon_it() walks *backwards* from the end, which is right only
 *  if (1) the post is ordered by key and (2) the reserved keys are the top of the key order.
 */
template <typename L>
concept ReservedKeysAtTail = requires {
	typename L::Key;
	typename L::Reserved;
	requires ReservedKeysLike<typename L::Reserved>;
	requires std::same_as<typename L::Key, typename L::Reserved::Key>;
	requires L::sorted_by_key;
};

namespace posts {
/**
 * @brief A traits specifying the key arity and target type of a post.
 */
///@{
template <typename X> struct level_traits {
	static constexpr size_t key_arity{0};
	using Target = typename X::value_type;
};
template <typename X>
	requires requires { X::key_arity; typename X::Target; }
struct level_traits<X> {
	static constexpr size_t key_arity{X::key_arity};
	using Target = typename X::Target;
};
template <typename X> inline constexpr size_t arity_of = level_traits<X>::key_arity;
template <typename X> using target_of = typename level_traits<X>::Target;
///@}
} // namespace mata::posts.

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
 * @brief A concept for a set of targets that can be walked, updated, and queried.
 */
template <typename T>
concept TargetSetLike = WalkableRange<T> && requires(const T t, const posts::target_of<T>& target) {
	typename posts::target_of<T>;
	requires posts::arity_of<T> == 0;
	{ std::ranges::is_sorted(t) } -> std::convertible_to<bool>;
	{ std::declval<T&>().push_back(std::declval<const posts::target_of<T>&>()) };
	{ std::declval<T&>().insert(std::declval<const posts::target_of<T>&>()) };
	{ std::declval<T&>().erase(target) };
	{ t.contains(target) } -> std::convertible_to<bool>;
};

/**
 * @brief A concept for a post entry, a cingle key and the post nested under it.
 */
template <typename E>
concept PostEntryLike = requires(const E e) {
	typename E::Key;
	typename E::Nested;
	requires std::totally_ordered<typename E::Key>;
	{ e.key() } -> std::convertible_to<typename E::Key>;
	{ e.nested() } -> std::convertible_to<const typename E::Nested&>;
	{ std::declval<E&>().nested() } -> std::same_as<typename E::Nested&>;
};

/**
 * @brief A concept for a post, a range of entries that can be walked, updated, and queried.
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
	{ l.is_sorted() } -> std::convertible_to<bool>;
	{ std::declval<L&>().push_back(std::declval<const typename L::Entry&>()) };
	{ std::declval<L&>().find(std::declval<const typename L::Key&>()) };
	{ std::declval<L&>().insert(std::declval<const typename L::Entry&>()) };
	{ l.find(std::declval<const typename L::Key&>()) };
	{ std::declval<L&>().erase(std::declval<const typename L::Entry&>()) };
};

/**
 * @brief A concept for an automaton that can report its runs.
 */
template <typename A>
concept AutomatonWithRuns = requires(const A a, typename A::Run r) {
	typename A::Run;
	typename A::State;
	{ r.path } -> std::same_as<std::vector<typename A::State>&>;
	{ r.word = a.get_word_for_path(r).first.word };
};

/**
 * @brief A concept for a transition relation an @c mata::AutomatonBase can be built on.
 *
 * @note One contract, deliberately whole so the errors are reported immediately, rather
 *  then at the calls inside functions that need them.
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
	requires std::same_as<typename D::State, typename TargetTraits<typename D::Target>::State>;
	{ D::key_arity } -> std::convertible_to<size_t>;
	{ D::state_of(t) } -> std::convertible_to<typename D::State>;

	// At most three keys between a source state and a target. (TODO: make this unbounded).
	// Cursors are optimal until the arity 3, after that a different implementation would be needed.
	requires D::key_arity <= 3;

	// Structure.
	{ cd.num_of_states() } -> std::convertible_to<size_t>;
	{ cd.empty() } -> std::convertible_to<bool>;
	{ d.allocate(size_t{}) };
	{ d.clear() };

	/// Reverting writes into a fresh relation and needs a mutable post to write through.
	{ d.mutable_state_post(s) } -> std::same_as<typename D::PostType&>;

	// Traversal.
	{ cd.for_each_successor(s, [](const typename D::Target&) {}) };
	{ cd.for_each_move(s, [](auto&&...) {}) };
	{ cd.successor_cursor(s) };
	{ cd.has_self_loop(s) } -> std::convertible_to<bool>;

	// Defragmentation.
	{ d.defragment(is_staying, renaming) };

	// Comparison, for @c is_identical().
	requires std::equality_comparable<D>;

	// Value semantics.
	requires std::default_initializable<D>;
	requires std::movable<D>;
	requires std::constructible_from<D, size_t>;
};

} // namespace mata.

#endif // MATA_CORE_CONCEPTS_HH
