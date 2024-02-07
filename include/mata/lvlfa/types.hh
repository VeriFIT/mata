// TODO: Insert file header.

#ifndef MATA_LVLFA_TYPES_HH
#define MATA_LVLFA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"

#include "mata/nfa/types.hh"

#include <limits>


namespace mata::lvlfa {

extern const std::string TYPE_NFA;

using Level = unsigned;
using State = mata::nfa::State;
using StateSet = mata::nfa::StateSet;

using Run = mata::nfa::Run;

using StateRenaming = mata::nfa::StateRenaming;

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
using ParameterMap = mata::nfa::ParameterMap;

using Limits = mata::nfa::Limits;

struct Lvlfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = mata::nfa::EPSILON;
constexpr Symbol DONT_CARE = EPSILON - 1;

} // namespace mata::nfa.

#endif //MATA_TYPES_HH
