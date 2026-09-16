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

#include <concepts>

#include "mata/relation.hh"
#include "mata/nft/types.hh"

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
 * Re-exported here rather than named as @c mata::Automaton at the class, because it is a *function
 *  of* @c Delta — `AutomatonBase<Delta>` — so substituting the relation on the line above has to
 *  carry the base with it. Naming @c mata::Automaton directly at `class Nft` would leave the
 *  base pointing at the depth-2 relation while @c Delta pointed somewhere else, which is the one
 *  way the seam can be bypassed without any call site looking wrong.
 *
 * @note This was exactly the hole: the layering grep in §8 of the Plan did not list @c Automaton, so
 *  `class Nft : public mata::Automaton` passed a check written to forbid it. The grep now lists it.
 */
using Automaton = mata::Automaton;

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
 * The relation and the alphabet beside it must agree on what a symbol is.
 *
 * @c Nft holds both a @c Delta and a `shared_ptr<Alphabet>`, and nothing has ever checked that
 *  the keys of the one are the symbols of the other — because both are @c mata::Symbol by
 *  construction and cannot disagree. This records the constraint while it is still free, so that
 *  making either side configurable is a compile error rather than a silent truncation: the alphabet
 *  hands out its @c Symbol, which converts implicitly to the key type whether or not it fits.
 */
static_assert(
	std::same_as<Delta::Key<0>, Alphabet::Symbol>,
	"nft::Delta's keys and mata::Alphabet's symbols must be one type."
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
