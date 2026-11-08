/** @file
 * @brief NFA language inclusion by bisimulation up to congruence (HKC), after F. Bonchi and D. Pous, "Checking NFA
 *  equivalence with bisimulations up to congruence", POPL 2013.
 *
 * The check explores pairs of macrostates of the on-the-fly determinizations of its operands and skips a pair that
 *  lies in the congruence closure of the pairs stored so far: such a pair is equivalent whenever the stored ones are.
 *  States that cannot reach a final state are dropped from every macrostate, as the antichain inclusion does.
 */

#include <algorithm>
#include <cstdint>
#include <vector>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

#include "antichain-inclusion.hh"

using namespace mata::nfa;
using mata::nfa::internal::ProcessedAntichain;
using mata::nfa::internal::Step;

namespace {

/// Drops from @p set the states that cannot reach a final state: they contribute no word.
void drop_useless(StateSet& set, const std::vector<State>& distances) {
	set.erase_if([&](const State state) { return distances[state] == Limits::max_state; });
}

/// The state of the non-empty @p set closest to a final state.
State closest_to_final(const StateSet& set, const std::vector<State>& distances) {
	return *std::ranges::min_element(set, {}, [&](const State state) { return distances[state]; });
}

/// The length of a shortest word accepted from @p set, or @c Limits::max_state when @p set accepts none.
State min_distance(const StateSet& set, const std::vector<State>& distances) {
	return set.empty() ? Limits::max_state : distances[closest_to_final(set, distances)];
}

/**
 * @brief Writes to @p cex an accepting run of @p nfa: the symbols of the search steps that lead to @p node_idx,
 *  followed by a shortest word accepted from the macrostate of @p node_idx.
 *
 * The path goes backwards through the macrostates @p set_of(node) of @p nfa: a macrostate is the post of its parent
 *  over the symbol of its step, minus useless states, so some state of the parent reads that symbol into the current
 *  state, and the macrostate of an initial node only holds initial states.
 */
template <class SetOf>
void accepting_run(
	const Nfa& nfa,
	const std::vector<State>& distances,
	const std::vector<Step>& steps,
	size_t node_idx,
	const SetOf& set_of,
	Run& cex
) {
	State state{closest_to_final(set_of(node_idx), distances)};
	const Run suffix{nfa.get_shortest_accepting_run_from_state(state, distances)};
	cex.word.clear();
	cex.path = {state};
	for (; steps[node_idx].parent != node_idx; node_idx = steps[node_idx].parent) {
		const Step& step{steps[node_idx]};
		state = *std::ranges::find_if(set_of(step.parent), [&](const State source) {
			return nfa.delta.contains(source, step.symbol, state);
		});
		cex.word.push_back(step.symbol);
		cex.path.push_back(state);
	}
	std::ranges::reverse(cex.word);
	std::ranges::reverse(cex.path);
	cex.word.insert(cex.word.end(), suffix.word.begin(), suffix.word.end());
	// The suffix starts in the last state of the path.
	cex.path.insert(cex.path.end(), suffix.path.begin() + 1, suffix.path.end());
}

/// A pair (X ∪ Y, Y) of the HKC inclusion check: a macrostate X of the smaller automaton and Y of the bigger one.
struct InclusionPair {
	StateSet smaller_set;
	StateSet bigger_set;
	/// @c macrostate_signature() of @c bigger_set.
	uint64_t signature;
	/// The states of @c smaller_set whose processed antichain still holds this pair. Once there is none, smaller
	///  pairs cover every state of @c smaller_set, so the pair is in their congruence closure and is not expanded.
	size_t live_entries;
};

} // namespace

