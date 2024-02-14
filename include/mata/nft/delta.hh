// TODO: Insert file header.

#ifndef MATA_NFT_DELTA_HH
#define MATA_NFT_DELTA_HH

#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/alphabet.hh"
#include "mata/nft/types.hh"

#include "mata/nfa/delta.hh"

#include <iterator>

namespace mata::nft {

/// A single transition in Delta represented as a triple(source, symbol, target).
using Transition = mata::nfa::Transition;

/**
 * Move from a @c StatePost for a single source state, represented as a pair of @c symbol and target state @c target.
 */
using Move = mata::nfa::Move;

/**
 * Structure represents a post of a single @c symbol: a set of target states in transitions.
 *
 * A set of @c SymbolPost, called @c StatePost, is describing the automata transitions from a single source state.
 */
using SymbolPost = mata::nfa::SymbolPost;

/**
 * @brief A data structure representing possible transitions over different symbols from a source state.
 *
 * It is an ordered vector containing possible @c SymbolPost (i.e., pair of symbol and target states).
 * @c SymbolPosts in the vector are ordered by symbols in @c SymbolPosts.
 */
using StatePost = mata::nfa::StatePost;


/**
 * @brief Specialization of utils::SynchronizedExistentialIterator for iterating over SymbolPosts.
 */
using SynchronizedExistentialSymbolPostIterator = mata::nfa::SynchronizedExistentialSymbolPostIterator;

/**
 * @brief Delta is a data structure for representing transition relation.
 *
 * Transition is represented as a triple Trans(source state, symbol, target state). Move is the part (symbol, target
 *  state), specified for a single source state.
 * Its underlying data structure is vector of StatePost classes. Each index to the vector corresponds to one source
 *  state, that is, a number for a certain state is an index to the vector of state posts.
 * Transition relation (delta) in Mata stores a set of transitions in a four-level hierarchical structure:
 *  Delta, StatePost, SymbolPost, and a set of target states.
 * A vector of 'StatePost's indexed by a source states on top, where the StatePost for a state 'q' (whose number is
 *  'q' and it is the index to the vector of 'StatePost's) stores a set of 'Move's from the source state 'q'.
 * Namely, 'StatePost' has a vector of 'SymbolPost's, where each 'SymbolPost' stores a symbol 'a' and a vector of
 *  target states of 'a'-moves from state 'q'. 'SymbolPost's are ordered by the symbol, target states are ordered by
 *  the state number.
 */
using Delta = mata::nfa::Delta;

} // namespace mata::nft.

#endif //MATA_DELTA_HH
