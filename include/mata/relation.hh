/** @file
 * @brief The transition relation and automaton this library ships: depth 2, symbols keying states.
 *
 * @c mata/core/ is generic. It has the post templates, the walks, the cursor, the concepts and
 *  @c mata::AutomatonBase, and it names no particular relation anywhere — deliberately, because a
 *  third party building their own relation compiles all of it and should not be paying for, or
 *  tripping over, the one this library happens to ship.
 *
 * *This* is the one it ships. Every alias below is a single line, and together they are the whole of
 *  what "an NFA's transition relation" means here: a vector indexed by source state, over one key —
 *  the symbol — ending in a set of target states. @c mata::nfa and @c mata::nft both re-export these
 *  through their own seams, which is what lets either module substitute something else later by
 *  editing its own two lines rather than this file.
 *
 * Keeping it out of @c core/ is not tidiness. It is the difference between a core that *is* generic
 *  and a core that merely *contains* something generic: with the instantiation in here, the
 *  layering check "does core name a concrete relation" has an answer, and the answer is no.
 *
 * @note The explicit instantiations matching the @c extern template declarations at the bottom live
 *  in `src/relation.cc`. They are load-bearing, not an optimisation to be tidied away: without
 *  them every translation unit that includes this header instantiates the entire post stack and
 *  @c mata::AutomatonBase's ~400 lines of template bodies.
 */

#ifndef MATA_RELATION_HH
#define MATA_RELATION_HH

#include <utility>

#include "mata/alphabet.hh"
#include "mata/core/automaton.hh"
#include "mata/core/delta.hh"
#include "mata/core/types.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/synchronized-iterator.hh"

namespace mata {

/// A transition of the depth-2 relation: (source, symbol, target).
using Transition = posts::Transition<State, Symbol, State>;

/// A move out of the depth-2 relation: a symbol and one target state.
using Move = posts::Move<Symbol, State>;

/// The depth-2 entry: a symbol keying a set of target states. Every existing call site names this.
using SymbolPost = posts::PostEntry<Symbol, StateSet>;

/// The depth-2 post: symbols keying sets of target states. Every existing call site names this.
using StatePost = posts::Post<SymbolPost>;

/**
 * @brief The depth-2 relation: states, then symbols, then target states. Every call site names this.
 *
 * A **class**, not an alias, and the reason is diagnostics. An alias does not survive into compiler
 *  messages — GCC prints the canonical type — so `delta.add_transition(...)` on the alias reported
 *  `'class mata::posts::Delta<mata::posts::Post<mata::posts::PostEntry<unsigned int,
 *  mata::posts::StateTargets<long unsigned int> > > >' has no member named 'add_transition'`:
 *  124 characters of noun for a one-word mistake. Measured; the rest of the message was unchanged.
 *
 * It adds **nothing** — no members, no behaviour, no virtuals. That is what makes the conversions
 *  below safe: there is nothing to slice, so a base and a derived object differ only in the name the
 *  compiler prints. @c sizeof is asserted equal at the bottom of this file.
 *
 * @note The conversions from the base are not decoration. Generic code in @c core legitimately
 *  produces a `posts::Delta<StatePost>` — `posts::defragment()` returns one, and
 *  `src/nfa/nfa.cc` assigns it straight into an automaton's @c delta. Core cannot know about this
 *  class and must not, so the class has to accept what core hands back.
 * @see mata::Automaton, which is @c AutomatonBase over this.
 */
class Delta : public posts::Delta<StatePost> {
  public:
	using Base = posts::Delta<StatePost>;
	using Base::Base; ///< including `explicit Delta(size_t)`, which @c mata::DeltaLike requires

	Delta() = default;
	Delta(const Delta&) = default;
	Delta(Delta&&) = default;
	Delta& operator=(const Delta&) = default;
	Delta& operator=(Delta&&) = default;
	~Delta() = default;

