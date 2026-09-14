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

/**
 * @c EPSILON is re-exported by @c nfa/types.hh as a *using-declaration*, and the relation's own
 *  epsilon comes from @c Delta::Reserved<0>. This is where the two are checked to agree.
 *
 * The re-export must stay a using-declaration: `constexpr Symbol EPSILON{mata::EPSILON}` creates a
 *  *distinct object*, and any translation unit with both `using namespace mata` and a
 *  `using namespace` of a module that redefined it then has two candidates for the name (8 errors
 *  in `src/applications/strings/replace.cc`, measured on the NFT side). So @c Delta::Reserved<0>::epsilon does not
 *  *become* the module constant -- it is tied to it here instead, which gets the single source of
 *  truth without the ambiguity. See the Plan, §3.8.
 */
static_assert(
	Delta::Reserved<0>::epsilon == EPSILON,
	"nfa::EPSILON must be the epsilon the relation's own members default to."
);

} // namespace mata::nfa.

#endif // MATA_NFA_DELTA_HH
