/** @file
 * @brief Concatenation of NFAs.
 */

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

namespace mata::nfa {

Nfa concatenate(
	const Nfa& lhs,
	const Nfa& rhs,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(lhs, rhs, EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming);
}

Nfa concatenate(
	Nfa&& lhs,
	const Nfa& rhs,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(
		std::move(lhs), rhs, EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
}

Nfa& Nfa::concatenate(const Nfa& aut) {
	const size_t n = this->num_of_states();
	auto upd_fnc = [&](const State st) { return st + n; };

	// copy the information about aut to save the case when this is the same object as aut.
	utils::SparseSet<mata::nfa::State> aut_initial = aut.initial;
	utils::SparseSet<mata::nfa::State> aut_final = aut.final;
	const size_t aut_n = aut.num_of_states();

	this->delta.allocate(n);
	this->delta.append(aut.delta.renumber_targets(upd_fnc));

	// set accepting states
	utils::SparseSet<State> new_fin{};
	new_fin.reserve(n + aut_n);
	for (const State& aut_fin : aut_final) { new_fin.insert(upd_fnc(aut_fin)); }

	// connect both parts
	for (const State& ini : aut_initial) {
		const StatePost& ini_post = this->delta[upd_fnc(ini)];
		// is ini state also final?
		const bool is_final = aut_final[ini];
		for (const State& fin : this->final) {
			if (is_final) { new_fin.insert(fin); }
			for (const SymbolPost& ini_mv : ini_post) {
				// TODO: this should be done efficiently in a delta method
				// TODO: in fact it is not efficient for now
				for (const State& dest : ini_mv.targets) { this->delta.add(fin, ini_mv.symbol, dest); }
			}
		}
	}
	this->final = new_fin;
	return *this;
}

Nfa& Nfa::concatenate(Nfa&& aut) {
	if (this == &aut) {
		// Self-concatenation: one controlled snapshot of the operand; `this` keeps its own content.
		const Nfa snapshot{aut};
		return concatenate(snapshot);
	}

	const size_t n = this->num_of_states();
	auto upd_fnc = [&](const State st) { return st + n; };

	// The state sets of `aut` are still read after its delta has been consumed.
	const utils::SparseSet<State> aut_initial{aut.initial};
	const utils::SparseSet<State> aut_final{aut.final};
	const size_t aut_n = aut.num_of_states();

	// Move the operand's state posts in; the shift by `n` is the renumbering the lvalue path copies for.
	this->delta.append_shifted(std::move(aut.delta), n);

	// set accepting states
	utils::SparseSet<State> new_fin{};
	new_fin.reserve(n + aut_n);
	for (const State& aut_fin : aut_final) { new_fin.insert(upd_fnc(aut_fin)); }

	// connect both parts
	for (const State& ini : aut_initial) {
		const StatePost& ini_post = this->delta[upd_fnc(ini)];
		// is ini state also final?
		const bool is_final = aut_final[ini];
		for (const State& fin : this->final) {
			if (is_final) { new_fin.insert(fin); }
			for (const SymbolPost& ini_mv : ini_post) {
				// TODO: this should be done efficiently in a delta method
				// TODO: in fact it is not efficient for now
				for (const State& dest : ini_mv.targets) { this->delta.add(fin, ini_mv.symbol, dest); }
			}
		}
	}
	this->final = new_fin;

	// Reset `aut` to leave it valid and empty; `clear()` keeps the state domain of the sparse sets.
	aut = Nfa{};
	return *this;
}

Nfa algorithms::concatenate_eps(
	const Nfa& lhs,
	const Nfa& rhs,
	const Symbol& epsilon,
	bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	// Compute concatenation of given automata.
	// Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.

	if (lhs.num_of_states() == 0 || rhs.num_of_states() == 0 || lhs.initial.empty() || lhs.final.empty() ||
		rhs.initial.empty() || rhs.final.empty()) {
		return Nfa{};
	}

	const unsigned long lhs_states_num{lhs.num_of_states()};
	const unsigned long rhs_states_num{rhs.num_of_states()};
	Nfa result{}; // Concatenated automaton.
	StateRenaming lhs_states_renaming{}; // Map mapping rhs states to result states.
	StateRenaming rhs_states_renaming{}; // Map mapping rhs states to result states.

	const size_t result_num_of_states{lhs_states_num + rhs_states_num};
	if (result_num_of_states == 0) { return Nfa{}; }

	// Map lhs states to result states.
	lhs_states_renaming.reserve(lhs_states_num);
	Symbol result_state_index{0};
	for (State lhs_state{0}; lhs_state < lhs_states_num; ++lhs_state) {
		lhs_states_renaming.insert(std::make_pair(lhs_state, result_state_index));
		++result_state_index;
	}
	// Map rhs states to result states.
	rhs_states_renaming.reserve(rhs_states_num);
	for (State rhs_state{0}; rhs_state < rhs_states_num; ++rhs_state) {
		rhs_states_renaming.insert(std::make_pair(rhs_state, result_state_index));
		++result_state_index;
	}

	result = Nfa();
	result.delta = lhs.delta;
	result.initial = lhs.initial;
	result.add_state(result_num_of_states - 1);

	// Add epsilon transitions connecting lhs and rhs automata.
	// The epsilon transitions lead from lhs original final states to rhs original initial states.
	for (const auto& lhs_final_state : lhs.final) {
		for (const auto& rhs_initial_state : rhs.initial) {
			result.delta.add(lhs_final_state, epsilon, rhs_states_renaming[rhs_initial_state]);
		}
	}

	// Make result final states.
	for (const auto& rhs_final_state : rhs.final) { result.final.insert(rhs_states_renaming[rhs_final_state]); }

	// Add rhs transitions to the result.
	for (State rhs_state{0}; rhs_state < rhs_states_num; ++rhs_state) {
		for (const SymbolPost& rhs_move : rhs.delta.state_post(rhs_state)) {
			for (const State& rhs_state_to : rhs_move.targets) {
				result.delta.add(rhs_states_renaming[rhs_state], rhs_move.symbol, rhs_states_renaming[rhs_state_to]);
			}
		}
	}

	if (!use_epsilon) { result.remove_epsilon(epsilon); }
	if (lhs_state_renaming != nullptr) { *lhs_state_renaming = lhs_states_renaming; }
	if (rhs_state_renaming != nullptr) { *rhs_state_renaming = rhs_states_renaming; }
	return result;
} // concatenate_eps().

Nfa algorithms::concatenate_eps(
	Nfa&& lhs,
	const Nfa& rhs,
	const Symbol& epsilon,
	bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	if (&lhs == &rhs) {
		// Self-concatenation: one controlled snapshot of the right operand, then the consuming path.
		const Nfa rhs_snapshot{rhs};
		return concatenate_eps(
			std::move(lhs), rhs_snapshot, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
		);
	}

	if (lhs.num_of_states() == 0 || rhs.num_of_states() == 0 || lhs.initial.empty() || lhs.final.empty() ||
		rhs.initial.empty() || rhs.final.empty()) {
		lhs = Nfa{};
		return Nfa{};
	}

	const unsigned long lhs_states_num{lhs.num_of_states()};
	const unsigned long rhs_states_num{rhs.num_of_states()};
	const size_t result_num_of_states{lhs_states_num + rhs_states_num};

	StateRenaming lhs_states_renaming{}; // Map mapping lhs states to result states.
	StateRenaming rhs_states_renaming{}; // Map mapping rhs states to result states.

	// Map lhs states to result states.
	lhs_states_renaming.reserve(lhs_states_num);
	State result_state_index{0};
	for (State lhs_state{0}; lhs_state < lhs_states_num; ++lhs_state) {
		lhs_states_renaming.insert(std::make_pair(lhs_state, result_state_index));
		++result_state_index;
	}
	// Map rhs states to result states.
	rhs_states_renaming.reserve(rhs_states_num);
	for (State rhs_state{0}; rhs_state < rhs_states_num; ++rhs_state) {
		rhs_states_renaming.insert(std::make_pair(rhs_state, result_state_index));
		++result_state_index;
	}

	// The lhs states keep their indices in the result, so its delta and initial states move in as they are.
	// The final states of lhs do not survive the concatenation, so they are only read here.
	const utils::SparseSet<State> lhs_final_states{std::move(lhs.final)};
	Nfa result{}; // Concatenated automaton.
	result.delta = std::move(lhs.delta);
	result.initial = std::move(lhs.initial);
	lhs = Nfa{};

	// Append the rhs transitions, shifted by the number of lhs states, before any state beyond lhs is allocated.
	result.delta.allocate(lhs_states_num);
	result.delta.append_shifted(rhs.delta, lhs_states_num);
	result.add_state(result_num_of_states - 1);

	// Add epsilon transitions connecting lhs and rhs automata.
	// The epsilon transitions lead from lhs original final states to rhs original initial states.
	for (const State& lhs_final_state : lhs_final_states) {
		for (const State& rhs_initial_state : rhs.initial) {
			result.delta.add(lhs_final_state, epsilon, rhs_states_renaming[rhs_initial_state]);
		}
	}

	// Make result final states.
	for (const State& rhs_final_state : rhs.final) { result.final.insert(rhs_states_renaming[rhs_final_state]); }

	if (!use_epsilon) { result.remove_epsilon(epsilon); }
	if (lhs_state_renaming != nullptr) { *lhs_state_renaming = lhs_states_renaming; }
	if (rhs_state_renaming != nullptr) { *rhs_state_renaming = rhs_states_renaming; }
	return result;
}

Nfa concatenate_nth_power(Nfa nfa_to_concatenate, unsigned power) {
	// If power is 0, return the NFA that accepts only the empty string.
	if (power == 0) { return builder::create_empty_string_nfa(); }

	// `result` holds the accumulating product (starts as identity — empty-string NFA)
	Nfa result = builder::create_empty_string_nfa();

	// `base` is the current power of the original NFA.
	Nfa base = std::move(nfa_to_concatenate);

	// Exponentiation by squaring (binary exponentiation)
	// For each binary digit (LSB first): if the bit is 1, multiply `result` by `base`.
	// Then square `base` for the next bit.
	while (power > 0) {
		// If current least-significant bit is set, append `base` to `result`.
		if (power & 1u) { result.concatenate(base); }

		// Shift to the next bit.
		power >>= 1;

		// If there are still bits to process, square `base` (i.e. base = base * base).
		if (power) { base.concatenate(base); }
	}

	return result;
}

} // Namespace mata::nfa.
