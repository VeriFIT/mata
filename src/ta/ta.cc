/** @file
 * @brief Ranked bottom-up tree automaton implementation.
 */

#include "mata/ta/ta.hh"

#include <algorithm>
#include <deque>
#include <functional>
#include <stdexcept>

namespace mata::ta {

const std::string TYPE_TA{"TA-explicit"};

namespace {

bool all_productive(const std::vector<State>& states, const std::vector<bool>& productive) {
	return std::all_of(states.begin(), states.end(), [&productive](const State state) { return productive[state]; });
}

struct Productivity {
	std::vector<bool> productive;
	/// The rule that first made each productive state productive; its children became productive before it.
	std::vector<const Transition*> generators;
};

Productivity analyze_productivity(const Ta& aut) {
	struct PendingRule {
		const Transition* transition;
		size_t missing_children;
	};

	const size_t num_of_states{aut.num_of_states()};
	Productivity result{
		.productive = std::vector<bool>(num_of_states, false),
		.generators = std::vector<const Transition*>(num_of_states, nullptr)
	};
	std::vector<PendingRule> pending_rules{};
	pending_rules.reserve(aut.delta.size());
	std::unordered_map<State, std::vector<size_t>> rules_by_child{};
	std::vector<size_t> nullary_rules{};
	for (const auto& [target, transitions] : aut.delta) {
		for (const Transition& transition : transitions) {
			const size_t rule_index{pending_rules.size()};
			std::vector<State> unique_children{transition.children};
			std::ranges::sort(unique_children);
			unique_children.erase(std::ranges::unique(unique_children).begin(), unique_children.end());
			pending_rules.push_back(PendingRule{&transition, unique_children.size()});
			if (unique_children.empty()) {
				nullary_rules.push_back(rule_index);
			} else {
				for (const State child : unique_children) { rules_by_child[child].push_back(rule_index); }
			}
		}
	}

	std::deque<State> worklist{};
	const auto make_productive{[&](const PendingRule& rule) {
		const State target{rule.transition->target};
		if (result.productive[target]) { return; }
		result.productive[target] = true;
		result.generators[target] = rule.transition;
		worklist.push_back(target);
	}};

	for (const size_t rule_index : nullary_rules) { make_productive(pending_rules[rule_index]); }
	while (!worklist.empty()) {
		const State child{worklist.front()};
		worklist.pop_front();
		const auto rules_it{rules_by_child.find(child)};
		if (rules_it == rules_by_child.end()) { continue; }
		for (const size_t rule_index : rules_it->second) {
			PendingRule& rule{pending_rules[rule_index]};
			if (rule.missing_children == 0 || --rule.missing_children != 0) { continue; }
			make_productive(rule);
		}
	}
	return result;
}

/// Expands only the generators below @p state, so witnesses of other productive states are never built.
Tree build_witness(const std::vector<const Transition*>& generators, const State state) {
	const Transition& rule{*generators[state]};
	std::vector<Tree> children{};
	children.reserve(rule.children.size());
	for (const State child : rule.children) { children.push_back(build_witness(generators, child)); }
	return Tree{rule.symbol, std::move(children)};
}

struct RuleKey {
	Symbol symbol;
	std::vector<State> children;
	bool operator==(const RuleKey&) const = default;
};

struct RuleKeyHash {
	size_t operator()(const RuleKey& key) const {
		size_t hash{std::hash<Symbol>{}(key.symbol)};
		for (const State child : key.children) { hash = utils::hash_combine(hash, child); }
		return hash;
	}
};

class Evaluator {
  public:
	explicit Evaluator(const Ta& aut) : aut_{aut} {
		for (const auto& [target, transitions] : aut.delta) {
			for (const Transition& transition : transitions) {
				if (transition.children.empty()) {
					nullary_rules_.push_back(&transition);
				} else {
					rules_by_first_child_[transition.children.front()].push_back(&transition);
				}
			}
		}
	}

