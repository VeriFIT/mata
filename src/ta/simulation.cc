/** @file
 * @brief Ranked tree simulation computation and simulation-based reduction.
 */

#include "mata/ta/simulation.hh"

#include <deque>
#include <stdexcept>

namespace mata::ta {
namespace {

using Simlib::Util::BinaryRelation;

bool rule_covered(const Transition& source, const Delta::TransitionList& candidates, const BinaryRelation& downward) {
	for (const Transition& candidate : candidates) {
		if (source.symbol != candidate.symbol || source.children.size() != candidate.children.size()) { continue; }
		bool covered{true};
		for (size_t i{0}; i < source.children.size(); ++i) {
			if (!downward.get(source.children[i], candidate.children[i])) {
				covered = false;
				break;
			}
		}
		if (covered) { return true; }
	}
	return false;
}

BinaryRelation downward_simulation(const Ta& aut) {
	const size_t num_of_states{aut.num_of_states()};
	BinaryRelation relation{num_of_states, true};
	bool changed{true};
	while (changed) {
		changed = false;
		for (State q{0}; q < num_of_states; ++q) {
			for (State r{0}; r < num_of_states; ++r) {
				if (!relation.get(q, r)) { continue; }
				for (const Transition& source : aut.delta[q]) {
					if (!rule_covered(source, aut.delta[r], relation)) {
						relation.set(q, r, false);
						changed = true;
						break;
					}
				}
			}
		}
	}
	return relation;
}

struct OneHoleEnvironment {
	const Transition* rule;
	size_t hole;
};

using Environments = std::vector<std::vector<OneHoleEnvironment>>;

bool same_environment(const OneHoleEnvironment& lhs, const OneHoleEnvironment& rhs) {
	if (lhs.rule->symbol != rhs.rule->symbol || lhs.hole != rhs.hole || lhs.rule->target != rhs.rule->target) {
		return false;
	}
	for (size_t i{0}; i < lhs.rule->children.size(); ++i) {
		if (i != lhs.hole && lhs.rule->children[i] != rhs.rule->children[i]) { return false; }
	}
	return true;
}

Environments collect_environments(const Ta& aut) {
	Environments environments(aut.num_of_states());
	for (const auto& [target, rules] : aut.delta) {
		for (const Transition& rule : rules) {
			for (size_t hole{0}; hole < rule.children.size(); ++hole) {
				const OneHoleEnvironment environment{&rule, hole};
				auto& state_environments{environments[rule.children[hole]]};
				bool duplicate{false};
				for (const OneHoleEnvironment& other : state_environments) {
					if (same_environment(environment, other)) {
						duplicate = true;
						break;
					}
				}
				if (!duplicate) { state_environments.push_back(environment); }
			}
		}
	}
	return environments;
}

bool environment_covered(
	const OneHoleEnvironment& source,
	const OneHoleEnvironment& candidate,
	const BinaryRelation& downward,
	const BinaryRelation& upward
) {
	if (source.rule->symbol != candidate.rule->symbol || source.hole != candidate.hole ||
		!upward.get(source.rule->target, candidate.rule->target)) {
		return false;
	}
	for (size_t i{0}; i < source.rule->children.size(); ++i) {
		if (i != source.hole && !downward.get(source.rule->children[i], candidate.rule->children[i])) { return false; }
	}
	return true;
}

BinaryRelation upward_simulation(const Ta& aut, const BinaryRelation& downward) {
	const size_t num_of_states{aut.num_of_states()};
	const Environments environments{collect_environments(aut)};
	BinaryRelation relation{num_of_states, false};
	for (State q{0}; q < num_of_states; ++q) {
		for (State r{0}; r < num_of_states; ++r) {
			relation.set(q, r, !aut.final.contains(q) || aut.final.contains(r));
		}
	}

	bool changed{true};
	while (changed) {
		changed = false;
		for (State q{0}; q < num_of_states; ++q) {
			for (State r{0}; r < num_of_states; ++r) {
				if (!relation.get(q, r)) { continue; }
				for (const OneHoleEnvironment& source : environments[q]) {
					bool covered{false};
					for (const OneHoleEnvironment& candidate : environments[r]) {
						if (environment_covered(source, candidate, downward, relation)) {
							covered = true;
							break;
						}
					}
					if (!covered) {
						relation.set(q, r, false);
						changed = true;
						break;
					}
				}
			}
		}
	}
	return relation;
}

BinaryRelation composed_simulation(const Ta& aut) {
	const size_t num_of_states{aut.num_of_states()};
	const BinaryRelation downward{downward_simulation(aut)};
	const BinaryRelation upward{upward_simulation(aut, downward)};

	// Start with downward composed with inverse upward, then retain its maximal downward-extensible fragment.
	BinaryRelation relation{num_of_states, false};
	for (State q{0}; q < num_of_states; ++q) {
		for (State r{0}; r < num_of_states; ++r) {
			for (State mediator{0}; mediator < num_of_states; ++mediator) {
				if (downward.get(q, mediator) && upward.get(r, mediator)) {
					relation.set(q, r, true);
					break;
				}
			}
		}
	}
	std::deque<std::pair<State, State>> missing_pairs;
	for (State q{0}; q < num_of_states; ++q) {
		for (State r{0}; r < num_of_states; ++r) {
			if (!relation.get(q, r)) { missing_pairs.emplace_back(q, r); }
		}
	}
	while (!missing_pairs.empty()) {
		const auto [q, s]{missing_pairs.front()};
		missing_pairs.pop_front();
		for (State r{0}; r < num_of_states; ++r) {
			if (downward.get(r, s) && relation.get(q, r)) {
				relation.set(q, r, false);
				missing_pairs.emplace_back(q, r);
			}
		}
	}
	return relation;
}

Ta quotient(const Ta& aut, const BinaryRelation& relation) {
	const size_t num_of_states{aut.num_of_states()};
	std::vector<size_t> representatives;
	relation.get_symmetric_quotient_projection(representatives);
	std::vector<State> reduced_states(num_of_states);
	Ta result{aut.delta.alphabet()};
	for (State q{0}; q < num_of_states; ++q) {
		if (representatives[q] == q) { reduced_states[q] = result.add_state(); }
	}
	for (State q{0}; q < num_of_states; ++q) {
		reduced_states[q] = reduced_states[representatives[q]];
		if (aut.final.contains(q)) { result.final.insert(reduced_states[q]); }
	}
	for (State q{0}; q < num_of_states; ++q) {
		for (const Transition& rule : aut.delta[q]) {
			std::vector<State> children;
			children.reserve(rule.children.size());
			for (const State child : rule.children) { children.push_back(reduced_states[child]); }
			result.add_transition(rule.symbol, std::move(children), reduced_states[q]);
		}
	}
	return std::move(result).trim();
}

} // namespace

Simlib::Util::BinaryRelation compute_simulation(const Ta& aut, const SimulationDirection direction) {
	aut.validate();
	switch (direction) {
		case SimulationDirection::Downward:
			return downward_simulation(aut);
		case SimulationDirection::Upward: {
			const BinaryRelation downward{downward_simulation(aut)};
			return upward_simulation(aut, downward);
		}
	}
	throw std::invalid_argument("unknown tree simulation direction");
}

Ta reduce_by_simulation(const Ta& aut, const SimulationDirection direction) {
	aut.validate();
	if (direction == SimulationDirection::Upward) {
		throw std::invalid_argument("upward simulation alone is not a sound tree automaton reduction");
	}
	if (direction != SimulationDirection::Downward) {
		throw std::invalid_argument("unknown tree simulation direction");
	}
	return quotient(aut, downward_simulation(aut));
}

Ta reduce_by_composed_simulation(const Ta& aut) {
	aut.validate();
	return quotient(aut, composed_simulation(aut));
}

} // namespace mata::ta
