/** @file
 * @brief Transition relation of an NFT.
 */

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include <concepts>

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

/**
 * @brief The base every NFT derives from.
 *
 * Re-exported here rather than named as @c mata::Automaton at the class.
 * Naming @c mata::Automaton directly at `class Nft` would leave the base pointing
 * at the depth-2 relation while @c Delta pointed somewhere else.
 */
using Automaton = mata::Automaton;

using mata::defragment;

/**
 * @c EPSILON is re-exported by @c nft/types.hh as a *using-declaration*, and the relation's own
 *  epsilon comes from @c Delta::Reserved<0>. This is where the two are checked to agree.
 */
static_assert(
	Delta::Reserved<0>::min_epsilon == EPSILON,
	"nft::EPSILON must be the epsilon the relation's own members default to."
);

/**
 * The relation and the alphabet beside it must agree on what a symbol is.
 *
 * @c Nft holds both a @c Delta and a `shared_ptr<Alphabet>`, and nothing has ever checked that
 *  the keys of the one are the symbols of the other. This is where they are checked to agree.
 */
static_assert(
	std::same_as<Delta::Key<0>, Alphabet::Symbol>, "nft::Delta's keys and mata::Alphabet's symbols must be one type."
);

// TODO: @c DONT_CARE is @c EPSILON-1 and is not reserved: to the relation it is an ordinary symbol.
//  Whether to reserve it is an NFT semantics decision. Doing so means keying the relation by
//  `mata::ReservedKeys<Symbol, EPSILON, DONT_CARE - 1>` instead of plain @c Symbol.

} // namespace mata::nft.

#endif // MATA_NFT_DELTA_HH
