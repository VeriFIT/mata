/** @file
 * @brief The depth-2 relation (state, symbol, StateSet) shipped with the library, and automata built on it.
 */

#ifndef MATA_RELATION_HH
#define MATA_RELATION_HH

#include <concepts>
#include <set>
#include <type_traits>
#include <utility>

#include "mata/alphabet.hh"
#include "mata/core/automaton.hh"
#include "mata/core/delta.hh"
#include "mata/types.hh"
#include "mata/utils/ord-vector.hh"

namespace mata {

/// A transition (source, symbol, target).
using Transition = posts::Transition<State, Symbol, State>;
/// A move out of the depth-2 relation: a symbol and one target state.
using Move = posts::Move<Symbol, State>;
/// The depth-2 entry: a symbol keying a set of target states.
using SymbolPost = posts::PostEntry<Symbol, StateSet>;
/// The depth-2 post: symbols keying sets of target states.
using StatePost = posts::Post<SymbolPost>;

/**
 * @brief The depth-2 relation: (state, symbol, StateSet).
 *
 * @note Exposes the generic relation as a concrete type, so that users are shielded from the template plumbing.
 */
class Delta : public posts::DeltaBase<StatePost, posts::SymbolTransitionTraits<StatePost>> {
  public:
	using Base = posts::DeltaBase<StatePost, posts::SymbolTransitionTraits<StatePost>>; ///< The generic relation.
	using Base::Base; ///< The constructors of the generic relation, including `explicit Delta(size_t)`.

	Delta() = default;
	Delta(const Delta&) = default;
	Delta(Delta&&) = default;
	Delta& operator=(const Delta&) = default;
	Delta& operator=(Delta&&) = default;
	~Delta() = default;

	/// @name Conversions from the generic relation
	/// Implicit, since a `posts::DeltaBase<StatePost>` is this relation.
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

	/// @name Keyed members in concrete types
	/// Forwarders, so that diagnostics and IDEs show `add(State, Symbol, State)`.
	/// Redeclaring a name hides every base overload of it, so every overload is listed here.
	///@{
	/**
	 * @brief Add a transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 */
	void add(const mata::State source, const mata::Symbol symbol, const mata::State target) {
		Base::add(source, symbol, target);
	}

	/**
	 * @brief Add a transition from @p source over @p symbol to every one of @p targets.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param targets Set of targets of the transition.
	 */
	void add(const mata::State source, const mata::Symbol symbol, const mata::StateSet& targets) {
		Base::add(source, symbol, targets);
	}

	/**
	 * @brief Add the transition @p transition.
	 *
	 * @param transition Transition to add.
	 */
	void add(const Transition& transition) { Base::add(transition); }

	/**
	 * @brief Remove the transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 * @throws Throws std::invalid_argument if @p source has no transition over @p symbol.
	 */
	void remove(const mata::State source, const mata::Symbol symbol, const mata::State target) {
		Base::remove(source, symbol, target);
	}

	/**
	 * @brief Remove the transition @p transition.
	 *
	 * @param transition Transition to remove.
	 * @throws Throws std::invalid_argument if the source of @p transition has no transition over its symbol.
	 */
	void remove(const Transition& transition) { Base::remove(transition); }

	/**
	 * @brief Check whether the relation contains the transition from @p source over @p symbol to @p target.
	 *
	 * @param source Source state of the transition.
	 * @param symbol Symbol of the transition.
	 * @param target Target of the transition.
	 * @return True if the relation contains the transition, false otherwise.
	 */
	bool contains(const mata::State source, const mata::Symbol symbol, const mata::State target) const {
		return Base::contains(source, symbol, target);
	}

	/**
	 * @brief Check whether the relation contains the transition @p transition.
	 *
	 * @param transition Transition to look for.
	 * @return True if the relation contains the transition, false otherwise.
	 */
	bool contains(const Transition& transition) const { return Base::contains(transition); }
	///@}