	StateSet evaluate(const Tree& tree) const {
		const std::optional<size_t> arity{aut_.delta.alphabet().arity(tree.symbol)};
		if (!arity.has_value() || *arity != tree.children.size()) { return {}; }

		std::vector<StateSet> child_results{};
		child_results.reserve(tree.children.size());
		for (const Tree& child : tree.children) {
			child_results.push_back(evaluate(child));
			if (child_results.back().empty()) { return {}; }
		}

		StateSet result{};
		if (child_results.empty()) {
			for (const Transition* transition : nullary_rules_) {
				if (transition->symbol == tree.symbol) { result.insert(transition->target); }
			}
			return result;
		}

		for (const State first_child : child_results.front()) {
			const auto rules_it{rules_by_first_child_.find(first_child)};
			if (rules_it == rules_by_first_child_.end()) { continue; }
			for (const Transition* transition : rules_it->second) {
				if (transition->symbol != tree.symbol || transition->children.size() != child_results.size()) {
					continue;
				}
				bool matches{true};
				for (size_t position{1}; position < child_results.size(); ++position) {
					if (!child_results[position].contains(transition->children[position])) {
						matches = false;
						break;
					}
				}
				if (matches) { result.insert(transition->target); }
			}
		}
		return result;
	}

  private:
	const Ta& aut_;
	std::vector<const Transition*> nullary_rules_{};
	std::unordered_map<State, std::vector<const Transition*>> rules_by_first_child_{};
};

} // namespace

RankedAlphabet::RankedAlphabet(std::initializer_list<std::pair<const Symbol, size_t>> symbols) {
	for (const auto& [symbol, arity] : symbols) { add(symbol, arity); }
}

bool RankedAlphabet::add(const Symbol symbol, const size_t arity) {
	const auto [it, inserted]{arities_.emplace(symbol, arity)};
	if (!inserted && it->second != arity) {
		throw std::invalid_argument(
			"ranked alphabet: symbol " + std::to_string(symbol) + " has arity " + std::to_string(it->second) +
			", not " + std::to_string(arity)
		);
	}
	return inserted;
}

std::optional<size_t> RankedAlphabet::arity(const Symbol symbol) const {
	const auto it{arities_.find(symbol)};
	if (it == arities_.end()) { return std::nullopt; }
	return it->second;
}

bool RankedAlphabet::is_compatible(const Symbol symbol, const size_t arity) const {
	const auto it{arities_.find(symbol)};
	return it == arities_.end() || it->second == arity;
}

bool Delta::add(Transition transition) {
	nfa::num_of_states_for(transition.target);
	for (const State child : transition.children) { nfa::num_of_states_for(child); }
	alphabet_.add(transition.symbol, transition.children.size());
	TransitionList& transitions{transitions_[transition.target]};
	if (std::find(transitions.begin(), transitions.end(), transition) != transitions.end()) { return false; }
	transitions.push_back(std::move(transition));
	++size_;
	return true;
}

bool Delta::contains(const Transition& transition) const {
	const auto target_it{transitions_.find(transition.target)};
	if (target_it == transitions_.end()) { return false; }
	return std::find(target_it->second.begin(), target_it->second.end(), transition) != target_it->second.end();
}

void Delta::clear() {
	alphabet_ = {};
	transitions_.clear();
	size_ = 0;
}

const Delta::TransitionList& Delta::operator[](const State target) const {
	static const TransitionList empty{};
	const auto it{transitions_.find(target)};
	return it == transitions_.end() ? empty : it->second;
}

Ta::Ta(const size_t num_of_states, RankedAlphabet alphabet) : delta{std::move(alphabet)} {
	nfa::check_num_of_states(num_of_states);
	num_of_states_ = num_of_states;
}

Ta::Ta(Delta delta, StateSet final, const size_t num_of_states)
	: final{std::move(final)},
	  delta{std::move(delta)},
	  num_of_states_{num_of_states} {
	for (const auto& [target, transitions] : this->delta) {
		num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(target));
		for (const Transition& transition : transitions) {
			for (const State child : transition.children) {
				num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(child));
			}
		}
	}
	for (const State state : this->final) { num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(state)); }
}

