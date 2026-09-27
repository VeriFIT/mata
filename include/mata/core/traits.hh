/** @file
 * @brief The traits a relation is described by: what a target denotes, where the reserved keys begin,
 *  and how deep a chain of posts is.
 *
 * The bottom of @c mata/core/. Every other core header reads these; this one reads none of them.
 *
 * @section arity Counting posts
 *
 * @c key_arity is the number of keys between a source state and a target: 1 for an NFA (the symbol),
 *  2 for a two-tape relation, and so on. Keys, not containers -- counting containers gives a number
 *  that depends on where you start. Unrelated to @c mata::Level and @c mata::nft::Levels, which are
 *  an NFT's tape levels. Posts are named by key index: `DeltaBase::Key<I>` is key @p I's type,
 *  `Reserved<I>` its reserved-key convention, and `PostAt<I>` the post that far in.
 */

#ifndef MATA_CORE_TRAITS_HH
#define MATA_CORE_TRAITS_HH

#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace mata {
namespace detail {
// The default smallest epsilon exists only for integral keys.
template <typename K> consteval K default_min_epsilon() {
	static_assert(
		std::integral<K>,
		"ReservedKeys<K>: only an integral key has a default reserved tail (min_epsilon = max, "
		"max_ordinary = min_epsilon - 1). For any other key spell all three arguments: "
		"ReservedKeys<K, MinEpsilon, MaxOrdinary>."
	);
	if constexpr (std::integral<K>) { return std::numeric_limits<K>::max(); }
	else { return K{}; }
}

// The default maximum ordinary key is just below the smallest epsilon, leaving no gap.
template <typename K> consteval K default_max_ordinary(const K min_epsilon) {
	if constexpr (std::integral<K>) { return min_epsilon - 1; }
	else { return K{}; }
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
 *  an algorithm that indexes by it. An example of specialization for a target with MyPayload is:
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
	using State = T; ///< The state a target denotes. Equal to @c T when a target *is* a state.
	static constexpr State state_of(const T& target) { return target; }
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
	K MinEpsilon = detail::default_min_epsilon<K>(),
	K MaxOrdinary = detail::default_max_ordinary<K>(MinEpsilon)
>
struct ReservedKeys {
	using Key = K;
	static constexpr K min_epsilon{MinEpsilon}; ///< The smallest epsilon key.
	static constexpr K max_ordinary{MaxOrdinary}; ///< The largest key that is not reserved.
	// When true, an epsilon will always be at the end of any sorted container (simplifying the search for it).
	static constexpr bool epsilon_is_greatest{MinEpsilon == std::numeric_limits<K>::max()};

	static_assert(MaxOrdinary < MinEpsilon, "The ordinary keys must stop below the epsilons.");
};

/**
 * @brief A traits specifying the key arity and target type of a post.
 */
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

namespace posts {
/// How a parameter of type @p X is taken: by value when it is small and trivially copyable (a bare
///  state stays in a register), by reference otherwise (a payload owning memory is not copied to be
///  looked at). A reference to a small trivial value would force it into memory at every call that
///  does not inline, since a reference needs something to point at.
template <typename X>
using ArgOf = std::conditional_t<std::is_trivially_copyable_v<X> && sizeof(X) <= 2 * sizeof(void*), const X, const X&>;

/**
 * @brief The post @p I steps down a chain.
 *
 * `PostAt<P, 0>::type` is @p P itself and `PostAt<P, P::key_arity>::type` is the innermost post,
 *  the set of targets. What @c mata::posts::DeltaBase::Key and @c mata::posts::DeltaBase::Reserved are
 *  spelled in terms of: a post has exactly one key so @c PostLike::Key is unambiguous and stays
 *  singular, but a *relation* spans every post, and there the same name would silently mean "the
 *  outermost one" — right at @c key_arity 1 and quietly wrong above it.
 */
template <typename P, size_t I> struct PostAt {
	static_assert(I <= arity_of<P>, "no post that deep in this chain");
	using type = typename PostAt<typename P::Nested, I - 1>::type;
};
/// @copydoc PostAt
template <typename P> struct PostAt<P, 0> {
	using type = P;
};

/// Key @p I's type, taken from the chain rather than from a relation. What
///  @c mata::posts::DeltaBase::Key is spelled in terms of, and what the per-arity keyed writes need in
///  their *signatures* — a member of the relation would not do there, because the parameter types
///  have to depend on the member's own template parameter to stay lazy at the wrong arity.
template <typename P, size_t I> using KeyOf = typename PostAt<P, I>::type::Key;

namespace detail {
template <typename P, size_t... Is>
auto keys_of(std::index_sequence<Is...>) -> std::tuple<KeyOf<P, Is>...>;
} // namespace mata::posts::detail.
/// The keys of a chain as one tuple type, outermost first: `std::tuple<KeyOf<P, 0>, ..., KeyOf<P, arity - 1>>`.
template <typename P> using KeysOf = decltype(detail::keys_of<P>(std::make_index_sequence<arity_of<P>>{}));
} // namespace mata::posts.

} // namespace mata.

#endif // MATA_CORE_TRAITS_HH
