/** @file
 * @brief Basic types shared by every automaton in Mata.
 *
 * These types carry no language semantics and belong to no particular automaton class, so they live
 *  in @c mata rather than in one of the automaton modules. Each module (@c mata::nfa, @c mata::nft,
 *  ...) re-exports the ones it uses; see @c mata/nfa/types.hh for why the re-export matters.
 */

#ifndef MATA_CORE_TYPES_HH
#define MATA_CORE_TYPES_HH

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/utils/ord-vector.hh"

namespace mata {

using State = unsigned long;
using StateSet = utils::OrdVector<State>;

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

#endif // MATA_CORE_TYPES_HH
