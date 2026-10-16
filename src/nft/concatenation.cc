/* nft-concatenation.cc -- Concatenation of NFTs
 */

#include <type_traits>
#include <utility>

#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/nft.hh"
#include "mata/utils/assert.hh"

using namespace mata::nft;

namespace mata::nft {

namespace {

/// Whether the concatenation of @p lhs and @p rhs has no state of either operand, i.e., is the empty transducer.
bool concatenation_is_empty(const Nft& lhs, const Nft& rhs) {
	return lhs.num_of_states() == 0 || rhs.num_of_states() == 0 || lhs.initial.empty() || lhs.final.empty() ||
		   rhs.initial.empty() || rhs.final.empty();
}

/**
 * Shared body of every @c algorithms::concatenate_eps() overload: consumes @p lhs, and consumes @p rhs only when it is
 *  passed as an rvalue. The operands must not be aliased.
 */
template <typename RhsNft>
Nft concatenate_eps_consuming(
	Nft&& lhs,
	RhsNft&& rhs,
	const Symbol& epsilon,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	constexpr bool consume_rhs{!std::is_const_v<std::remove_reference_t<RhsNft>>};
	MATA_ASSERT(lhs.levels.num_of_levels == rhs.levels.num_of_levels);
	const size_t num_of_levels{lhs.levels.num_of_levels};
	const size_t rhs_num_of_levels{rhs.levels.num_of_levels};
	if (concatenation_is_empty(lhs, rhs)) {
		lhs = Nft::with_levels(num_of_levels);
		if constexpr (consume_rhs) { rhs = Nft::with_levels(rhs_num_of_levels); }
		return Nft::with_levels(num_of_levels);
	}

	const size_t lhs_states_num{lhs.num_of_states()};
	const size_t rhs_states_num{rhs.num_of_states()};
	const size_t result_num_of_states{lhs_states_num + rhs_states_num};
	nfa::check_num_of_states(result_num_of_states);
	StateRenaming lhs_states_renaming{}; // Map mapping lhs states to result states.
	StateRenaming rhs_states_renaming{}; // Map mapping rhs states to result states.

	// The lhs states keep their indices in the result, so its delta and initial states move in as they are.
	// The final states and levels of lhs are only read here.
	const utils::SparseSet<State> lhs_final_states{std::move(lhs.final)};
	const Levels lhs_levels{std::move(lhs.levels)};
	Nft result{Nft::with_levels(num_of_levels)}; // Concatenated automaton.
	result.delta = std::move(lhs.delta);
	result.initial = std::move(lhs.initial);
	lhs = Nft::with_levels(num_of_levels);

	// Append the rhs transitions, shifted by the number of lhs states, before any state beyond lhs is allocated.
	result.delta.allocate(lhs_states_num);
	if constexpr (consume_rhs) {
		result.delta.append_shifted(std::move(rhs.delta), lhs_states_num);
	} else {
		result.delta.append_shifted(rhs.delta, lhs_states_num);
	}
	result.add_state(result_num_of_states - 1);

	// Map lhs states to result states.
	lhs_states_renaming.reserve(lhs_states_num);
	State result_state_index{0};
	for (State lhs_state{0}; lhs_state < lhs_states_num; ++lhs_state) {
		lhs_states_renaming.insert(std::make_pair(lhs_state, result_state_index));
		result.levels[result_state_index] = lhs_levels[lhs_state];
		++result_state_index;
	}
	// Map rhs states to result states.
	rhs_states_renaming.reserve(rhs_states_num);
	for (State rhs_state{0}; rhs_state < rhs_states_num; ++rhs_state) {
		rhs_states_renaming.insert(std::make_pair(rhs_state, result_state_index));
		result.levels[result_state_index] = rhs.levels[rhs_state];
		++result_state_index;
	}

	// Add epsilon transitions connecting lhs and rhs automata.
	// The epsilon transitions lead from lhs original final states to rhs original initial states.
	const std::vector<Symbol> nft_epsilon(result.levels.num_of_levels, epsilon);
	for (const State& lhs_final_state : lhs_final_states) {
		for (const State& rhs_initial_state : rhs.initial) {
			result.add_transition_by_levels(lhs_final_state, nft_epsilon, rhs_states_renaming[rhs_initial_state]);
		}
	}

	// Make result final states.
	for (const State& rhs_final_state : rhs.final) { result.final.insert(rhs_states_renaming[rhs_final_state]); }
	if constexpr (consume_rhs) { rhs = Nft::with_levels(rhs_num_of_levels); }

	if (!use_epsilon) { result.remove_epsilon(epsilon); }
	if (lhs_state_renaming != nullptr) { *lhs_state_renaming = lhs_states_renaming; }
	if (rhs_state_renaming != nullptr) { *rhs_state_renaming = rhs_states_renaming; }
	return result;
}

} // namespace

Nft concatenate(
	const Nft& lhs,
	const Nft& rhs,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(lhs, rhs, EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming);
}

Nft concatenate(
	Nft&& lhs,
	const Nft& rhs,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(
		std::move(lhs), rhs, EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
}

Nft concatenate(
	Nft&& lhs, Nft&& rhs, const bool use_epsilon, StateRenaming* lhs_state_renaming, StateRenaming* rhs_state_renaming
) {
	return algorithms::concatenate_eps(
		std::move(lhs), std::move(rhs), EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming
	);
}

Nft& Nft::concatenate(const Nft& aut) {
	MATA_ASSERT(levels.num_of_levels == aut.levels.num_of_levels);
	const size_t n = this->num_of_states();
	auto upd_fnc = [&](const State st) { return static_cast<State>(st + n); };

	// copy the information about aut to save the case when this is the same object as aut.
	utils::SparseSet<mata::nft::State> aut_initial = aut.initial;
	utils::SparseSet<mata::nft::State> aut_final = aut.final;
	const size_t aut_n = aut.num_of_states();
	nfa::check_num_of_states(n + aut_n);

	this->delta.allocate(n);
	this->delta.append_shifted(aut.delta, n);
	this->levels.append(aut.levels);

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

Nft& Nft::concatenate(Nft&& aut) {
	MATA_ASSERT(levels.num_of_levels == aut.levels.num_of_levels);
	if (this == &aut) {
		// Self-concatenation: one controlled snapshot of the operand; `this` keeps its own content.
		const Nft snapshot{aut};
		return concatenate(snapshot);
	}

	const size_t n = this->num_of_states();
	auto upd_fnc = [&](const State st) { return st + n; };

	const size_t aut_n = aut.num_of_states();
	const utils::SparseSet<State> aut_initial{std::move(aut.initial)};
	const utils::SparseSet<State> aut_final{std::move(aut.final)};

	this->delta.allocate(n);
	this->delta.append_shifted(std::move(aut.delta), n);
	this->levels.append(aut.levels);

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
				for (const State& dest : ini_mv.targets) { this->delta.add(fin, ini_mv.symbol, dest); }
			}
		}
	}
	this->final = new_fin;

