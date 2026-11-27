/** @file
 * @brief Predicate-state abstraction for NFAs.
 */

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "mata/nfa/nfa.hh"

mata::nfa::Nfa mata::nfa::collapse_by_predicates(
	const Nfa& aut, std::initializer_list<Nfa> predicates, const ParameterMap& params
) {
	auto view = std::vector<const Nfa*>{};
	view.reserve(predicates.size());
	for (const Nfa& predicate : predicates) { view.push_back(&predicate); }
	return collapse_by_predicates(aut, std::span<const Nfa* const>{view.data(), view.size()}, params);
}

mata::nfa::Nfa
	mata::nfa::collapse_by_predicates(const Nfa& aut, const std::vector<Nfa>& predicates, const ParameterMap& params) {
	auto view = std::vector<const Nfa*>{};
	view.reserve(predicates.size());
	for (const Nfa& predicate : predicates) { view.push_back(&predicate); }
	return collapse_by_predicates(aut, std::span<const Nfa* const>{view.data(), view.size()}, params);
}

mata::nfa::Nfa mata::nfa::collapse_by_predicates(
	const Nfa& aut, std::span<const Nfa* const> predicates, const ParameterMap& params
) {
	const std::string granularity{params.contains("granularity") ? params.at("granularity") : "states"};
	const std::string direction{params.contains("direction") ? params.at("direction") : "forward"};
	if (granularity != "states" && granularity != "languages") {
		throw std::invalid_argument(std::string(__func__) + ": unknown \"granularity\": " + granularity);
	}
	if (direction != "forward" && direction != "backward") {
		throw std::invalid_argument(std::string(__func__) + ": unknown \"direction\": " + direction);
	}
	if (std::ranges::any_of(predicates, [](const Nfa* predicate) { return predicate == nullptr; })) {
		throw std::invalid_argument(std::string(__func__) + ": null predicate");
	}
	const bool backward{direction == "backward"};
	const bool all_predicate_states{granularity == "states"};

	Nfa input{trim(backward ? revert(aut) : aut)};
	Nfa dfa{input.is_deterministic() ? std::move(input) : determinize(input)};
	dfa.trim();
	if (dfa.num_of_states() == 0) {
		Nfa empty_language{1};
		empty_language.initial.insert(0);
		empty_language.alphabet = aut.alphabet;
		return empty_language;
	}

	// Predecessors of the oriented DFA certify common suffixes. In backward mode those suffixes are prefixes of aut.
	const Nfa subject_predecessors{revert(dfa)};
	using Fingerprint = std::vector<size_t>;
	std::vector<Fingerprint> fingerprints(dfa.num_of_states());
	size_t predicate_state_offset{0};
	for (const Nfa* predicate_ptr : predicates) {
		const Nfa& predicate{*predicate_ptr};
		// Reversal preserves state numbers. Backward mode orients the predicate for prefixes and uses the original
		// transition relation for predecessor steps; forward mode uses these relations the other way around.
		const Nfa reversed_predicate{revert(predicate)};
		const Nfa& oriented_predicate{backward ? reversed_predicate : predicate};
		const Nfa& predicate_predecessors{backward ? predicate : reversed_predicate};
		const size_t num_predicate_states{oriented_predicate.num_of_states()};
		if (num_predicate_states > std::numeric_limits<size_t>::max() - predicate_state_offset) {
			throw std::overflow_error(std::string(__func__) + ": predicate state index overflow");
		}
		if (num_predicate_states != 0) {
			if (num_predicate_states > std::numeric_limits<size_t>::max() / dfa.num_of_states()) {
				throw std::overflow_error(std::string(__func__) + ": product state index overflow");
			}
			std::vector<bool> visited(dfa.num_of_states() * num_predicate_states, false);
			std::vector<size_t> worklist{};
			const auto discover{[&](const State subject, const State predicate_state) {
				const size_t pair{static_cast<size_t>(subject) * num_predicate_states + predicate_state};
				if (visited[pair]) { return; }
				visited[pair] = true;
				worklist.push_back(pair);
				if (all_predicate_states || oriented_predicate.initial.contains(predicate_state)) {
					fingerprints[subject].push_back(predicate_state_offset + predicate_state);
				}
			}};

			for (const State subject_final : subject_predecessors.initial) {
				for (const State predicate_final : predicate_predecessors.initial) {
					discover(subject_final, predicate_final);
				}
			}
			while (!worklist.empty()) {
				const size_t pair{worklist.back()};
				worklist.pop_back();
				const State subject{static_cast<State>(pair / num_predicate_states)};
				const State predicate_state{static_cast<State>(pair % num_predicate_states)};
				const StatePost& subject_post{subject_predecessors.delta[subject]};
				const StatePost& predicate_post{predicate_predecessors.delta[predicate_state]};
				auto subject_symbol{subject_post.begin()};
				auto predicate_symbol{predicate_post.begin()};
				while (subject_symbol != subject_post.end() && predicate_symbol != predicate_post.end()) {
					if (subject_symbol->symbol < predicate_symbol->symbol) {
						++subject_symbol;
						continue;
					}
					if (predicate_symbol->symbol < subject_symbol->symbol) {
						++predicate_symbol;
						continue;
					}
					for (const State subject_predecessor : subject_symbol->targets) {
						for (const State predicate_predecessor : predicate_symbol->targets) {
							discover(subject_predecessor, predicate_predecessor);
						}
					}
					++subject_symbol;
					++predicate_symbol;
				}
			}
		}
		predicate_state_offset += num_predicate_states;
	}

	std::map<Fingerprint, State> renumbering{};
	std::vector<State> block(dfa.num_of_states());
	for (State state{0}; state < dfa.num_of_states(); ++state) {
		Fingerprint& fingerprint{fingerprints[state]};
		std::ranges::sort(fingerprint);
		fingerprint.erase(std::unique(fingerprint.begin(), fingerprint.end()), fingerprint.end());
		const auto next_block{static_cast<State>(renumbering.size())};
		block[state] = renumbering.try_emplace(std::move(fingerprint), next_block).first->second;
	}

	Nfa quotient{renumbering.size()};
	for (const State state : dfa.initial) { quotient.initial.insert(block[state]); }
	for (const State state : dfa.final) { quotient.final.insert(block[state]); }
	for (const Transition& transition : dfa.delta.transitions()) {
		quotient.delta.add(block[transition.source], transition.symbol, block[transition.target]);
	}
	Nfa result{backward ? revert(trim(quotient)) : trim(quotient)};
	result.alphabet = aut.alphabet;
	return result;
}
