/** @file
 * @brief Traits for relation and post types.
 *
 * A relation is described by three traits: what a target denotes,
 *  where the reserved keys begin, and how deep a chain of posts is.
 */

#ifndef MATA_CORE_TRAITS_HH
#define MATA_CORE_TRAITS_HH

#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "mata/utils/arg-of.hh"

namespace mata {
namespace detail {
// Returns the default largest ordinary key for an integral key type, given the smallest epsilon key.
template <typename K> consteval K default_max_ordinary(const K min_epsilon) {
	static_assert(
		std::integral<K>,
		"ReservedKeys<K>: only an integral key has a default reserved tail (min_epsilon = max, "
		"max_ordinary = min_epsilon - 1). For any other key spell all three arguments: "
		"ReservedKeys<K, MinEpsilon, MaxOrdinary>."
	);
	if constexpr (std::integral<K>) { return min_epsilon - 1; }
	else { return K{}; } // K{} is never used (just to satisfy the compiler)
}
} // namespace mata::detail.

/**
 * @brief A traits specifying which state a target denotes.
 *
 * A target is whatever the innermost post stores: a state, or a state with a payload.
 *  The structural algorithms need two things from it: @c state_of(target) to know the state it denotes,
 *  and @c with_state(target, state) to rebuild a target with a new state when reverting, renumbering or
 *  trimming. Both are the identity for a bare state.
 *
 * The default covers only a bare state, an integral type. Any other target has to specialise this,
 *  and one that does not is stopped here, rather than being taken for a state and failing deep inside
 *  an algorithm that indexes by it.
 *
 * An example of specialization for a target with MyPayload is:
 * ```cpp
 * template <> struct mata::TargetTraits<MyPayload> {
 *     using State = mata::State;
 *     static State state_of(const MyPayload& t) { return t.state; }
 *     static MyPayload with_state(const MyPayload& t, State s) { return {s, t.payload}; }
 * };
 * ```
 */
template <typename T> struct TargetTraits {
	static_assert(
		std::integral<T>,
		"TargetTraits<T>: a target that is not a bare state has to say which state it denotes. "
		"Specialise mata::TargetTraits<T> with State, state_of() and with_state()."
	);
	using State = T; ///< The state a target denotes.

	// Returns the state a target denotes.
	static constexpr State state_of(const T& target) { return target; }
	// Returns a target with a new state name.
	static constexpr T with_state(const T& /* target */, const State state) { return state; }
};

/**
 * @brief The structure defines where the ordinary keys stop and the reserved tail begins.
 *
 * The key order splits into three regions:
 *  - **ordinary**, up to and including @c max_ordinary
 *  - **reserved but not epsilon**, above @c max_ordinary and below @c min_epsilon
 *  - **epsilon**, from @c min_epsilon upwards
 *
 * The middle region is empty by default, since @c MaxOrdinary defaults to one below @p MinEpsilon.
 *  Widening the reserved tail opens it, which is how a key can be reserved.
 *
 * @tparam K The key type.
 * @tparam MinEpsilon The smallest epsilon key: every key at or above it is an epsilon. Defaults to
 *  the largest value. A non-integral key has no default and must be spelled explicitly.
 * @tparam MaxOrdinary The largest key that is not reserved. Defaults to one below @p MinEpsilon.
 *  Can be defined explicitly to a smaller value to open the reserved-but-not-epsilon region.
 *  A non-integral key has no default and must be spelled explicitly.
 */
template <
	typename K,
	K MinEpsilon = std::numeric_limits<K>::max(),
	K MaxOrdinary = detail::default_max_ordinary<K>(MinEpsilon),
	bool EpsilonIsGreatest = (MinEpsilon == std::numeric_limits<K>::max())
>
struct ReservedKeys {
	using Key = K;
	static constexpr K min_epsilon{MinEpsilon}; ///< The smallest epsilon key.
	static constexpr K max_ordinary{MaxOrdinary}; ///< The largest key that is not reserved.

	// When true, an epsilon will always be at the end of any sorted container (simplifying the search for it).
	static constexpr bool epsilon_is_greatest{EpsilonIsGreatest};

	static_assert(MaxOrdinary < MinEpsilon, "The ordinary keys must stop below the epsilons.");
	static_assert(
		!(std::integral<K> && EpsilonIsGreatest) || MinEpsilon == std::numeric_limits<K>::max(),
		"ReservedKeys<K, ..., EpsilonIsGreatest>: an integral key cannot claim its epsilon is the "
		"greatest key unless min_epsilon is that key. Claiming it wrongly makes epsilon_symbol_posts() "
		"break the algorithms that rely on it."
	);
};

namespace posts {
/// @brief A traits specifying the key arity and target type of a post, and the aliases reading them.
///@{
template <typename X> struct PostTraits {
	static constexpr size_t key_arity{0};
	using Target = typename X::value_type;
};
template <typename X>
	requires requires { X::key_arity; typename X::Target; }
struct PostTraits<X> {
	static constexpr size_t key_arity{X::key_arity};
	using Target = typename X::Target;
};
template <typename X> inline constexpr size_t arity_of = PostTraits<X>::key_arity;
template <typename X> using target_of = typename PostTraits<X>::Target;
///@}

/// @brief The post at depth @p I in a chain of posts, outermost first.
///@{
template <typename P, size_t I> struct PostAt {
	static_assert(I <= arity_of<P>, "no post that deep in this chain");
	using type = typename PostAt<typename P::Nested, I - 1>::type;
};
template <typename P> struct PostAt<P, 0> {
	using type = P;
};
///@}

/// @brief Defines the key type of a post at depth @p I and the tuple of all key types in a chain of posts.
///@{
/**
 * @brief Key @p I's type, taken from the chain of posts @p P, outermost first.
 *
 * @tparam P The chain of posts.
 * @tparam I The index of the key, outermost first.
 * @note @p I must be < the arity of the chain.
 */
template <typename P, size_t I> using KeyOf = typename PostAt<P, I>::type::Key;

namespace detail {
template <typename P, size_t... Is>
auto keys_of(std::index_sequence<Is...>) -> std::tuple<KeyOf<P, Is>...>;
} // namespace mata::posts::detail.

/**
 * @brief The tuple of all key types in a chain of posts @p P, outermost first.
 * For example `std::tuple<KeyOf<P, 0>, ..., KeyOf<P, arity - 1>>`.
 */
template <typename P> using KeysOf = decltype(detail::keys_of<P>(std::make_index_sequence<arity_of<P>>{}));
///@}
} // namespace mata::posts.

} // namespace mata.

#endif // MATA_CORE_TRAITS_HH
