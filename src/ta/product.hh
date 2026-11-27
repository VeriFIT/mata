/** @file
 * @brief Lazy synchronized bottom-up construction for ranked rules.
 */

#ifndef MATA_TA_PRODUCT_HH
#define MATA_TA_PRODUCT_HH

#include "mata/ta/ta.hh"

#include <algorithm>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mata::ta::detail {

struct ProductRule {
	Symbol input_symbol;
	Symbol output_symbol;
	std::span<const State> children;
	State target;
};

inline std::vector<ProductRule> product_rules(const Ta& aut) {
	std::vector<ProductRule> rules{};
	rules.reserve(aut.delta.size());
	for (const auto& [target, transitions] : aut.delta) {
		for (const Transition& transition : transitions) {
			rules.push_back(ProductRule{transition.symbol, transition.symbol, transition.children, transition.target});
		}
	}
	return rules;
}

struct ProductState {
	State left;
	State right;
	bool operator==(const ProductState&) const = default;
};

struct ProductStateHash {
	size_t operator()(const ProductState state) const {
		return utils::hash_combine(std::hash<State>{}(state.left), state.right);
	}
};

struct RulePair {
	size_t left;
	size_t right;
	bool operator==(const RulePair&) const = default;
};

struct RulePairHash {
	size_t operator()(const RulePair pair) const {
		return utils::hash_combine(std::hash<size_t>{}(pair.left), pair.right);
	}
};

struct RulePosition {
	size_t rule;
	size_t position;
};

struct Requirement {
	Symbol symbol;
	size_t arity;
	size_t position;
	State child;
	bool operator==(const Requirement&) const = default;
};

struct RequirementHash {
	size_t operator()(const Requirement& requirement) const {
		size_t hash{std::hash<Symbol>{}(requirement.symbol)};
		hash = utils::hash_combine(hash, requirement.arity);
		hash = utils::hash_combine(hash, requirement.position);
		return utils::hash_combine(hash, requirement.child);
	}
};

/**
 * Discovers only state pairs with a common input tree. Rule pairs are instantiated lazily when one of their ordered
 * child requirements is satisfied. A newly satisfied position expands only tuples containing that new child pair;
 * previously discovered pairs at the other positions make this a complete incremental closure, including repetitions.
 */
