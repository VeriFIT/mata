/** @file
 * @brief The depth-2 relation (state, symbol, StateSet) shipped with the library, and automata built on it.
 */

#ifndef MATA_RELATION_HH
#define MATA_RELATION_HH

#include <concepts>
#include <set>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/core/automaton.hh"
#include "mata/core/delta.hh"
#include "mata/types.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"

namespace mata {

/// A transition (source, symbol, target).
using Transition = posts::Transition<State, Symbol, State>;
/// A move out of the depth-2 relation: a symbol and one target state.
using Move = posts::Move<Symbol, State>;
/// The depth-2 entry: a symbol keying a set of target states.
using SymbolPost = posts::PostEntry<Symbol, SmallStateSet>;
/// The depth-2 post: symbols keying sets of target states.
using StatePost = posts::Post<SymbolPost>;

/// Reusable buffers of @c Delta::post() and @c Delta::epsilon_closure(), so that a loop of steps allocates once.
struct PostWorkspace {
	std::vector<State> sources{}; ///< Epsilon closure of the source states of a step.
	std::vector<State> gathered{}; ///< Targets of a step, or the closure being built.
	std::vector<State> worklist{}; ///< Closure states whose epsilon posts are still unread.
	std::vector<State> visited{}; ///< Open-addressed hash table of the states of the closure being built.
};

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

	/// Installs its finished posts in one move.
	friend class DeltaBuilder;

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
		add(source, symbol, std::span<const mata::State>{targets.begin(), targets.end()});
	}

	/**
	 * @brief Add a transition from @p source over @p symbol to every one of the sorted, duplicate-free @p targets.
	 *
	 * One sorted merge into the targets of @p symbol, instead of a search and a shift per target.
	 */
	void add(mata::State source, mata::Symbol symbol, std::span<const mata::State> targets);

	/// Add every transition of @p symbol_post from @p source, merging it into an existing post of its symbol.
	void add(mata::State source, SymbolPost&& symbol_post);

	/**
	 * @brief Add all @p transitions at once.
	 *
	 * A relation without transitions is built anew in linear time (bucketed by source); otherwise the transitions are
	 *  sorted and added one (source, symbol) run at a time.
	 */
	void add(std::vector<Transition>&& transitions);

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
	bool remove(const mata::State source, const mata::Symbol symbol, const mata::State target) {
		if (source >= state_posts_.size()) { return false; }
		StatePost& state_post{state_posts_[source]};
		if (state_post.empty() || state_post.back().symbol < symbol) { return false; }
		const auto symbol_post_it{state_post.find(symbol)};
		if (symbol_post_it == state_post.end() || !symbol_post_it->erase(target)) { return false; }
		// Erase through the iterator already found instead of searching for the symbol post again.
		if (symbol_post_it->empty()) { state_post.erase(symbol_post_it); }
		return true;
	}

	/**
	 * @brief Remove the transition @p transition.
	 *
	 * @param transition Transition to remove.
	 * @throws Throws std::invalid_argument if the source of @p transition has no transition over its symbol.
	 */
	bool remove(const Transition& transition) { return remove(transition.source, transition.symbol, transition.target); }

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
	/**
	 * @brief Append the state posts of @p other with every state shifted by @p offset.
	 *
	 * The states of @c this below @p offset are kept (and added as empty when missing); @p other's state @c q becomes
	 *  @c q + @p offset. Throws @c std::overflow_error when the shifted states do not fit in @c State.
	 */
	Delta& append_shifted(const Delta& other, mata::State offset);
	/// @copydoc append_shifted(const Delta&, mata::State)
	Delta& append_shifted(Delta&& other, mata::State offset);

	/**
	 * @brief The reverted relation over (at least) @p num_of_states states.
	 *
	 * A counting sort over the predecessor lists: linear in the transitions, plus sorting each state's incoming
	 *  (symbol, source) pairs.
	 */
	[[nodiscard]] Delta reverted(size_t num_of_states) const;

	/// The epsilon closure of @p states over @p epsilons, using the buffers of @p workspace.
	StateSet
		epsilon_closure(const StateSet& states, const std::vector<mata::Symbol>& epsilons, PostWorkspace& workspace) const;

	/**
	 * @brief The successors of @p states over @p symbol, with the epsilon closures of @p epsilon_closure_opt over
	 *  @p epsilons, using the buffers of @p workspace.
	 */
	StateSet post(
		const StateSet& states,
		mata::Symbol symbol,
		EpsilonClosureOpt epsilon_closure_opt,
		const std::vector<mata::Symbol>& epsilons,
		PostWorkspace& workspace
	) const;

	/// @c post() that hands the gathered buffer of @p workspace to the result instead of copying it.
	StateSet post_adopt_buffer(
		const StateSet& states,
		mata::Symbol symbol,
		EpsilonClosureOpt epsilon_closure_opt,
		const std::vector<mata::Symbol>& epsilons,
		PostWorkspace& workspace
	) const;

	using Base::get_successors;
	/// The successors of @p state over @p symbol, with the epsilon closures of @p epsilon_closure_opt.
	StateSet get_successors(mata::State state, mata::Symbol symbol, EpsilonClosureOpt epsilon_closure_opt) const;

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

