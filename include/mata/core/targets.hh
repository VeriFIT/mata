/** @file
 * @brief The innermost post: the targets reachable once every key has been supplied.
 *
 * Generic, so it belongs in @c core: @c mata::AutomatonBase spells its own @c StateSet over this
 *  template, and never over a concrete one. The concrete @c mata::StateSet alias lives in
 *  @c mata/types.hh with the other value types.
 */

#ifndef MATA_CORE_TARGETS_HH
#define MATA_CORE_TARGETS_HH

#include <algorithm>
#include <cstddef>

#include "mata/utils/ord-vector.hh"

namespace mata {

namespace posts {

/**
 * @brief The innermost post: the targets reachable once every key has been supplied.
 *
 * @c utils::OrdVector with the post protocol added on top — @c Target, a @c key_arity of zero
 *  (nothing left to key by), and the sortedness advertisement. Deriving rather than aliasing is what
 *  lets @c mata::StatePost compute its own @c key_arity as `Nested::key_arity + 1` instead of
 *  hardcoding it, and it is where a payload target will hang: specialise
 *  @c mata::TargetTraits for the payload and instantiate this over it.
 *
 * @see mata::TargetSetLike, and @ref nesting for how the posts stack.
 */
template <typename S> class StateTargets : public utils::OrdVector<S> {
	using super = utils::OrdVector<S>;

  public:
	/// @name Post protocol
	/// @see mata::TargetSetLike.
	///@{
	using Target = S; ///< What a successor walk yields. The innermost post decides this.
	static constexpr size_t key_arity{0}; ///< Nothing below here is keyed.
	/// @see @ref sortedness. Sorted by target, which is what @c OrdVector maintains.
	static constexpr bool sorted_by_target{true};
	/// @c OrdVector keeps its own @c is_sorted() private as an assertion helper, so check the range
	///  directly — the same workaround @c mata::StatePost uses.
	bool is_sorted() const { return std::ranges::is_sorted(*this); }
	///@}

	using super::OrdVector; ///< Inherit the base's constructors; copy and move are declared below.
	StateTargets() = default;
	StateTargets(const StateTargets&) = default;
	StateTargets(StateTargets&&) noexcept = default;
	StateTargets& operator=(const StateTargets&) = default;
	StateTargets& operator=(StateTargets&&) noexcept = default;

	/// Base to derived, and deliberately not @c explicit: a handful of sites outside @c core/ build a
	///  bare @c utils::OrdVector<State> and use it where a @c StateSet is expected — as a subset-map
	///  key in determinisation, for instance. This keeps them compiling untouched (invariant 2).
	StateTargets(const super& other) : super{other} {}
	StateTargets(super&& other) noexcept : super{std::move(other)} {}
};

} // namespace mata::posts.

} // namespace mata.

/**
 * @brief Hash for the innermost post.
 *
 * @c std::hash is specialised for @c mata::utils::OrdVector by *exact* type, so a derived post gets
 *  no hash from it — and `std::unordered_map<StateSet, ...>` appears in public headers
 *  (@c mata/nfa/nfa.hh, @c mata/nfa/plumbing.hh, and the NFT equivalents). Delegates to the base's.
 */
template <typename S> struct std::hash<mata::posts::StateTargets<S>> {
	std::size_t operator()(const mata::posts::StateTargets<S>& targets) const {
		return std::hash<mata::utils::OrdVector<S>>{}(targets);
	}
};

#endif // MATA_CORE_TARGETS_HH
