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

} // namespace mata::nft.

#endif // MATA_NFT_DELTA_HH
