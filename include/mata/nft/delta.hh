/** @file
 * @brief Data structures representing the delta (transition function) of an NFT, mapping states and input symbols to
 *  sets of states where a single NFT transition comprises a sequence of (NFA) transitions in @c mata::nft::Delta.
 */

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include "mata/alphabet.hh"
#include "mata/nft/types.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"

#include "mata/nfa/delta.hh"

#include <iterator>

namespace mata::nft {

using Transition = nfa::Transition;

using Move = nfa::Move;

using SymbolPost = nfa::SymbolPost;

using StatePost = nfa::StatePost;

using SynchronizedExistentialSymbolPostIterator = nfa::SynchronizedExistentialSymbolPostIterator;

using Delta = nfa::Delta;

} // namespace mata::nft.

#endif
