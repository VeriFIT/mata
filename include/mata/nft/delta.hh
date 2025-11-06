/** @file
 * @brief Data structures representing the delta (transition function) of an NFT, mapping states and input symbols to
 *  sets of states where a single NFT transition comprises a sequence of (NFA) transitions in @c mata::nft::Delta.
 */

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/alphabet.hh"
#include "mata/nft/types.hh"

#include "mata/nfa/delta.hh"

#include <iterator>

namespace mata::nft {

using Transition = mata::nfa::Transition;

using Move = mata::nfa::Move;

using SymbolPost = mata::nfa::SymbolPost;

using StatePost = mata::nfa::StatePost;

using SynchronizedExistentialSymbolPostIterator = mata::nfa::SynchronizedExistentialSymbolPostIterator;

using Delta = mata::nfa::Delta;

} // namespace mata::nft.

#endif //MATA_DELTA_HH