State Ta::add_state() {
	const State state{nfa::to_state(num_of_states_)};
	++num_of_states_;
	return state;
}

State Ta::add_state(const State state) {
	num_of_states_ = std::max(num_of_states_, nfa::num_of_states_for(state));
	return state;
}

bool Ta::add_transition(Transition transition) {
	add_state(transition.target);
	for (const State child : transition.children) { add_state(child); }
	return delta.add(std::move(transition));
}

void Ta::validate() const {
	for (const State state : final) {
		if (state >= num_of_states_) { throw std::logic_error("TA final state is outside the state set"); }
	}
	for (const auto& [target, transitions] : delta) {
		if (target >= num_of_states_) { throw std::logic_error("TA transition target is outside the state set"); }
		for (const Transition& transition : transitions) {
			if (transition.target != target ||
				!delta.alphabet().is_compatible(transition.symbol, transition.children.size())) {
				throw std::logic_error("TA target index or ranked alphabet is inconsistent");
			}
			for (const State child : transition.children) {
				if (child >= num_of_states_) { throw std::logic_error("TA transition child is outside the state set"); }
			}
		}
	}
}

StateSet Ta::evaluate(const Tree& tree) const {
	validate();
	return Evaluator{*this}.evaluate(tree);
}

bool Ta::accepts(const Tree& tree) const {
	const StateSet root_states{evaluate(tree)};
	return root_states.is_intersection_empty_with(final) == false;
}

bool Ta::is_lang_empty(Tree* witness) const {
	validate();
	const Productivity productivity{analyze_productivity(*this)};
	for (const State state : final) {
		if (!productivity.productive[state]) { continue; }
		if (witness != nullptr) { *witness = build_witness(productivity.generators, state); }
		return false;
	}
	return true;
}

bool Ta::is_deterministic() const {
	std::unordered_set<RuleKey, RuleKeyHash> keys{};
	keys.reserve(delta.size());
	for (const auto& [target, transitions] : delta) {
		for (const Transition& transition : transitions) {
			if (!keys.emplace(transition.symbol, transition.children).second) { return false; }
		}
	}
	return true;
}

