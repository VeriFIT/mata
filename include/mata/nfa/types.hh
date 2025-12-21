/** @file
 * @brief Basic types used in the @c mata::nfa module for NFAs.
 */

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"

#include <limits>

#include "types.hh"

namespace mata::nfa {

extern const std::string TYPE_NFA;

using State = unsigned long;
using StateSet = utils::OrdVector<State>;

struct Run {
    Word word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
};

enum class EpsilonClosureOpt : unsigned {
    None   = 1 << 0,   ///< No epsilon closure.
    Before = 1 << 1,   ///< Epsilon closure before the transition.
    After  = 1 << 2,    ///< Epsilon closure after the transition.
    BeforeAndAfter = Before | After ///< Epsilon closure before and after the transition.
};

enum class ProductFinalStateCondition {
    And, ///< Both original states have to be final.
    Or,  ///< At least one of the original states has to be final.
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
constexpr Symbol EPSILON{ Limits::max_symbol };

} // namespace mata::nfa.

#endif //MATA_TYPES_HH
