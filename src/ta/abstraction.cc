/** @file
 * @brief Residual predicate and bounded-context abstractions of ranked tree automata.
 */

#include "mata/ta/abstraction.hh"

#include "product.hh"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace mata::ta {
namespace {

Ta quotient_by_blocks(const Ta& aut, const std::vector<State>& block, const size_t num_of_blocks) {
	if (aut.num_of_states() == 0) { return Ta{aut.delta.alphabet()}; }
	Ta quotient{num_of_blocks, aut.delta.alphabet()};
	for (State state{0}; state < aut.num_of_states(); ++state) {
		if (aut.final.contains(state)) { quotient.final.insert(block[state]); }
	}
	for (const auto& [target, transitions] : aut.delta) {
		for (const Transition& transition : transitions) {
			std::vector<State> children{};
			children.reserve(transition.children.size());
			for (const State child : transition.children) { children.push_back(block[child]); }
			quotient.add_transition(transition.symbol, std::move(children), block[target]);
		}
	}
	return std::move(quotient).trim();
}

} // namespace

Ta intersection(const Ta& lhs, const Ta& rhs) {
	lhs.validate();
	rhs.validate();
	const std::vector<detail::ProductRule> left_rules{detail::product_rules(lhs)};
	const std::vector<detail::ProductRule> right_rules{detail::product_rules(rhs)};
	Ta result{lhs.delta.alphabet()};
	detail::bottom_up_product(
		left_rules, right_rules,
		[&](const State product, const State left, const State right) {
			result.add_state(product);
			if (lhs.final.contains(left) && rhs.final.contains(right)) { result.final.insert(product); }
		},
		[&](const detail::ProductRule& left, const detail::ProductRule&, const std::span<const State> children,
			const State target) {
			result.add_transition(left.input_symbol, std::vector<State>{children.begin(), children.end()}, target);
		}
	);
	return std::move(result).trim();
}

Ta collapse_by_predicates(
	const Ta& subject, const std::initializer_list<const Ta*> predicates, const bool whole_languages
) {
	return collapse_by_predicates(
		subject, std::span<const Ta* const>{predicates.begin(), predicates.size()}, whole_languages
	);
}

Ta collapse_by_predicates(const Ta& subject, const std::vector<Ta>& predicates, const bool whole_languages) {
	std::vector<const Ta*> view{};
	view.reserve(predicates.size());
	for (const Ta& predicate : predicates) { view.push_back(&predicate); }
	return collapse_by_predicates(subject, std::span<const Ta* const>{view.data(), view.size()}, whole_languages);
}

Ta collapse_by_predicates(const Ta& subject, const std::span<const Ta* const> predicates, const bool whole_languages) {
	for (const Ta* predicate : predicates) {
		if (predicate == nullptr) { throw std::invalid_argument("collapse_by_predicates: null predicate"); }
		predicate->validate();
	}

	Ta input{subject};
	input.trim();
	Ta dfa{input.determinize()};
	dfa.trim();
	if (dfa.num_of_states() == 0) { return Ta{subject.delta.alphabet()}; }

	const std::vector<detail::ProductRule> subject_rules{detail::product_rules(dfa)};
	using Fingerprint = std::vector<size_t>;
	std::vector<Fingerprint> fingerprints(dfa.num_of_states());
	size_t predicate_state_offset{0};
	for (const Ta* predicate : predicates) {
		const size_t num_predicate_states{predicate->num_of_states()};
		if (num_predicate_states > std::numeric_limits<size_t>::max() - predicate_state_offset) {
			throw std::overflow_error("collapse_by_predicates: predicate state index overflow");
		}
		if (num_predicate_states != 0) {
			const std::vector<detail::ProductRule> predicate_rules{detail::product_rules(*predicate)};
			detail::bottom_up_product(
				subject_rules, predicate_rules,
				[&](const State, const State subject_state, const State predicate_state) {
					if (!whole_languages || predicate->final.contains(predicate_state)) {
						fingerprints[subject_state].push_back(predicate_state_offset + predicate_state);
					}
				},
				[](const detail::ProductRule&, const detail::ProductRule&, std::span<const State>, State) {}
			);
		}
		predicate_state_offset += num_predicate_states;
	}

	std::map<Fingerprint, State> renumbering{};
	std::vector<State> block(dfa.num_of_states());
	for (State state{0}; state < dfa.num_of_states(); ++state) {
		Fingerprint& fingerprint{fingerprints[state]};
		std::ranges::sort(fingerprint);
		fingerprint.erase(std::ranges::unique(fingerprint).begin(), fingerprint.end());
		const State next_block{nfa::to_state(renumbering.size())};
		block[state] = renumbering.try_emplace(std::move(fingerprint), next_block).first->second;
	}
	return quotient_by_blocks(dfa, block, renumbering.size());
}

Ta collapse_by_height(const Ta& subject, const size_t height) {
	Ta dfa{subject.determinize()};
	dfa.trim();
	const size_t num_of_states{dfa.num_of_states()};
	if (num_of_states == 0) { return Ta{subject.delta.alphabet()}; }

	struct ContextRule {
		const Transition* transition;
		size_t position;
	};
	std::vector<std::vector<ContextRule>> contexts_by_child(num_of_states);
	for (const auto& [target, transitions] : dfa.delta) {
		for (const Transition& transition : transitions) {
			for (size_t position{0}; position < transition.children.size(); ++position) {
				contexts_by_child[transition.children[position]].push_back(ContextRule{&transition, position});
			}
		}
	}

	std::vector<State> block(num_of_states, 0);
	bool has_final{false};
	bool has_nonfinal{false};
	for (State state{0}; state < num_of_states; ++state) {
		if (dfa.final.contains(state)) {
			block[state] = 1;
			has_final = true;
		} else {
			has_nonfinal = true;
		}
	}
	if (!has_final) {
		std::ranges::fill(block, 0);
	} else if (!has_nonfinal) {
		std::ranges::fill(block, 0);
	}
	size_t num_of_blocks{has_final && has_nonfinal ? 2U : 1U};

	using Context = std::tuple<Symbol, size_t, std::vector<State>, State>;
	using Signature = std::pair<State, std::vector<Context>>;
	for (size_t round{0}; round < height; ++round) {
		std::map<Signature, State> renumbering{};
		std::vector<State> refined(num_of_states);
		for (State state{0}; state < num_of_states; ++state) {
			std::vector<Context> contexts{};
			contexts.reserve(contexts_by_child[state].size());
			for (const ContextRule context_rule : contexts_by_child[state]) {
				const Transition& transition{*context_rule.transition};
				std::vector<State> siblings{};
				siblings.reserve(transition.children.size() - 1);
				for (size_t position{0}; position < transition.children.size(); ++position) {
					if (position != context_rule.position) { siblings.push_back(block[transition.children[position]]); }
				}
				contexts.emplace_back(
					transition.symbol, context_rule.position, std::move(siblings), block[transition.target]
				);
			}
			std::ranges::sort(contexts);
			contexts.erase(std::ranges::unique(contexts).begin(), contexts.end());
			Signature signature{block[state], std::move(contexts)};
			const State next_block{nfa::to_state(renumbering.size())};
			refined[state] = renumbering.try_emplace(std::move(signature), next_block).first->second;
		}
		block = std::move(refined);
		if (renumbering.size() == num_of_blocks) { break; }
		num_of_blocks = renumbering.size();
	}
	return quotient_by_blocks(dfa, block, num_of_blocks);
}

} // namespace mata::ta
