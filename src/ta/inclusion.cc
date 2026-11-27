/** @file
 * @brief Antichain language inclusion for ranked bottom-up tree automata.
 */

#include "mata/ta/inclusion.hh"

#include "mata/nfa/algorithms.hh"
#include "mata/utils/ord-vector.hh"

#include "../nfa/antichain-inclusion.hh"

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mata::ta {
namespace {

using nfa::internal::Node;
using nfa::internal::ProcessedAntichain;

struct Step {
	Symbol symbol;
	std::vector<size_t> children;
};

using Rule = const Transition*;
using RulesBySymbol = std::unordered_map<Symbol, std::vector<Rule>>;

RulesBySymbol index_rules_by_symbol(const Ta& aut) {
	RulesBySymbol result{};
	for (const auto& [target, transitions] : aut.delta) {
		for (const Transition& transition : transitions) { result[transition.symbol].push_back(&transition); }
	}
	return result;
}

Tree reconstruct_tree(const std::vector<Step>& steps, const size_t node_idx) {
	const Step& step{steps[node_idx]};
	std::vector<Tree> children{};
	children.reserve(step.children.size());
	for (const size_t child : step.children) { children.push_back(reconstruct_tree(steps, child)); }
	return Tree{step.symbol, std::move(children)};
}

Tree reconstruct_tree(const std::vector<Step>& steps, const Symbol symbol, const std::vector<size_t>& child_nodes) {
	std::vector<Tree> children{};
	children.reserve(child_nodes.size());
	for (const size_t child : child_nodes) { children.push_back(reconstruct_tree(steps, child)); }
	return Tree{symbol, std::move(children)};
}

} // namespace

bool is_included(const Ta& smaller, const Ta& bigger, Tree* const counterexample) {
	smaller.validate();
	bigger.validate();

	const RulesBySymbol bigger_rules{index_rules_by_symbol(bigger)};
	std::vector<Node> nodes{};
	std::vector<Step> steps{};
	std::vector<size_t> worklist{};
	std::vector<ProcessedAntichain> processed(smaller.num_of_states());
	// Nodes already taken from the worklist, per smaller state. Dominated ones stay here and are skipped when read.
	std::vector<std::vector<size_t>> expanded_nodes(smaller.num_of_states());

	// Every visit corresponds to one concrete tree. Its bigger set is the complete post-image of the child
	// macrostates, not one nondeterministically selected bigger run.
	const auto visit = [&](const State smaller_state, StateSet bigger_set, const Symbol symbol,
						   const std::vector<size_t>& child_nodes) -> bool {
		// A subsumed pair can itself be the bad root, so this check must precede antichain subsumption.
		if (smaller.final.contains(smaller_state) && utils::are_disjoint(bigger_set, bigger.final)) {
			if (counterexample != nullptr) { *counterexample = reconstruct_tree(steps, symbol, child_nodes); }
			return false;
		}

		const uint64_t signature{nfa::algorithms::macrostate_signature(bigger_set)};
		if (processed[smaller_state].subsumes(nodes, bigger_set, signature)) { return true; }

		// Pruning clears only search data. Steps and smaller states stay intact because live descendants may
		// still refer to the pruned nodes while reconstructing a tree.
		processed[smaller_state].prune(nodes, bigger_set, signature);
		const size_t node_idx{nodes.size()};
		nodes.push_back(
			Node{
				.smaller_state = smaller_state,
				.bigger_set = std::move(bigger_set),
				.signature = signature,
				.dominated = false,
			}
		);
		if (counterexample != nullptr) { steps.push_back(Step{symbol, child_nodes}); }
		processed[smaller_state].insert(nodes, node_idx);
		worklist.push_back(node_idx);
		return true;
	};

	const auto bigger_post = [&](const Symbol symbol, const std::vector<size_t>& child_nodes) {
		StateSet result{};
		for (const size_t child : child_nodes) {
			if (nodes[child].bigger_set.empty()) { return result; }
		}
		const auto rules_it{bigger_rules.find(symbol)};
		if (rules_it == bigger_rules.end()) { return result; }
		for (const Rule rule : rules_it->second) {
			if (rule->children.size() != child_nodes.size()) { continue; }
			bool matches{true};
			for (size_t position{0}; position < child_nodes.size(); ++position) {
				if (!nodes[child_nodes[position]].bigger_set.contains(rule->children[position])) {
					matches = false;
					break;
				}
			}
			if (matches) { result.insert(rule->target); }
		}
		return result;
	};

	// Nullary rules are the bootstrap: an empty bigger post-image is a genuine rejecting configuration, not an
	// absent product state that can be skipped.
	for (const auto& [target, transitions] : smaller.delta) {
		for (const Transition& transition : transitions) {
			if (!transition.children.empty()) { continue; }
			if (!visit(transition.target, bigger_post(transition.symbol, {}), transition.symbol, {})) { return false; }
		}
	}

	std::vector<std::vector<Rule>> rules_by_child(smaller.num_of_states());
	for (const auto& [target, transitions] : smaller.delta) {
		for (const Transition& transition : transitions) {
			if (transition.children.empty()) { continue; }
			const StateSet unique_children{transition.children};
			for (const State child : unique_children) { rules_by_child[child].push_back(&transition); }
		}
	}

	// Semi-naive expansion: a tuple of child nodes is instantiated once, when the last of its nodes is expanded, at
	// the first position holding that node. Earlier positions of the same smaller state draw only from the nodes
	// expanded before it, later ones also from the node itself, which keeps repeated children such as f(q,q).
	std::vector<size_t> same_state_nodes{};
	std::vector<std::vector<size_t>> choices{};
	std::vector<size_t> selected{};
	while (!worklist.empty()) {
		const size_t node_idx{worklist.back()};
		worklist.pop_back();
		if (nodes[node_idx].dominated) { continue; }
		const State smaller_state{nodes[node_idx].smaller_state};
		same_state_nodes.clear();
		for (const size_t earlier : expanded_nodes[smaller_state]) {
			if (!nodes[earlier].dominated) { same_state_nodes.push_back(earlier); }
		}
		same_state_nodes.push_back(node_idx);
		expanded_nodes[smaller_state].push_back(node_idx);
		const std::span<const size_t> earlier_nodes{same_state_nodes.data(), same_state_nodes.size() - 1};

		for (const Rule smaller_rule : rules_by_child[smaller_state]) {
			const size_t arity{smaller_rule->children.size()};
			choices.resize(arity);
			bool has_all_children{true};
			for (size_t position{0}; position < arity && has_all_children; ++position) {
				const State child{smaller_rule->children[position]};
				choices[position].clear();
				if (child == smaller_state) { continue; }
				for (const size_t child_node : expanded_nodes[child]) {
					if (!nodes[child_node].dominated) { choices[position].push_back(child_node); }
				}
				has_all_children = !choices[position].empty();
			}
			if (!has_all_children) { continue; }

			selected.resize(arity);
			for (size_t new_position{0}; new_position < arity; ++new_position) {
				if (smaller_rule->children[new_position] != smaller_state) { continue; }
				const auto instantiate = [&](auto&& self, const size_t position) -> bool {
					if (position == arity) {
						for (const size_t child : selected) {
							if (nodes[child].dominated) { return true; }
						}
						return visit(
							smaller_rule->target, bigger_post(smaller_rule->symbol, selected), smaller_rule->symbol,
							selected
						);
					}
					if (position == new_position) {
						selected[position] = node_idx;
						return self(self, position + 1);
					}
					std::span<const size_t> options{choices[position]};
					if (smaller_rule->children[position] == smaller_state) {
						options = position < new_position ? earlier_nodes : std::span<const size_t>{same_state_nodes};
					}
					for (const size_t child_node : options) {
						selected[position] = child_node;
						if (!self(self, position + 1)) { return false; }
					}
					return true;
				};
				if (!instantiate(instantiate, 0)) { return false; }
			}
		}
	}
	return true;
}

} // namespace mata::ta
