/** @file
 * @brief Ranked bottom-up tree automata.
 */

#ifndef MATA_TA_TA_HH
#define MATA_TA_TA_HH

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/utils/utils.hh"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mata::ta {

using State = nfa::State;
using StateSet = nfa::StateSet;
using Symbol = mata::Symbol;
using StateRenaming = nfa::StateRenaming;
using Limits = nfa::Limits;

extern const std::string TYPE_TA;

/// A ranked symbol table. Adding an existing symbol with a different arity is an error.
class RankedAlphabet {
  public:
	/// Returns false when the symbol and arity were already present.
	/// @throws std::invalid_argument If @p symbol already has a different arity.
	bool add(Symbol symbol, size_t arity);

	/// Calls @c add() for every pair.
	RankedAlphabet(std::initializer_list<std::pair<const Symbol, size_t>> symbols = {});

	std::optional<size_t> arity(Symbol symbol) const;
	bool is_compatible(Symbol symbol, size_t arity) const;
	bool empty() const { return arities_.empty(); }
	size_t size() const { return arities_.size(); }
	const std::unordered_map<Symbol, size_t>& arities() const { return arities_; }

  private:
	std::unordered_map<Symbol, size_t> arities_{};
};

/// A ground ranked tree. Child order and repetitions are significant.
struct Tree {
	Symbol symbol;
	std::vector<Tree> children;

	explicit Tree(Symbol symbol) : symbol{symbol}, children{} {}
	Tree(Symbol symbol, std::vector<Tree> children) : symbol{symbol}, children{std::move(children)} {}
	bool operator==(const Tree&) const = default;
};

/// An ordered hyperedge @c symbol(children) -> target. A nullary rule has no children.
struct Transition {
	Symbol symbol;
	std::vector<State> children;
	State target;

	Transition(Symbol symbol, std::vector<State> children, State target)
		: symbol{symbol},
		  children{std::move(children)},
		  target{target} {}
	Transition(Symbol symbol, State target) : symbol{symbol}, children{}, target{target} {}
	bool operator==(const Transition&) const = default;
};

/**
 * Target-indexed transition storage with its ranked alphabet.
 *
 * All modification goes through @c add(), which checks symbol arities and suppresses exact duplicate rules.
 * Read-only exposure of the target index keeps those invariants intact for consumers inspecting the low-level layout.
 */
class Delta {
  public:
	using TransitionList = std::vector<Transition>;
	using TargetMap = std::unordered_map<State, TransitionList>;
	using const_iterator = TargetMap::const_iterator;

	explicit Delta(RankedAlphabet alphabet = {}) : alphabet_{std::move(alphabet)} {}

	/// Adds a rule after checking its rank and the state sentinels. Returns false for an exact duplicate.
	bool add(Transition transition);
	bool add(Symbol symbol, std::vector<State> children, State target) {
		return add(Transition{symbol, std::move(children), target});
	}
	bool add(Symbol symbol, State target) { return add(Transition{symbol, target}); }

	bool contains(const Transition& transition) const;
	bool empty() const { return size_ == 0; }
	size_t size() const { return size_; }
	void clear();

	const RankedAlphabet& alphabet() const { return alphabet_; }
	const TargetMap& transitions() const { return transitions_; }
	const TransitionList& operator[](State target) const;
	State num_of_targets() const { return static_cast<State>(transitions_.size()); }

	const_iterator begin() const { return transitions_.begin(); }
	const_iterator end() const { return transitions_.end(); }

  private:
	RankedAlphabet alphabet_{};
	TargetMap transitions_{};
	size_t size_{0};
};

/**
 * A nondeterministic ranked bottom-up tree automaton.
 *
 * Nullary rules seed evaluation. A tree is accepted exactly when the set of states computed at its root intersects
 * @c final; there is no initial-state set. States are numbered from zero to @c num_of_states() - 1.
 */
class Ta {
  public:
	StateSet final{};
	Delta delta{};

	explicit Ta(RankedAlphabet alphabet = {}) : delta{std::move(alphabet)} {}
	explicit Ta(size_t num_of_states, RankedAlphabet alphabet = {});
	explicit Ta(Delta delta, StateSet final = {}, size_t num_of_states = 0);

	State add_state();
	State add_state(State state);
	size_t num_of_states() const { return num_of_states_; }

	bool add_transition(Transition transition);
	bool add_transition(Symbol symbol, std::vector<State> children, State target) {
		return add_transition(Transition{symbol, std::move(children), target});
	}
	bool add_transition(Symbol symbol, State target) { return add_transition(Transition{symbol, target}); }

	/// Checks state bounds and transition ranks, throwing @c std::logic_error on an inconsistent low-level object.
	void validate() const;

	/// Computes all states to which the root of @p tree can evaluate bottom-up.
	/// Unknown symbols and trees violating the ranked alphabet evaluate to the empty set.
	StateSet evaluate(const Tree& tree) const;
	bool accepts(const Tree& tree) const;

	/// Returns whether no tree is accepted. If non-null, @p witness receives an accepted tree when one exists.
	bool is_lang_empty(Tree* witness = nullptr) const;

	/// Two rules with the same symbol and complete ordered child-state tuple make the automaton nondeterministic.
	bool is_deterministic() const;

	/// Reachable subset construction over tuples of child macrostates.
	/// @param subset_map If non-null, receives each result state indexed by its nonempty input macrostate.
	Ta determinize(std::unordered_map<StateSet, State>* subset_map = nullptr) const;

	/// Retains only productive states reachable backwards from final states, then compacts their numbering.
	Ta& trim(StateRenaming* state_renaming = nullptr) &;
	Ta trim(StateRenaming* state_renaming = nullptr) && {
		trim(state_renaming);
		return std::move(*this);
	}

  private:
	size_t num_of_states_{0};
};

} // namespace mata::ta

#endif // MATA_TA_TA_HH