bool mata::nfa::algorithms::is_included_hkc(
	const Nfa& smaller, const Nfa& bigger, [[maybe_unused]] const Alphabet* const alphabet, Run* const cex
) {
	if (cex != nullptr) {
		cex->word.clear();
		cex->path.clear();
	}
	const std::vector<State> distances_smaller{smaller.distances_to_final()};
	const std::vector<State> distances_bigger{bigger.distances_to_final()};

	std::vector<InclusionPair> pairs{};
	std::vector<Step> steps{}; // Indexed like @c pairs; empty unless @p cex is requested.
	std::vector<size_t> worklist{};
	// The pairs stored for each state x of the smaller automaton: (X ∪ Y, Y) is in the congruence closure of the
	//  stored pairs iff every x ∈ X lies in X_i of a stored (X_i ∪ Y_i, Y_i) with Y_i ⊆ Y, which is the antichain
	//  subsumption of (x, Y). The rules X_i ∪ Y_i → Y_i never fire anything new, and the left-hand side Y_i of the
	//  others only holds states of the bigger automaton, which no rule adds, so no rewriting chains.
	std::vector<ProcessedAntichain> processed(smaller.num_of_states());
	std::vector<State> uncovered{};

	// Stores and schedules the pair (X ∪ Y, Y) unless it is in the congruence closure; false if it shows that
	//  inclusion does not hold.
	const auto visit{[&](StateSet smaller_set, StateSet bigger_set, const size_t parent, const Symbol symbol) {
		drop_useless(smaller_set, distances_smaller);
		if (smaller_set.empty()) { return true; }
		drop_useless(bigger_set, distances_bigger);
		// A shortest word accepted from X that is shorter than any accepted from Y is accepted from X ∪ Y only. This
		//  covers a final state of X with none in Y.
		if (min_distance(smaller_set, distances_smaller) < min_distance(bigger_set, distances_bigger)) {
			if (cex != nullptr) {
				const size_t pair_idx{pairs.size()};
				pairs.push_back(
					InclusionPair{
						.smaller_set = std::move(smaller_set),
						.bigger_set = {},
						.signature = 0,
						.live_entries = 0,
					}
				);
				steps.push_back(Step{.parent = parent, .symbol = symbol});
				accepting_run(
					smaller, distances_smaller, steps, pair_idx,
					[&](const size_t idx) -> const StateSet& { return pairs[idx].smaller_set; }, *cex
				);
			}
			return false;
		}

		const uint64_t signature{macrostate_signature(bigger_set)};
		uncovered.clear();
		for (const State state : smaller_set) {
			if (!processed[state].subsumes(pairs, bigger_set, signature)) { uncovered.push_back(state); }
		}
		if (uncovered.empty()) { return true; }

		const size_t pair_idx{pairs.size()};
		pairs.push_back(
			InclusionPair{
				.smaller_set = std::move(smaller_set),
				.bigger_set = std::move(bigger_set),
				.signature = signature,
				.live_entries = uncovered.size(),
			}
		);
		// Only the uncovered states get an entry: a covered one is subsumed by a pair with a smaller macrostate.
		for (const State state : uncovered) {
			processed[state].prune(pairs, pairs[pair_idx].bigger_set, signature, [](InclusionPair& pruned) {
				--pruned.live_entries;
			});
			processed[state].insert(pairs, pair_idx);
		}
		if (cex != nullptr) { steps.push_back(Step{.parent = parent, .symbol = symbol}); }
		worklist.push_back(pair_idx);
		return true;
	}};

	if (!visit(StateSet{smaller.initial}, StateSet{bigger.initial}, 0, 0)) { return false; }

	SynchronizedExistentialSymbolPostIterator smaller_iterator{};
	SynchronizedExistentialSymbolPostIterator bigger_iterator{};
	while (!worklist.empty()) {
		const size_t pair_idx{worklist.back()};
		worklist.pop_back();
		if (pairs[pair_idx].live_entries == 0) { continue; }

		smaller_iterator.reset();
		bigger_iterator.reset();
		for (const State state : pairs[pair_idx].smaller_set) {
			mata::utils::push_back(smaller_iterator, smaller.delta[state]);
		}
		for (const State state : pairs[pair_idx].bigger_set) {
			mata::utils::push_back(bigger_iterator, bigger.delta[state]);
		}
		// A symbol that X cannot read leads to (Y', Y'), which is trivially in the closure.
		while (smaller_iterator.advance()) {
			const Symbol symbol{smaller_iterator.get_current_minimum()->symbol};
			StateSet bigger_succ{};
			if (bigger_iterator.synchronize_with(symbol)) { bigger_succ = bigger_iterator.unify_targets(); }
			if (!visit(smaller_iterator.unify_targets(), std::move(bigger_succ), pair_idx, symbol)) { return false; }
		}
	}
	return true;
}
