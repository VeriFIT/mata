/** @file
 * @brief Transition relation of an NFT.
 *
 * A single NFT transition comprises a sequence of @c num_of_levels transitions in the underlying
 *  @c Delta, one per level (tape). @c Delta itself carries no NFT-specific semantics, so it lives
 *  in @c mata (see @c mata/core/delta.hh) and is re-exported here. As with @c mata/nft/types.hh,
 *  the re-export is the seam: it lets @c mata::nft substitute its own transition relation later by
 *  editing these lines, with no change to any call site.
 */

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include "mata/core/delta.hh"
#include "mata/nft/types.hh"

namespace mata::nft {

using Transition = mata::Transition;
using Move = mata::Move;
using SymbolPost = mata::SymbolPost;
using StatePost = mata::StatePost;
using SuccessorCursor = mata::SuccessorCursor;
using SynchronizedExistentialSymbolPostIterator = mata::SynchronizedExistentialSymbolPostIterator;
using Delta = mata::Delta;

/// @note A using-declaration rather than a wrapper, so that @c nft::defragment and
///  @c mata::defragment name the same function and overload resolution stays unambiguous.
using mata::defragment;

/**
 * @c EPSILON is re-exported by @c nft/types.hh as a *using-declaration*, and the relation's own
 *  epsilon comes from @c Delta::Reserved<0>. This is where the two are checked to agree.
 *
 * The re-export must stay a using-declaration: `constexpr Symbol EPSILON{mata::EPSILON}` creates a
 *  *distinct object*, and any translation unit with both `using namespace mata` and
 *  `using namespace mata::nft` then has two candidates for the name (8 errors in
 *  `src/applications/strings/replace.cc`, measured). So @c Delta::Reserved<0>::epsilon does not
 *  *become* the module constant -- it is tied to it here instead, which gets the single source of
 *  truth without the ambiguity. See the Plan, §3.8.
 */
static_assert(
	Delta::Reserved<0>::epsilon == EPSILON,
	"nft::EPSILON must be the epsilon the relation's own members default to."
);

/**
 * @note @c DONT_CARE is @c EPSILON-1 and is *not* currently reserved: @c Delta::Reserved<0> leaves
 *  @c max_ordinary at @c EPSILON-1, so @c StatePost::moves_symbols() still iterates over it, exactly
 *  as it did before there was a descriptor at all. Whether it should is an NFT semantics question
 *  (a @c DONT_CARE is a wildcard *symbol*, not an epsilon), and not one this seam decides.
 *
 * What the descriptor buys is that answering it is now one line here --
 *  `using Delta = mata::posts::RelationOf<mata::ReservedKeys<Symbol, EPSILON, DONT_CARE - 1>, StateSet>;`
 *  -- instead of a hunt for every default argument that had baked in whichever epsilon was in scope
 *  where it was written. A @c mata::posts::ReservedKeysLike descriptor stands in for the key at its
 *  own position, so the whole stack follows from that one line and none of the four aliases above
 *  has to be respelled. The static_assert above then forces the module constant to keep up.
 */

} // namespace mata::nft.

#endif // MATA_NFT_DELTA_HH
