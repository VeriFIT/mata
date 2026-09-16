/** @file
 * @brief The basic value types every automaton in Mata is built on.
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

/// @name Basic types used by the automata in Mata.
///@{
using State = unsigned long; ///< A state of an automaton.
using StateSet = utils::OrdVector<State>; ///< A set of automaton states/targets.
using Level = unsigned; ///< A level of a state in an automaton with a level structure (e.g. NFA, BDD, etc.).
using Symbol = unsigned; ///< A symbol used by an automaton.
using Word = std::vector<Symbol>; ///< A finite-length word over @ Symbol.
using WordName = std::vector<std::string>; ///< The same word, spelled with symbol *names*.

/**
 * @brief A finite length run of an automaton, consisting of states and a word.
 */
struct Run {
	Word word{}; ///< A finite-length word.
	std::vector<State> path{}; ///< A finite-length path through automaton.
};
///@}

// Mapping of states to states, used for renaming states in an automaton.
using StateRenaming = std::unordered_map<State, State>;

// Minimum and maximum values of the data types used for states and symbols.
struct Limits {
	static constexpr State min_state = std::numeric_limits<State>::min();
	static constexpr State max_state = std::numeric_limits<State>::max();
	static constexpr Symbol min_symbol = std::numeric_limits<Symbol>::min();
	static constexpr Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON{Limits::max_symbol};

/// @name Options for algorithms and functions in Mata.
///@{
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
///@}

} // namespace mata

#endif // MATA_TYPES_HH
