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
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/core/traits.hh"
#include "mata/utils/utils.hh"

namespace mata {

/**
 * @brief A concept for an alphabet that can be *extended* with symbols it has not seen.
 *
 * Only some alphabets can: a fixed @c EnumAlphabet cannot grow, an @c OnTheFlyAlphabet can.
 *  @c mata::posts::DeltaBase::add_keys_to() needs the growing kind.
 *
 * The symbol type is the alphabet's own @c Symbol alias, which @c mata::Alphabet supplies. An
 *  alphabet that is not mata's declares one too: it has to name its members the way this concept
 *  does anyway, so it is written for mata and has nothing to adapt from outside.
 */
template <typename A>
concept ExtensibleAlphabet = requires(A& alphabet, typename A::Symbol symbol) {
	{ alphabet.update_next_symbol_value(symbol) };
	{ alphabet.try_add_new_symbol(std::string{}, symbol) };
};

/**
 * @brief A concept for a value with a printed form: one @c std::format can print.
 *
 * A user type gets one by specialising @c std::formatter. @see mata::utils::format_or_unprintable.
 */
template <typename T>
concept Printable = std::formattable<T, char>;

/**
 * @brief A concept for a reserved-key convention (see @c ReservedKeys).
 */
template <typename R>
concept ReservedKeysLike = requires {
	typename R::Key;
	requires std::totally_ordered<typename R::Key>;
	{ R::min_epsilon } -> std::convertible_to<typename R::Key>;
	{ R::max_ordinary } -> std::convertible_to<typename R::Key>;
	{ R::epsilon_is_greatest } -> std::convertible_to<bool>;
	typename std::bool_constant<R::epsilon_is_greatest>;
	requires R::max_ordinary < R::min_epsilon;
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
concept TargetSetLike = WalkableRange<T> && requires(const T t, const target_of<T>& target) {
	typename target_of<T>;
	requires arity_of<T> == 0;
	{ std::ranges::is_sorted(t) } -> std::convertible_to<bool>;
	{ std::declval<T&>().push_back(std::declval<const target_of<T>&>()) };
	{ std::declval<T&>().insert(std::declval<const target_of<T>&>()) };
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

	// At most three keys between a source state and a target.
	// Cursors are optimal until the arity 3, after that a different implementation would be needed.
	// TODO: Make this unbounded.
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
