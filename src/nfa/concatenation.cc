/** @file
 * @brief Concatenation of NFAs.
 */

#include <type_traits>
#include <utility>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

namespace mata::nfa {

namespace {

/// Whether the concatenation of @p lhs and @p rhs has no state of either operand, i.e., is the empty automaton.
bool concatenation_is_empty(const Nfa& lhs, const Nfa& rhs) {
	return lhs.num_of_states() == 0 || rhs.num_of_states() == 0 || lhs.initial.empty() || lhs.final.empty() ||
		   rhs.initial.empty() || rhs.final.empty();
}

/**
 * Shared body of every @c algorithms::concatenate_eps() overload: consumes @p lhs, and consumes @p rhs only when it is
 *  passed as an rvalue. The operands must not be aliased.
 */
template <typename RhsNfa>
Nfa concatenate_eps_consuming(
	Nfa&& lhs,
	// A forwarding reference only to take a const operand or one to consume, which is used in place.
	RhsNfa&& rhs,
	// By value: `epsilon` may name an entry of a requested renaming map, which is cleared and refilled below.
	const Symbol epsilon,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	constexpr bool consume_rhs{!std::is_const_v<std::remove_reference_t<RhsNfa>>};
	const size_t lhs_states_num{lhs.num_of_states()};
	const size_t rhs_states_num{rhs.num_of_states()};

	// The renamings are arithmetic: lhs states keep their indices, rhs states shift by the number of lhs states.
	//  Computing the maps costs one entry per state, so they are filled only when the caller asked for them, and
	//  also on the early return, where they keep describing the (empty) concatenation.
	auto output_renamings = [&] {
		if (lhs_state_renaming != nullptr) {
			lhs_state_renaming->clear();
			lhs_state_renaming->reserve(lhs_states_num);
			for (State state{0}; state < lhs_states_num; ++state) { lhs_state_renaming->emplace(state, state); }
		}
		if (rhs_state_renaming != nullptr) {
			rhs_state_renaming->clear();
			rhs_state_renaming->reserve(rhs_states_num);
			for (State state{0}; state < rhs_states_num; ++state) {
				rhs_state_renaming->emplace(state, static_cast<State>(lhs_states_num) + state);
			}
		}
	};

	// The rhs shift must be representable before it is written into a requested map, also on the empty return.
	if (rhs_state_renaming != nullptr && Limits::max_num_of_states - lhs_states_num < rhs_states_num) {
		throw_state_overflow();
	}
	if (concatenation_is_empty(lhs, rhs)) {
		output_renamings();
		lhs = Nfa{};
		if constexpr (consume_rhs) { rhs = Nfa{}; }
		return Nfa{};
	}

	const size_t result_num_of_states{lhs_states_num + rhs_states_num};

	output_renamings();

	// The lhs states keep their indices in the result, so its delta and initial states move in as they are.
	// The final states of lhs do not survive the concatenation, so they are only read here.
	const utils::SparseSet<State> lhs_final_states{std::move(lhs.final)};
	Nfa result{}; // Concatenated automaton.
	result.delta = std::move(lhs.delta);
	result.initial = std::move(lhs.initial);
	lhs = Nfa{};

	// Append the rhs transitions, shifted by the number of lhs states, before any state beyond lhs is allocated.
	result.delta.allocate(lhs_states_num);
	if constexpr (consume_rhs) {
		result.delta.append_shifted(std::move(rhs.delta), static_cast<State>(lhs_states_num));
	} else {
		result.delta.append_shifted(rhs.delta, static_cast<State>(lhs_states_num));
	}
	result.add_state(static_cast<State>(result_num_of_states - 1));

	// Add epsilon transitions connecting lhs and rhs automata.
	// The epsilon transitions lead from lhs original final states to rhs original initial states.
	for (const State& lhs_final_state : lhs_final_states) {
		for (const State rhs_initial_state : rhs.initial) {
			result.delta.add(lhs_final_state, epsilon, static_cast<State>(lhs_states_num) + rhs_initial_state);
		}
	}

	// Make result final states.
	for (const State rhs_final_state : rhs.final) {
		result.final.insert(static_cast<State>(lhs_states_num) + rhs_final_state);
	}
	if constexpr (consume_rhs) { rhs = Nfa{}; }

	if (!use_epsilon) { result.remove_epsilon(epsilon); }
	return result;
}

} // namespace

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

Nfa concatenate(
	Nfa&& lhs, Nfa&& rhs, const bool use_epsilon, StateRenaming* lhs_state_renaming, StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(
		std::move(lhs), std::move(rhs), EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
}

Nfa& Nfa::concatenate(const Nfa& aut) {
	const size_t n = this->num_of_states();
	const size_t aut_n = aut.num_of_states();
	check_num_of_states(n + aut_n);
	auto upd_fnc = [&](const State st) { return static_cast<State>(st + n); };

	// copy the information about aut to save the case when this is the same object as aut.
	const utils::SparseSet<mata::nfa::State> aut_initial = aut.initial;
	const utils::SparseSet<mata::nfa::State> aut_final = aut.final;

	this->delta.allocate(n);
	this->delta.append_shifted(aut.delta, static_cast<State>(n));

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
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
		const Nfa snapshot{aut};
		return concatenate(snapshot);
	}

	const size_t n = this->num_of_states();
	const size_t aut_n = aut.num_of_states();
	check_num_of_states(n + aut_n);
	auto upd_fnc = [&](const State st) { return static_cast<State>(st + n); };

	// The state sets of `aut` are still read after its delta has been consumed.
	const utils::SparseSet<State> aut_initial{aut.initial};
	const utils::SparseSet<State> aut_final{aut.final};

	// Move the operand's state posts in; the shift by `n` is the renumbering the lvalue path copies for.
	this->delta.append_shifted(std::move(aut.delta), static_cast<State>(n));

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
	// The consuming path fills the requested renaming maps also for the empty result.
	if (concatenation_is_empty(lhs, rhs)) {
		// Do not clone a possibly large lhs only to throw it away: fill the requested maps from the original counts.
		if (rhs_state_renaming != nullptr && Limits::max_num_of_states - lhs.num_of_states() < rhs.num_of_states()) {
			throw_state_overflow();
		}
		if (lhs_state_renaming != nullptr) {
			lhs_state_renaming->clear();
			lhs_state_renaming->reserve(lhs.num_of_states());
			for (State state{0}; state < lhs.num_of_states(); ++state) { lhs_state_renaming->emplace(state, state); }
		}
		if (rhs_state_renaming != nullptr) {
			rhs_state_renaming->clear();
			rhs_state_renaming->reserve(rhs.num_of_states());
			for (State state{0}; state < rhs.num_of_states(); ++state) {
				rhs_state_renaming->emplace(state, static_cast<State>(lhs.num_of_states()) + state);
			}
		}
		return Nfa{};
	}
	// The one intentional copy: the parts of lhs that the result is built on. The copy cannot alias rhs.
	return concatenate_eps_consuming(
		Nfa{lhs.delta, lhs.initial, lhs.final}, rhs, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
}

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
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
		const Nfa rhs_snapshot{rhs};
		return concatenate_eps_consuming(
			std::move(lhs), rhs_snapshot, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
		);
	}
	return concatenate_eps_consuming(std::move(lhs), rhs, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming);
}

Nfa algorithms::concatenate_eps(
	Nfa&& lhs,
	Nfa&& rhs,
	const Symbol& epsilon,
	bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	// Aliased operands: the overload consuming only lhs snapshots rhs first.
	if (&lhs == &rhs) {
		return concatenate_eps(
			std::move(lhs), std::as_const(rhs), epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
		);
	}
	return concatenate_eps_consuming(
		std::move(lhs), std::move(rhs), epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
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
		if (power & 1U) { result.concatenate(base); }

		// Shift to the next bit.
		power >>= 1;

		// If there are still bits to process, square `base` (i.e. base = base * base).
		if (power) { base.concatenate(base); }
	}

	return result;
}

} // Namespace mata::nfa.
