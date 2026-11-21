/** @file
 * @brief The connection of the operands of the in-place concatenation, shared by @c Nfa::concatenate() and
 *  @c Nft::concatenate(); not installed.
 */

#ifndef MATA_SRC_NFA_CONCATENATION_INTERNAL_HH_
#define MATA_SRC_NFA_CONCATENATION_INTERNAL_HH_

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

#include "mata/nfa/delta.hh"
#include "mata/nfa/types.hh"
#include "mata/utils/sparse-set.hh"

namespace mata::nfa::internal {

/**
 * @brief Connect the operands of an in-place concatenation whose rhs delta is already appended at @p offset.
 *
 * Every lhs final state takes over the transitions of every shifted rhs initial state. The posts of the rhs initial
 *  states are first united into one connector post, which each lhs final state then merges in with one sorted union
 *  per symbol, instead of one @c Delta::add() per target.
 *
 * @param[in] lhs_final The lhs final states; may be the final states of the automaton owning @p delta.
 * @param[in] num_of_states The number of states of the concatenation, which the returned set covers.
 * @return The final states of the concatenation: the shifted rhs final states, and the lhs final states when an rhs
 *  initial state is final.
 */
inline utils::SparseSet<State> connect_concatenation(
	Delta& delta,
	const utils::SparseSet<State>& lhs_final,
	const utils::SparseSet<State>& rhs_initial,
	const utils::SparseSet<State>& rhs_final,
	const State offset,
	const size_t num_of_states
) {
	utils::SparseSet<State> result_final{};
	result_final.reserve(num_of_states);
	for (const State rhs_final_state : rhs_final) { result_final.insert(offset + rhs_final_state); }

	// With several rhs initial states, their (symbol, target) pairs are sorted at once: uniting the posts one by one
	//  costs a merge with the growing connector for each of them.
	StatePost connector{};
	bool rhs_initial_is_final{false};
	if (rhs_initial.size() == 1) {
		const State rhs_initial_state{*rhs_initial.begin()};
		rhs_initial_is_final = rhs_final.contains(rhs_initial_state);
		connector = delta[offset + rhs_initial_state];
	} else {
		std::vector<std::pair<Symbol, State>> moves{};
		for (const State rhs_initial_state : rhs_initial) {
			rhs_initial_is_final = rhs_initial_is_final || rhs_final.contains(rhs_initial_state);
			for (const SymbolPost& symbol_post : delta[offset + rhs_initial_state]) {
				for (const State target : symbol_post.targets) { moves.emplace_back(symbol_post.symbol, target); }
			}
		}
		std::ranges::sort(moves);
		const auto [duplicates_begin, duplicates_end]{std::ranges::unique(moves)};
		moves.erase(duplicates_begin, duplicates_end);
		for (const auto& [symbol, target] : moves) {
			if (connector.empty() || connector.back().symbol != symbol) { connector.push_back(SymbolPost{symbol}); }
			// The targets of one symbol come sorted and duplicate-free.
			connector.back().push_back(target);
		}
	}

	for (const State lhs_final_state : lhs_final) {
		if (rhs_initial_is_final) { result_final.insert(lhs_final_state); }
		for (const SymbolPost& symbol_post : connector) {
			delta.add(lhs_final_state, symbol_post.symbol, std::span<const State>{symbol_post.targets});
		}
	}
	return result_final;
}

} // namespace mata::nfa::internal

#endif // MATA_SRC_NFA_CONCATENATION_INTERNAL_HH_