	/// @name Key members as symbols
	/// For this relation a key is a symbol, so the protected key members of the base are exported
	///  under the symbol names.
	///@{
	/**
	 * @brief Expand @p target_alphabet by the symbols used on the transitions.
	 *
	 * The already existing symbols of @p target_alphabet are not overwritten.
	 *
	 * @param target_alphabet Alphabet to expand.
	 */
	template <ExtensibleAlphabet A> requires std::same_as<typename A::Symbol, mata::Symbol>
	void add_symbols_to(A& target_alphabet) const {
		Base::add_keys_to(target_alphabet);
	}

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * They do not have to equal the symbols of the alphabet used by the automaton.
	 *
	 * @return The used symbols, ordered.
	 */
	utils::OrdVector<mata::Symbol> get_used_symbols() const { return Base::get_used_keys(); }

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * @return The used symbols, ordered.
	 */
	utils::OrdVector<mata::Symbol> get_used_symbols_vec() const { return Base::get_used_keys_vec(); }

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * @return The used symbols, as a @c std::set.
	 */
	std::set<mata::Symbol> get_used_symbols_set() const { return Base::get_used_keys_set(); }

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * @return The used symbols, as a sparse set.
	 */
	utils::SparseSet<mata::Symbol> get_used_symbols_sps() const { return Base::get_used_keys_sps(); }

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * @note Indexed by symbol, so it allocates up to the greatest symbol used, epsilons included.
	 *
	 * @return A bit vector, true at every used symbol.
	 */
	std::vector<bool> get_used_symbols_bv() const { return Base::get_used_keys_bv(); }

	/**
	 * @brief Get the symbols used on the transitions.
	 *
	 * @note Indexed by symbol, so it allocates up to the greatest symbol used, epsilons included.
	 *
	 * @return A @c BoolVector, true at every used symbol.
	 */
	BoolVector get_used_symbols_chv() const { return Base::get_used_keys_chv(); }

	/**
	 * @brief Get the greatest symbol used on the transitions, epsilons included.
	 *
	 * @return The greatest symbol used, or zero when there are no transitions.
	 */
	mata::Symbol get_max_symbol() const { return Base::get_max_key().value_or(0); }
	///@}
};

using SuccessorCursor = posts::SuccessorCursor<StatePost>; ///< The successor cursor of @c Delta.

/// The automaton every NFA and NFT derives from: @c mata::AutomatonBase over @c Delta.
using Automaton = AutomatonBase<Delta>;

/// @name Contract checks
/// @c Delta must satisfy the contract the generic algorithms are written against.
///@{
static_assert(posts::relation_contract_holds<Delta>());
static_assert(
	std::integral<Delta::Key<0>>,
	"the depth-2 relation is keyed by symbols, so the symbol forwarders and the indexed key members must exist."
);
/// @c mata::EPSILON, written at call sites, and the epsilon the relation's members default to must be one value.
static_assert(
	Delta::Reserved<0>::min_epsilon == EPSILON,
	"mata::EPSILON and the relation's own epsilon must be the same value."
);
/// The keyed members take a target state by value.
static_assert(
	std::is_same_v<Delta::TargetArg, const State>,
	"the shipped relation must take its targets by value; see mata::utils::ArgOf."
);
///@}

} // namespace mata.

namespace mata {
/// Instantiated once, in `src/relation.cc`, instead of in every translation unit including this header.
extern template class posts::detail::PostEntryCore<Symbol, StateSet, ReservedKeys<Symbol>>;
extern template class posts::PostEntry<Symbol, StateSet>;
extern template class posts::Post<SymbolPost>;
extern template class posts::PostMoves<StatePost>;
extern template class posts::KeyedAccess<posts::DeltaBase<StatePost>, StatePost>;
extern template class posts::DeltaBase<StatePost>;
extern template class posts::DeltaTransitions<posts::DeltaBase<StatePost>>;
extern template class AutomatonBase<Delta>;
} // namespace mata.

#endif // MATA_RELATION_HH