/**
 * @brief Builds a @c Delta in its final form from transitions given in order: sources, then symbols of a source, then
 *  targets of a symbol, each strictly increasing.
 *
 * Every symbol post is created once with exact capacity, instead of being grown target by target.
 */
class DeltaBuilder {
  public:
	DeltaBuilder() = default;
	explicit DeltaBuilder(const size_t num_of_states_hint) { state_posts_.reserve(num_of_states_hint); }

	void begin_state(State source) {
		MATA_ASSERT(!in_state_ && "begin_state() called before finish_state()");
		MATA_ASSERT((state_posts_.empty() || source >= state_posts_.size()) && "sources must be strictly increasing");
		state_posts_.resize(static_cast<size_t>(source) + 1);
		source_ = source;
		in_state_ = true;
		has_symbol_ = false;
	}

	void reserve_symbols(const size_t num_of_symbols) {
		MATA_ASSERT(in_state_ && "reserve_symbols() called outside of a state");
		state_posts_[source_].reserve(num_of_symbols);
	}

	void begin_symbol(const Symbol symbol) {
		MATA_ASSERT(in_state_ && "begin_symbol() called outside of a state");
		MATA_ASSERT(!in_symbol_ && "begin_symbol() called before finish_symbol()");
		MATA_ASSERT((!has_symbol_ || symbol > symbol_) && "symbols must be strictly increasing");
		symbol_ = symbol;
		targets_.clear();
		in_symbol_ = true;
	}

	void push_sorted_target(const State target) {
		MATA_ASSERT(in_symbol_ && "push_sorted_target() called outside of a symbol");
		MATA_ASSERT((targets_.empty() || target > targets_.back()) && "targets must be strictly increasing");
		targets_.push_back(target);
		max_target_ = std::max(max_target_, target);
	}

	void finish_symbol() {
		MATA_ASSERT(in_symbol_ && "finish_symbol() without begin_symbol()");
		in_symbol_ = false;
		if (targets_.empty()) { return; }
		has_symbol_ = true;
		state_posts_[source_].emplace_back(symbol_, std::span<const State>{targets_});
	}

	void finish_state() {
		MATA_ASSERT(in_state_ && "finish_state() without begin_state()");
		MATA_ASSERT(!in_symbol_ && "finish_state() called before finish_symbol()");
		in_state_ = false;
	}

	Delta finish() {
		MATA_ASSERT(!in_state_ && !in_symbol_ && "finish() called inside an unfinished state or symbol");
		// A target state needs its own (possibly empty) post, like every other state of the relation.
		if (!state_posts_.empty() && max_target_ >= state_posts_.size()) {
			state_posts_.resize(num_of_states_for(max_target_));
		}
		Delta delta{};
		delta.state_posts_ = std::move(state_posts_);
		return delta;
	}

  private:
	std::vector<StatePost> state_posts_{};
	std::vector<State> targets_{};
	Symbol symbol_{0};
	State source_{0};
	State max_target_{0};
	bool in_state_{false};
	bool in_symbol_{false};
	bool has_symbol_{false}; ///< Has the current state emitted a symbol post already?
}; // class DeltaBuilder.

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
extern template class posts::detail::PostEntryCore<Symbol, SmallStateSet, ReservedKeys<Symbol>>;
extern template class posts::PostEntry<Symbol, SmallStateSet>;
extern template class posts::Post<SymbolPost>;
extern template class posts::PostMoves<StatePost>;
extern template class posts::KeyedAccess<posts::DeltaBase<StatePost>, StatePost>;
extern template class posts::DeltaBase<StatePost>;
extern template class posts::DeltaTransitions<posts::DeltaBase<StatePost>>;
extern template class AutomatonBase<Delta>;
} // namespace mata.

#endif // MATA_RELATION_HH
