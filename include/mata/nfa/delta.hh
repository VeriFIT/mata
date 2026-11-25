/** @file
 * @brief Transition relation of an NFA.
 */

#ifndef MATA_NFA_DELTA_HH
#define MATA_NFA_DELTA_HH

#include "mata/nfa/types.hh"
#include "mata/relation.hh"
#include "mata/synchronized-symbol-post-iterator.hh"

namespace mata::nfa {

using Transition = mata::Transition;
using Move = mata::Move;
using SymbolPost = mata::SymbolPost;
using StatePost = mata::StatePost;
using SuccessorCursor = mata::SuccessorCursor;
using SynchronizedExistentialSymbolPostIterator = mata::SynchronizedExistentialSymbolPostIterator;
using Delta = mata::Delta;
using DeltaBuilder = mata::DeltaBuilder;
using PostWorkspace = mata::PostWorkspace;

using Automaton = mata::Automaton; ///< The base every NFA derives from.

using mata::defragment;

} // namespace mata::nfa.

#endif // MATA_NFA_DELTA_HH