	/// @name Accepting what core hands back
	/// Implicit on purpose: a `posts::Delta<StatePost>` *is* this relation, spelled the long way.
	///@{
	Delta(const Base& other) : Base{other} {}
	Delta(Base&& other) noexcept : Base{std::move(other)} {}
	Delta& operator=(const Base& other) {
		Base::operator=(other);
		return *this;
	}
	Delta& operator=(Base&& other) noexcept {
		Base::operator=(std::move(other));
		return *this;
	}
	///@}
};

/// The depth-2 successor cursor. Every existing call site names this.
using SuccessorCursor = posts::SuccessorCursor<StatePost>;

/**
 * @brief Specialization of utils::SynchronizedExistentialIterator for iterating over SymbolPosts.
 */
class SynchronizedExistentialSymbolPostIterator
	: public utils::SynchronizedExistentialIterator<utils::OrdVector<SymbolPost>::const_iterator> {
  public:
	/**
	 * @brief Get union of all targets.
	 */
	StateSet unify_targets() const;

	/**
	 * @brief Synchronize with the given SymbolPost @p sync.
	 *
	 * Alignes the synchronized iterator to the same symbol as @p sync.
	 * @return True iff the synchronized iterator points to the same symbol as @p sync.
	 */
	bool synchronize_with(const SymbolPost& sync);

	/**
	 * @brief Synchronize with the given symbol @p sync_symbol.
	 *
	 * Alignes the synchronized iterator to the same symbol as @p sync_symbol.
	 * @return True iff the synchronized iterator points to the same symbol as @p sync.
	 */
	bool synchronize_with(Symbol sync_symbol);
}; // class SynchronizedExistentialSymbolPostIterator.

/**
 * @brief The automaton every NFA and NFT in the tree derives from.
 *
 * @c mata::AutomatonBase over the relation above. Never named as a type outside inheritance
 *  plumbing — no variable, parameter or return type anywhere — which is why it is an alias here
 *  rather than a class: it is a *data-owning mixin*, and the Plan's §3.9 says why that is not
 *  dissolvable into free functions.
 */
using Automaton = AutomatonBase<Delta>;

/// @name Contract checks
/// The concrete relation must satisfy the contract the generic algorithms are written against.
/// A failure here means @c mata/core/concepts.hh and this file have drifted apart.
///@{
static_assert(PostEntryLike<SymbolPost>, "SymbolPost must be StatePost's entry type.");
static_assert(PostLike<StatePost>, "StatePost must be one post of the relation.");
static_assert(
	ReservedKeysAtTail<StatePost>,
	"the epsilon lookups walk back from the end of a StatePost, which needs the reserved keys to be "
	"the last ones."
);
static_assert(
	KeyDenotesSymbols<Delta::Key<0>>,
	"the depth-2 relation is keyed by symbols, so it must have the symbol members."
);
/**
 * The one place the relation's epsilon and the module constant meet.
 *
 * @c mata::EPSILON is what call sites write and @c Delta::Reserved<0>::epsilon is what the relation's
 *  own members default to; they are two spellings that have to denote one value. Give a relation a
 *  different reserved tail without updating the constant and every explicit `EPSILON` argument at a
 *  call site starts disagreeing with every defaulted one -- which is the silent wrong answer this
 *  whole descriptor exists to prevent, so it is a compile error instead. See the Plan, §3.8.
 */
static_assert(
	Delta::Reserved<0>::epsilon == EPSILON,
	"mata::EPSILON and the relation's own epsilon must be the same value."
);
static_assert(DeltaLike<Delta>, "Delta must satisfy the contract mata::Automaton is written against.");
/// Naming the relation must cost nothing. @c Delta adds no members, so the empty-base rules make it
///  the same size as what it derives from; if that ever stops holding, the class has grown something
///  and the conversions above have something to slice.
static_assert(
	sizeof(Delta) == sizeof(posts::Delta<StatePost>) && alignof(Delta) == alignof(posts::Delta<StatePost>),
	"mata::Delta exists only to shorten a name in diagnostics; it must add nothing."
);
static_assert(TargetSetLike<SymbolPost::Nested>, "The innermost post must be a set of targets.");
/// The cursor is hand-written per arity, 1 to 3. @see the Plan, T3.2 and §3.3b.
static_assert(
	Delta::key_arity <= 3,
	"mata::Delta is capped at key_arity 3 (structure depth 4), because SuccessorCursor is "
	"hand-written per arity and three is where that stops paying. Past it, add a specialisation."
);
//  as SymbolPost::Nested (T2.1); that is also what lets StatePost::key_arity be computed rather
//  than hardcoded.
///@}

} // namespace mata.

namespace mata {
/// Instantiated once, in `src/relation.cc`. Without these, every translation unit including this
///  header instantiates the whole post stack and the automaton on top of it.
extern template class posts::PostEntry<Symbol, StateSet>;
extern template class posts::Post<SymbolPost>;
extern template class posts::Delta<StatePost>;
extern template class AutomatonBase<Delta>;
} // namespace mata.

#endif // MATA_RELATION_HH
