/** @file
 * @brief Transition relation of an NFA.
 */

#ifndef MATA_NFA_DELTA_HH
#define MATA_NFA_DELTA_HH

#include <concepts>

#include "mata/relation.hh"
#include "mata/synchronized-symbol-post-iterator.hh"
#include "mata/nfa/types.hh"

namespace mata::nfa {

using Transition = mata::Transition;
using Move = mata::Move;
using SymbolPost = mata::SymbolPost;
using StatePost = mata::StatePost;
using SuccessorCursor = mata::SuccessorCursor;
using SynchronizedExistentialSymbolPostIterator = mata::SynchronizedExistentialSymbolPostIterator;
using Delta = mata::Delta;
using DeltaBuilder = mata::DeltaBuilder;
using PostWorkspace = mata::PostWorkspace;

/**
 * @brief The base every NFA derives from.
 *
 * Re-exported here rather than named as @c mata::Automaton at the class.
 * Naming @c mata::Automaton directly at `class Nfa` would leave the base pointing 
 * at the depth-2 relation while @c Delta pointed somewhere else.
 */
using Automaton = mata::Automaton;

using mata::defragment;

/**
 * @c EPSILON is re-exported by @c nfa/types.hh as a *using-declaration*, and the relation's own
 *  epsilon comes from @c Delta::Reserved<0>. This is where the two are checked to agree.
 */
static_assert(
	Delta::Reserved<0>::min_epsilon == EPSILON,
	"nfa::EPSILON must be the epsilon the relation's own members default to."
);

/**
 * The relation and the alphabet beside it must agree on what a symbol is.
 *
 * @c Nfa holds both a @c Delta and a `shared_ptr<Alphabet>`, and nothing has ever checked that
 *  the keys of the one are the symbols of the other. This is where they are checked to agree.
 */
static_assert(
	std::same_as<Delta::Key<0>, Alphabet::Symbol>,
	"nfa::Delta's keys and mata::Alphabet's symbols must be one type."
);

} // namespace mata::nfa.

#endif // MATA_NFA_DELTA_HH
