/** @file
 * @brief Instantiations of the shipped relation and automaton templates.
 */

#include "mata/relation.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <queue>
#include <ranges>
#include <utility>

using namespace mata::utils;
using namespace mata;

/// @name Instantiations on (State, Symbol, StateSet) relation and automaton.
/// @{
template class mata::posts::detail::PostEntryCore<mata::Symbol, mata::StateSet, mata::ReservedKeys<mata::Symbol>>;
template class mata::posts::PostEntry<mata::Symbol, mata::StateSet>;
template class mata::posts::Post<mata::SymbolPost>;
template class mata::posts::PostMoves<mata::StatePost>;

template class mata::posts::KeyedAccess<mata::posts::DeltaBase<mata::StatePost>, mata::StatePost>;
template class mata::posts::DeltaBase<mata::StatePost>;
template class mata::posts::DeltaTransitions<mata::posts::DeltaBase<mata::StatePost>>;

template class mata::AutomatonBase<mata::Delta>;
/// @}
