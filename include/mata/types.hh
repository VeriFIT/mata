/** @file
 * @brief The basic value types every automaton in Mata is built on.
 *
 * Concrete choices, not generic machinery: what a state is, what a symbol is, what a run holds.
 *  They sit here rather than in @c mata/core/ because @c core names none of them -- it reads every
 *  type off the relation it is instantiated over -- so putting them there would have made the
 *  layering claim "core names nothing concrete" false. Each module (@c mata::nfa, @c mata::nft,
 *  ...) re-exports the ones it uses; see @c mata/nfa/types.hh for why the re-export matters.
 */

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

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

using StateSet = utils::OrdVector<State>; ///< A set of states: never defined through a target type.

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

#endif // MATA_TYPES_HH
