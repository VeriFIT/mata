/** @file
 * @brief Transition relation of an NFT.
 */

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include "mata/nft/types.hh"
#include "mata/relation.hh"
#include "mata/synchronized-symbol-post-iterator.hh"

namespace mata::nft {

using Transition = mata::Transition;
using Move = mata::Move;
using SymbolPost = mata::SymbolPost;
using StatePost = mata::StatePost;
using SuccessorCursor = mata::SuccessorCursor;
using SynchronizedExistentialSymbolPostIterator = mata::SynchronizedExistentialSymbolPostIterator;
using Delta = mata::Delta;

using Automaton = mata::Automaton; ///< The base every NFT derives from.

using mata::defragment;

// TODO(nft): Decide whether DONT_CARE is a reserved key: https://github.com/VeriFIT/mata/issues/1143.

} // namespace mata::nft.

#endif // MATA_NFT_DELTA_HH
