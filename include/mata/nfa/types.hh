/** @file
 * @brief Basic types used in the @c mata::nfa module for NFAs.
 */

#ifndef MATA_NFA_TYPES_HH
#define MATA_NFA_TYPES_HH

#include <string>

#include "mata/core/types.hh"

namespace mata::nfa {

extern const std::string TYPE_NFA;

class Nfa; ///< A non-deterministic finite automaton.

using State = mata::State;
using StateSet = mata::StateSet;
using Run = mata::Run;
using StateRenaming = mata::StateRenaming;
using ParameterMap = mata::ParameterMap;
using Limits = mata::Limits;
using EpsilonClosureOpt = mata::EpsilonClosureOpt;
using ProductFinalStateCondition = mata::ProductFinalStateCondition;

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
using mata::EPSILON;

} // namespace mata::nfa.

#endif // MATA_NFA_TYPES_HH
