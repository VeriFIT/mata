/** @file
 * @brief Transition relation of an NFA.
 *
 * @c Delta and its levels carry no NFA-specific semantics, so they live in @c mata
 *  (see @c mata/core/delta.hh) and are re-exported here. As with @c mata/nfa/types.hh, the
 *  re-export is the seam: it lets @c mata::nfa substitute its own transition relation later by
 *  editing these lines, with no change to any call site.
 */

#ifndef MATA_NFA_DELTA_HH
#define MATA_NFA_DELTA_HH

#include "mata/core/delta.hh"
#include "mata/nfa/types.hh"

namespace mata::nfa {

using Transition = mata::Transition;
using Move = mata::Move;
using SymbolPost = mata::SymbolPost;
using StatePost = mata::StatePost;
using SuccessorCursor = mata::SuccessorCursor;
using SynchronizedExistentialSymbolPostIterator = mata::SynchronizedExistentialSymbolPostIterator;
using Delta = mata::Delta;

/// @note A using-declaration rather than a wrapper, so that @c nfa::defragment and
///  @c mata::defragment name the same function and overload resolution stays unambiguous.
using mata::defragment;

} // namespace mata::nfa.

#endif // MATA_NFA_DELTA_HH
