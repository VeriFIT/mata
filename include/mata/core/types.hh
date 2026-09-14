/** @file
 * @brief Basic types shared by every automaton in Mata.
 *
 * These types carry no language semantics and belong to no particular automaton class, so they live
 *  in @c mata rather than in one of the automaton modules. Each module (@c mata::nfa, @c mata::nft,
 *  ...) re-exports the ones it uses; see @c mata/nfa/types.hh for why the re-export matters.
 */

#ifndef MATA_CORE_TYPES_HH
#define MATA_CORE_TYPES_HH

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "mata/utils/ord-vector.hh"

namespace mata {

using State = unsigned long;

/// @name The basic value types
///
/// Here rather than in @c mata/alphabet.hh, where they used to live, because they are what an
///  alphabet is *made of* rather than something it provides: a symbol is a value, and the alphabet
///  is the thing that translates names to and from it. With them here the dependency runs the way
///  it reads — @c alphabet.hh includes this file — and @c core stops depending on the alphabet
///  module for its own vocabulary.
///@{
using Symbol = unsigned;
using Level = unsigned;
using Word = std::vector<Symbol>; ///< A finite-length word over @c Symbol.
using WordName = std::vector<std::string>; ///< The same word, spelled with symbol *names*.
///@}

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

using StateSet = posts::StateTargets<State>;

struct Run {
	Word word{}; ///< A finite-length word.
	std::vector<State> path{}; ///< A finite-length path through automaton.
};

enum class EpsilonClosureOpt : unsigned {
	None = 1 << 0, ///< No epsilon closure.
	Before = 1 << 1, ///< Epsilon closure before the transition.
	After = 1 << 2, ///< Epsilon closure after the transition.
	BeforeAndAfter = Before | After ///< Epsilon closure before and after the transition.
};

enum class ProductFinalStateCondition {
	And, ///< Both original states have to be final.
	Or, ///< At least one of the original states has to be final.
};

using StateRenaming = std::unordered_map<State, State>;

/**
 * @brief Map of additional parameter name and value pairs.
 *
 * Used by certain functions for specifying some additional parameters in the following format:
 * ```cpp
 * ParameterMap {
 *     { "algorithm", "classical" },
 *     { "minimize", "true" }
 * }
 * ```
 */
using ParameterMap = std::unordered_map<std::string, std::string>;

struct Limits {
	static constexpr State min_state = std::numeric_limits<State>::min();
	static constexpr State max_state = std::numeric_limits<State>::max();
	static constexpr Symbol min_symbol = std::numeric_limits<Symbol>::min();
	static constexpr Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON{Limits::max_symbol};

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

#endif // MATA_CORE_TYPES_HH
