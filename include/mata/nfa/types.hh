/** @file
 * @brief Basic types used in the @c mata::nfa module for NFAs.
 */

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/alphabet.hh"

#include <cstdint>
#include <limits>

namespace mata::nfa {

extern const std::string TYPE_NFA;

using State = unsigned long;
using StateSet = utils::OrdVector<State>;

struct Run {
	Word word{}; ///< A finite-length word.
	std::vector<State> path{}; ///< A finite-length path through automaton.
};

/**
 * @brief Where an epsilon closure is applied around a transition step.
 *
 * The values are bit flags, so @c BeforeAndAfter must be tested with @c closes_before() and @c closes_after()
 *  rather than compared for equality. For @c 0 -ε-> 1 -a-> 2 -ε-> 3 and the step over @c a from @c {0}:
 *
 * | Option | Applied | Result |
 * |---|---|---|
 * | @c None | post(S) | @c {} |
 * | @c Before | post(closure(S)) | @c {2} |
 * | @c After | closure(post(S)) | @c {} |
 * | @c BeforeAndAfter | closure(post(closure(S))) | @c {2,3} |
 */
enum class EpsilonClosureOpt : std::uint8_t {
	None = 1 << 0, ///< No epsilon closure.
	Before = 1 << 1, ///< Epsilon closure before the transition.
	After = 1 << 2, ///< Epsilon closure after the transition.
	BeforeAndAfter = Before | After ///< Epsilon closure before and after the transition.
};

/// Does @p opt ask for an epsilon closure before the transition step?
constexpr bool closes_before(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::Before)) != 0;
}

/// Does @p opt ask for an epsilon closure after the transition step?
constexpr bool closes_after(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::After)) != 0;
}

enum class ProductFinalStateCondition : std::uint8_t {
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

class Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON{Limits::max_symbol};

} // namespace mata::nfa.

#endif // MATA_TYPES_HH
