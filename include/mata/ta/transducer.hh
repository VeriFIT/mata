/** @file
 * @brief Structure-preserving ranked bottom-up tree transducers.
 */

#ifndef MATA_TA_TRANSDUCER_HH
#define MATA_TA_TRANSDUCER_HH

#include "mata/ta/ta.hh"

#include <cstddef>

namespace mata::ta {

extern const std::string TYPE_TT;

/// A structure-preserving rule @c input(children) -> output : target.
struct TransducerTransition {
	Symbol input_symbol;
	std::vector<State> children;
	Symbol output_symbol;
	State target;

	TransducerTransition(Symbol input_symbol, std::vector<State> children, Symbol output_symbol, State target)
		: input_symbol{input_symbol},
		  children{std::move(children)},
		  output_symbol{output_symbol},
		  target{target} {}
	TransducerTransition(Symbol input_symbol, Symbol output_symbol, State target)
		: input_symbol{input_symbol},
		  children{},
		  output_symbol{output_symbol},
		  target{target} {}
	bool operator==(const TransducerTransition&) const = default;
};

/// Target-indexed transducer rules and their independent input and output ranked alphabets.
class TransducerDelta {
  public:
	using TransitionList = std::vector<TransducerTransition>;
	using TargetMap = std::unordered_map<State, TransitionList>;
	using const_iterator = TargetMap::const_iterator;

	explicit TransducerDelta(RankedAlphabet input_alphabet = {}, RankedAlphabet output_alphabet = {})
		: input_alphabet_{std::move(input_alphabet)},
		  output_alphabet_{std::move(output_alphabet)} {}

	/// Adds a rule after checking both ranks and the state sentinels. Returns false for an exact duplicate.
	bool add(TransducerTransition transition);
	bool add(Symbol input_symbol, std::vector<State> children, Symbol output_symbol, State target) {
		return add(TransducerTransition{input_symbol, std::move(children), output_symbol, target});
	}
	bool add(Symbol input_symbol, Symbol output_symbol, State target) {
		return add(TransducerTransition{input_symbol, output_symbol, target});
	}

	bool contains(const TransducerTransition& transition) const;
	bool empty() const { return size_ == 0; }
	size_t size() const { return size_; }
	void clear();

	const RankedAlphabet& input_alphabet() const { return input_alphabet_; }
	const RankedAlphabet& output_alphabet() const { return output_alphabet_; }
	const TargetMap& transitions() const { return transitions_; }
	const TransitionList& operator[](State target) const;

	const_iterator begin() const { return transitions_.begin(); }
	const_iterator end() const { return transitions_.end(); }

  private:
	RankedAlphabet input_alphabet_{};
	RankedAlphabet output_alphabet_{};
	TargetMap transitions_{};
	size_t size_{0};
};

/**
 * A nondeterministic structure-preserving ranked bottom-up tree transducer.
 *
 * Input and output symbols of every rule have the same rank as its ordered child-state list. Application starts at
 * nullary rules; a pair of an automaton state and a transducer state is final exactly when both components are final.
 * States are numbered from zero to @c num_of_states() - 1.
 */
class TreeTransducer {
  public:
	StateSet final{};
	TransducerDelta delta{};

	explicit TreeTransducer(RankedAlphabet input_alphabet = {}, RankedAlphabet output_alphabet = {})
		: delta{std::move(input_alphabet), std::move(output_alphabet)} {}
	explicit TreeTransducer(
		size_t num_of_states, RankedAlphabet input_alphabet = {}, RankedAlphabet output_alphabet = {}
	);
	explicit TreeTransducer(TransducerDelta delta, StateSet final = {}, size_t num_of_states = 0);

	State add_state();
	State add_state(State state);
	size_t num_of_states() const { return num_of_states_; }

	bool add_transition(TransducerTransition transition);
	bool add_transition(Symbol input_symbol, std::vector<State> children, Symbol output_symbol, State target) {
		return add_transition(TransducerTransition{input_symbol, std::move(children), output_symbol, target});
	}
	bool add_transition(Symbol input_symbol, Symbol output_symbol, State target) {
		return add_transition(TransducerTransition{input_symbol, output_symbol, target});
	}

	/// Checks state bounds and both ranked alphabets, throwing @c std::logic_error on a low-level inconsistency.
	void validate() const;

	/// Exchanges the input and output tracks without changing states, finality, or child order.
	TreeTransducer invert() const;

	/// Computes the output language of @p input and trims nonproductive and non-coaccessible product states.
	Ta apply(const Ta& input) const;

  private:
	size_t num_of_states_{0};
};

} // namespace mata::ta

#endif // MATA_TA_TRANSDUCER_HH