template <typename OnPair, typename OnRule>
void bottom_up_product(
	const std::vector<ProductRule>& left_rules,
	const std::vector<ProductRule>& right_rules,
	OnPair&& on_pair,
	OnRule&& on_rule
) {
	std::unordered_map<State, std::vector<RulePosition>> left_by_child{};
	std::unordered_map<Requirement, std::vector<RulePosition>, RequirementHash> right_by_requirement{};
	std::unordered_map<Symbol, std::vector<size_t>> left_nullary{};
	std::unordered_map<Symbol, std::vector<size_t>> right_nullary{};

	const auto index_left{[&](const size_t rule_index) {
		const ProductRule& rule{left_rules[rule_index]};
		if (rule.children.empty()) {
			left_nullary[rule.input_symbol].push_back(rule_index);
			return;
		}
		for (size_t position{0}; position < rule.children.size(); ++position) {
			left_by_child[rule.children[position]].push_back(RulePosition{rule_index, position});
		}
	}};
	const auto index_right{[&](const size_t rule_index) {
		const ProductRule& rule{right_rules[rule_index]};
		if (rule.children.empty()) {
			right_nullary[rule.input_symbol].push_back(rule_index);
			return;
		}
		for (size_t position{0}; position < rule.children.size(); ++position) {
			right_by_requirement[Requirement{
									 rule.input_symbol, rule.children.size(), position, rule.children[position]
								 }]
				.push_back(RulePosition{rule_index, position});
		}
	}};
	for (size_t rule_index{0}; rule_index < left_rules.size(); ++rule_index) { index_left(rule_index); }
	for (size_t rule_index{0}; rule_index < right_rules.size(); ++rule_index) { index_right(rule_index); }

	std::unordered_map<ProductState, State, ProductStateHash> product_indices{};
	std::vector<ProductState> product_states{};
	std::vector<State> worklist{};
	const auto discover{[&](const ProductState state) {
		if (const auto it{product_indices.find(state)}; it != product_indices.end()) { return it->second; }
		const State product{nfa::to_state(product_states.size())};
		product_indices.emplace(state, product);
		product_states.push_back(state);
		worklist.push_back(product);
		on_pair(product, state.left, state.right);
		return product;
	}};

	for (const auto& [symbol, left_indices] : left_nullary) {
		const auto right_it{right_nullary.find(symbol)};
		if (right_it == right_nullary.end()) { continue; }
		for (const size_t left_index : left_indices) {
			for (const size_t right_index : right_it->second) {
				const ProductRule& left{left_rules[left_index]};
				const ProductRule& right{right_rules[right_index]};
				const State target{discover(ProductState{left.target, right.target})};
				on_rule(left, right, std::span<const State>{}, target);
			}
		}
	}

	struct PendingRulePair {
		size_t left;
		size_t right;
		std::vector<std::vector<State>> candidates;
		std::vector<State> tuple;
	};
	std::vector<PendingRulePair> pending_rules{};
	std::unordered_map<RulePair, size_t, RulePairHash> pending_indices{};

	const auto emit_for_new_position{[&](PendingRulePair& pending, const size_t new_position, const State new_child) {
		const ProductRule& left{left_rules[pending.left]};
		const ProductRule& right{right_rules[pending.right]};
		pending.tuple[new_position] = new_child;
		const auto enumerate{[&](auto&& self, const size_t position) -> void {
			if (position == pending.candidates.size()) {
				const State target{discover(ProductState{left.target, right.target})};
				on_rule(left, right, std::span<const State>{pending.tuple}, target);
				return;
			}
			if (position == new_position) {
				self(self, position + 1);
				return;
			}
			for (const State child : pending.candidates[position]) {
				pending.tuple[position] = child;
				self(self, position + 1);
			}
		}};
		enumerate(enumerate, 0);
	}};

	for (size_t next_product{0}; next_product < worklist.size(); ++next_product) {
		const State current{worklist[next_product]};
		const ProductState state{product_states[static_cast<size_t>(current)]};
		const auto left_it{left_by_child.find(state.left)};
		if (left_it == left_by_child.end()) { continue; }

		for (const RulePosition left_position : left_it->second) {
			const ProductRule& left{left_rules[left_position.rule]};
			const auto right_it{right_by_requirement.find(
				Requirement{left.input_symbol, left.children.size(), left_position.position, state.right}
			)};
			if (right_it == right_by_requirement.end()) { continue; }

			for (const RulePosition right_position : right_it->second) {
				const RulePair key{left_position.rule, right_position.rule};
				auto [pending_it, inserted]{pending_indices.emplace(key, pending_rules.size())};
				if (inserted) {
					const ProductRule& right{right_rules[right_position.rule]};
					PendingRulePair pending{
						.left = key.left,
						.right = key.right,
						.candidates = std::vector<std::vector<State>>(left.children.size()),
						.tuple = std::vector<State>(left.children.size())
					};
					for (size_t position{0}; position < left.children.size(); ++position) {
						const auto known{
							product_indices.find(ProductState{left.children[position], right.children[position]})
						};
						if (known != product_indices.end() && known->second != current) {
							pending.candidates[position].push_back(known->second);
						}
					}
					pending_rules.push_back(std::move(pending));
				}
				PendingRulePair& pending{pending_rules[pending_it->second]};
				pending.candidates[left_position.position].push_back(current);
				emit_for_new_position(pending, left_position.position, current);
			}
		}
	}
}

} // namespace mata::ta::detail

#endif // MATA_TA_PRODUCT_HH
