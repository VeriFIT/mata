/** @file
 * @brief Structure-preserving ranked bottom-up tree transducer implementation.
 */

#include "mata/ta/transducer.hh"

#include "product.hh"

#include <algorithm>
#include <stdexcept>

namespace mata::ta {

const std::string TYPE_TT{"TT-explicit"};

bool TransducerDelta::add(TransducerTransition transition) {
	nfa::num_of_states_for(transition.target);
	for (const State child : transition.children) { nfa::num_of_states_for(child); }
	const size_t arity{transition.children.size()};
	if (!input_alphabet_.is_compatible(transition.input_symbol, arity) ||
		!output_alphabet_.is_compatible(transition.output_symbol, arity)) {
		throw std::invalid_argument("tree transducer rule rank disagrees with its ranked alphabet");
	}
	input_alphabet_.add(transition.input_symbol, arity);
	output_alphabet_.add(transition.output_symbol, arity);
	TransitionList& transitions{transitions_[transition.target]};
	if (std::find(transitions.begin(), transitions.end(), transition) != transitions.end()) { return false; }
	transitions.push_back(std::move(transition));
	++size_;
	return true;
}

bool TransducerDelta::contains(const TransducerTransition& transition) const {
	const auto target_it{transitions_.find(transition.target)};
	if (target_it == transitions_.end()) { return false; }
	return std::find(target_it->second.begin(), target_it->second.end(), transition) != target_it->second.end();
}

void TransducerDelta::clear() {
	input_alphabet_ = {};
	output_alphabet_ = {};
	transitions_.clear();
	size_ = 0;
}

const TransducerDelta::TransitionList& TransducerDelta::operator[](const State target) const {
	static const TransitionList empty{};
	const auto it{transitions_.find(target)};
	return it == transitions_.end() ? empty : it->second;
}

TreeTransducer::TreeTransducer(
	const size_t num_of_states, RankedAlphabet input_alphabet, RankedAlphabet output_alphabet
)
	: delta{std::move(input_alphabet), std::move(output_alphabet)} {
	nfa::check_num_of_states(num_of_states);
	num_of_states_ = num_of_states;
}

TreeTransducer::TreeTransducer(TransducerDelta delta, StateSet final, const size_t num_of_states)
	: final{std::move(final)},
	  delta{std::move(delta)},
	  num_of_states_{num_of_states} {
	for (const auto& [target, transitions] : this->delta) {
		num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(target));
		for (const TransducerTransition& transition : transitions) {
			for (const State child : transition.children) {
				num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(child));
			}
		}
	}
	for (const State state : this->final) { num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(state)); }
}

State TreeTransducer::add_state() {
	const State state{nfa::to_state(num_of_states_)};
	++num_of_states_;
	return state;
}

State TreeTransducer::add_state(const State state) {
	num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(state));
	return state;
}

bool TreeTransducer::add_transition(TransducerTransition transition) {
	add_state(transition.target);
	for (const State child : transition.children) { add_state(child); }
	return delta.add(std::move(transition));
}

void TreeTransducer::validate() const {
	for (const State state : final) {
		if (state >= num_of_states_) { throw std::logic_error("tree transducer final state is outside the state set"); }
	}
	for (const auto& [target, transitions] : delta) {
		if (target >= num_of_states_) { throw std::logic_error("tree transducer target is outside the state set"); }
		for (const TransducerTransition& transition : transitions) {
			if (transition.target != target ||
				!delta.input_alphabet().is_compatible(transition.input_symbol, transition.children.size()) ||
				!delta.output_alphabet().is_compatible(transition.output_symbol, transition.children.size())) {
				throw std::logic_error("tree transducer target index or ranked alphabet is inconsistent");
			}
			for (const State child : transition.children) {
				if (child >= num_of_states_) {
					throw std::logic_error("tree transducer child is outside the state set");
				}
			}
		}
	}
}

TreeTransducer TreeTransducer::invert() const {
	validate();
	TreeTransducer result{num_of_states_, delta.output_alphabet(), delta.input_alphabet()};
	result.final = final;
	for (const auto& [target, transitions] : delta) {
		for (const TransducerTransition& transition : transitions) {
			result.add_transition(transition.output_symbol, transition.children, transition.input_symbol, target);
		}
	}
	return result;
}

Ta TreeTransducer::apply(const Ta& input) const {
	validate();
	input.validate();
	const std::vector<detail::ProductRule> input_rules{detail::product_rules(input)};
	std::vector<detail::ProductRule> transducer_rules{};
	transducer_rules.reserve(delta.size());
	for (const auto& [target, transitions] : delta) {
		for (const TransducerTransition& transition : transitions) {
			transducer_rules.push_back(
				detail::ProductRule{
					transition.input_symbol, transition.output_symbol, transition.children, transition.target
				}
			);
		}
	}

	Ta result{delta.output_alphabet()};
	detail::bottom_up_product(
		input_rules, transducer_rules,
		[&](const State product, const State input_state, const State transducer_state) {
			result.add_state(product);
			if (input.final.contains(input_state) && final.contains(transducer_state)) { result.final.insert(product); }
		},
		[&](const detail::ProductRule&, const detail::ProductRule& transducer, const std::span<const State> children,
			const State target) {
			result.add_transition(
				transducer.output_symbol, std::vector<State>{children.begin(), children.end()}, target
			);
		}
	);
	return std::move(result).trim();
}

} // namespace mata::ta