Ta Ta::determinize(std::unordered_map<StateSet, State>* subset_map) const {
	validate();
	if (subset_map != nullptr) { subset_map->clear(); }
	Ta result{delta.alphabet()};
	if (delta.empty()) { return result; }

	std::unordered_map<Symbol, StateSet> nullary_targets{};
	std::unordered_map<Symbol, std::vector<const Transition*>> rules_by_symbol{};
	for (const auto& [target, transitions] : delta) {
		for (const Transition& transition : transitions) {
			if (transition.children.empty()) {
				nullary_targets[transition.symbol].insert(transition.target);
			} else {
				rules_by_symbol[transition.symbol].push_back(&transition);
			}
		}
	}

	std::vector<StateSet> macrostates{};
	std::unordered_map<StateSet, State> macrostate_indices{};
	const auto discover{[&](StateSet macrostate, const Symbol symbol, const std::vector<State>& children) {
		State parent_state{};
		if (const auto it{macrostate_indices.find(macrostate)}; it != macrostate_indices.end()) {
			parent_state = it->second;
		} else {
			parent_state = nfa::to_state(macrostates.size());
			result.add_state(parent_state);
			if (!macrostate.is_intersection_empty_with(final)) { result.final.insert(parent_state); }
			if (subset_map != nullptr) { subset_map->emplace(macrostate, parent_state); }
			macrostate_indices.emplace(macrostate, parent_state);
			macrostates.push_back(std::move(macrostate));
		}
		result.add_transition(symbol, children, parent_state);
	}};

	for (const auto& [symbol, targets] : nullary_targets) { discover(targets, symbol, {}); }

	// Each tuple of macrostates is built once: while expanding its highest macrostate, at the first position holding
	// it. Its target is the union over every rule enabled by the whole tuple, so the result stays deterministic.
	// enabled[p] holds the rules of the symbol whose first p children lie in the chosen macrostates.
	std::vector<std::vector<const Transition*>> enabled{};
	std::vector<State> tuple{};
	std::vector<State> parent_targets{};
	for (size_t next_macrostate{0}; next_macrostate < macrostates.size(); ++next_macrostate) {
		for (const auto& symbol_rules : rules_by_symbol) {
			const Symbol symbol{symbol_rules.first};
			const std::vector<const Transition*>& rules{symbol_rules.second};
			const size_t arity{rules.front()->children.size()};
			enabled.resize(arity + 1);
			tuple.resize(arity);
			for (size_t new_position{0}; new_position < arity; ++new_position) {
				const auto enumerate{[&](auto&& self, const size_t position) -> void {
					if (position == arity) {
						parent_targets.clear();
						for (const Transition* rule : enabled[arity]) { parent_targets.push_back(rule->target); }
						discover(StateSet{parent_targets}, symbol, tuple);
						return;
					}
					const std::vector<const Transition*>& candidates{position == 0 ? rules : enabled[position]};
					const size_t first{position == new_position ? next_macrostate : 0};
					const size_t last{position < new_position ? next_macrostate : next_macrostate + 1};
					for (size_t child_macrostate{first}; child_macrostate < last; ++child_macrostate) {
						std::vector<const Transition*>& matching{enabled[position + 1]};
						matching.clear();
						for (const Transition* rule : candidates) {
							if (macrostates[child_macrostate].contains(rule->children[position])) {
								matching.push_back(rule);
							}
						}
						if (matching.empty()) { continue; }
						tuple[position] = nfa::to_state(child_macrostate);
						self(self, position + 1);
					}
				}};
				enumerate(enumerate, 0);
			}
		}
	}
	return result;
}

Ta& Ta::trim(StateRenaming* state_renaming) & {
	validate();
	if (state_renaming != nullptr) { state_renaming->clear(); }
	const Productivity productivity{analyze_productivity(*this)};
	std::vector<bool> useful(num_of_states_, false);
	std::deque<State> worklist{};
	for (const State state : final) {
		if (productivity.productive[state] && !useful[state]) {
			useful[state] = true;
			worklist.push_back(state);
		}
	}
	while (!worklist.empty()) {
		const State target{worklist.front()};
		worklist.pop_front();
		for (const Transition& transition : delta[target]) {
			if (!all_productive(transition.children, productivity.productive)) { continue; }
			for (const State child : transition.children) {
				if (!useful[child]) {
					useful[child] = true;
					worklist.push_back(child);
				}
			}
		}
	}

	std::vector<State> new_states(num_of_states_, Limits::max_state);
	Ta trimmed{delta.alphabet()};
	for (size_t old_state{0}; old_state < num_of_states_; ++old_state) {
		if (!useful[old_state]) { continue; }
		const State new_state{trimmed.add_state()};
		new_states[old_state] = new_state;
		if (state_renaming != nullptr) { state_renaming->emplace(nfa::to_state(old_state), new_state); }
		if (final.contains(nfa::to_state(old_state))) { trimmed.final.insert(new_state); }
	}
	for (size_t old_target{0}; old_target < num_of_states_; ++old_target) {
		if (!useful[old_target]) { continue; }
		for (const Transition& transition : delta[nfa::to_state(old_target)]) {
			if (!std::all_of(transition.children.begin(), transition.children.end(), [&useful](const State child) {
					return useful[child];
				})) {
				continue;
			}
			std::vector<State> new_children{};
			new_children.reserve(transition.children.size());
			for (const State child : transition.children) { new_children.push_back(new_states[child]); }
			trimmed.add_transition(transition.symbol, std::move(new_children), new_states[old_target]);
		}
	}
	*this = std::move(trimmed);
	return *this;
}

} // namespace mata::ta
