/** @file
 * @brief Basic types used in the @c mata::nfa module for NFAs.
 */

#ifndef MATA_NFA_TYPES_HH
#define MATA_NFA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/config.hh"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "mata/types.hh"

namespace mata::nfa {

extern const std::string TYPE_NFA;

/// A Nondeterministic Finite Automaton.
class Nfa;

using State = mata::State;
using StateSet = mata::StateSet;
using SmallStateSet = mata::SmallStateSet;
using Run = mata::Run;
using StateRenaming = mata::StateRenaming;
using ParameterMap = mata::ParameterMap;
using Limits = mata::Limits;
using EpsilonClosureOpt = mata::EpsilonClosureOpt;
using ProductFinalStateCondition = mata::ProductFinalStateCondition;
using mata::closes_after;
using mata::closes_before;
using mata::throw_state_overflow;
using mata::check_num_of_states;
using mata::to_state;
using mata::num_of_states_for;

/// What @c Nfa::print_to_regex() optimizes for.
enum class RegexPrintMode : std::uint8_t {
	Fast, ///< The conversion time: a regex for a program to read.
	Readable, ///< The length and the legibility of the regex: a regex for a human to read. Takes longer.
};

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
using mata::EPSILON;

} // namespace mata::nfa.

#endif // MATA_NFA_TYPES_HH