	// Leave `aut` valid and empty, with its number of levels.
	aut = Nft::with_levels(aut.levels.num_of_levels);
	return *this;
}

Nft algorithms::concatenate_eps(
	const Nft& lhs,
	const Nft& rhs,
	const Symbol& epsilon,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	MATA_ASSERT(lhs.levels.num_of_levels == rhs.levels.num_of_levels);
	// Compute concatenation of given automata.
	// Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.
	if (concatenation_is_empty(lhs, rhs)) { return Nft::with_levels(lhs.levels.num_of_levels); }
	// The one intentional copy: the parts of lhs that the result is built on. The copy cannot alias rhs.
	return concatenate_eps_consuming(
		Nft{lhs.delta, lhs.initial, lhs.final, lhs.levels}, rhs, epsilon, use_epsilon, lhs_state_renaming,
		rhs_state_renaming
	);
}

Nft algorithms::concatenate_eps(
	Nft&& lhs,
	const Nft& rhs,
	const Symbol& epsilon,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	if (&lhs == &rhs) {
		// Self-concatenation: one controlled snapshot of the right operand, then the consuming path.
		const Nft rhs_snapshot{rhs};
		return concatenate_eps_consuming(
			std::move(lhs), rhs_snapshot, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming
		);
	}
	return concatenate_eps_consuming(std::move(lhs), rhs, epsilon, use_epsilon, lhs_state_renaming, rhs_state_renaming);
}

Nft algorithms::concatenate_eps(
	Nft&& lhs,
	Nft&& rhs,
	const Symbol& epsilon,
	const bool use_epsilon,
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

Nft concatenate_nth_power(Nft nft_to_concatenate, unsigned power) {
	// `result` holds the accumulating product (starts as identity — empty-string NFT)
	Nft result = builder::create_empty_string_nft(nft_to_concatenate.levels.num_of_levels);

	// `base` is the current power of the original NFT.
	Nft base = std::move(nft_to_concatenate);

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

} // Namespace mata::nft.
