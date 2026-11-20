/* nft-concatenation.cc -- Concatenation of NFTs
 */

#include <algorithm>
#include <span>
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
	// A forwarding reference only to take a const operand or one to consume, which is used in place.
	RhsNft&& rhs,
	// By value: `epsilon` may name an entry of a requested renaming map, which is cleared and refilled below.
	const Symbol epsilon,
	const bool use_epsilon,
	StateRenaming* lhs_state_renaming,
	StateRenaming* rhs_state_renaming
) {
	constexpr bool consume_rhs{!std::is_const_v<std::remove_reference_t<RhsNft>>};
	MATA_ASSERT(lhs.levels.num_of_levels == rhs.levels.num_of_levels);
	const size_t num_of_levels{lhs.levels.num_of_levels};
	const size_t rhs_num_of_levels{rhs.levels.num_of_levels};
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

	// The combined state count and rhs shift must be representable before construction, also on the empty return.
	if (nfa::Limits::max_num_of_states - lhs_states_num < rhs_states_num) { nfa::throw_state_overflow(); }
	if (concatenation_is_empty(lhs, rhs)) {
		output_renamings();
		lhs = Nft::with_levels(num_of_levels);
		if constexpr (consume_rhs) { rhs = Nft::with_levels(rhs_num_of_levels); }
		return Nft::with_levels(num_of_levels);
	}

	const size_t result_num_of_states{lhs_states_num + rhs_states_num};

	output_renamings();

	// The lhs states keep their indices in the result, so its delta, initial states and levels move in as they are.
	// The final states of lhs are only read here.
	utils::SparseSet<State> lhs_final_states{std::move(lhs.final)};
	Nft result{Nft::with_levels(num_of_levels)}; // Concatenated automaton.
	result.delta = std::move(lhs.delta);
	result.initial = std::move(lhs.initial);
	result.levels = std::move(lhs.levels);
	lhs = Nft::with_levels(num_of_levels);

	// Append the rhs transitions, shifted by the number of lhs states, before any state beyond lhs is allocated.
	result.delta.allocate(lhs_states_num);
	if constexpr (consume_rhs) {
		result.delta.append_shifted(std::move(rhs.delta), static_cast<State>(lhs_states_num));
	} else {
		result.delta.append_shifted(rhs.delta, static_cast<State>(lhs_states_num));
	}
	// The result states are the lhs states followed by the shifted rhs states, and so are their levels. Only the
	//  first lhs_states_num lhs levels belong to those states: a surplus entry cannot shift the rhs levels. Moving
	//  instead of copying the lhs levels, and growing them geometrically (`Levels::append()` reserves exactly), keeps
	//  a left fold of concatenations linear.
	result.levels.resize(lhs_states_num);
	if (const size_t num_of_result_levels{
			result_num_of_states + lhs_final_states.size() * (std::max(num_of_levels, size_t{1}) - 1)
		};
		result.levels.capacity() < num_of_result_levels) {
		result.levels.reserve(std::max(num_of_result_levels, 2 * result.levels.capacity()));
	}
	result.levels.append(rhs.levels);
	result.add_state(static_cast<State>(result_num_of_states - 1));

	// Share one epsilon chain per lhs final state across all rhs initial states.
	std::vector<State> shifted_rhs_initials{};
	shifted_rhs_initials.reserve(rhs.initial.size());
	for (const State rhs_initial_state : rhs.initial) {
		shifted_rhs_initials.push_back(static_cast<State>(lhs_states_num) + rhs_initial_state);
	}

	std::ranges::sort(shifted_rhs_initials);
	for (const State lhs_final_state : lhs_final_states) {
		State source{lhs_final_state};
		Level level{result.levels[source]};
		for (size_t level_index{1}; level_index < num_of_levels; ++level_index) {
			level = result.levels.next_level_after(level);
			const State inner_state{result.add_state_with_level(level)};
			result.delta.add(source, epsilon, inner_state);
			source = inner_state;
		}
		// Sorted above because SparseSet iteration is unordered; rhs.initial also makes the targets duplicate-free.
		result.delta.add(source, epsilon, std::span<const State>{shifted_rhs_initials});
	}

	// Make result final states, reusing the storage of the lhs final states, which spans the lhs states already.
	result.final = std::move(lhs_final_states);
	result.final.clear();
	for (const State rhs_final_state : rhs.final) {
		result.final.insert(static_cast<State>(lhs_states_num) + rhs_final_state);
	}
	if constexpr (consume_rhs) { rhs = Nft::with_levels(rhs_num_of_levels); }

	if (!use_epsilon) { result.remove_epsilon(epsilon); }
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

Nft& Nft::concatenate(const Nft& aut) & {
	MATA_ASSERT(levels.num_of_levels == aut.levels.num_of_levels);
	const size_t n = this->num_of_states();
	const size_t aut_n = aut.num_of_states();
	nfa::check_num_of_states(n + aut_n);
	auto upd_fnc = [&](const State st) { return static_cast<State>(st + n); };

	// copy the information about aut to save the case when this is the same object as aut.
	const utils::SparseSet<mata::nft::State> aut_initial = aut.initial;
	const utils::SparseSet<mata::nft::State> aut_final = aut.final;

	this->delta.allocate(n);
	this->delta.append_shifted(aut.delta, static_cast<State>(n));
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

Nft& Nft::concatenate(Nft&& aut) & {
	MATA_ASSERT(levels.num_of_levels == aut.levels.num_of_levels);
	if (this == &aut) {
		// Self-concatenation: one controlled snapshot of the operand; `this` keeps its own content.
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
		const Nft snapshot{aut};
		return concatenate(snapshot);
	}

	const size_t n = this->num_of_states();
	const size_t aut_n = aut.num_of_states();
	nfa::check_num_of_states(n + aut_n);
	auto upd_fnc = [&](const State st) { return static_cast<State>(st + n); };

	const utils::SparseSet<State> aut_initial{std::move(aut.initial)};
	const utils::SparseSet<State> aut_final{std::move(aut.final)};

	this->delta.allocate(n);
	this->delta.append_shifted(std::move(aut.delta), static_cast<State>(n));
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
	// The consuming path fills the requested renaming maps also for the empty result.
	if (concatenation_is_empty(lhs, rhs)) {
		// Do not clone a possibly large lhs only to throw it away: fill the requested maps from the original counts.
		// The combined state count must be representable, as on the consuming path, whichever maps are requested.
		if (nfa::Limits::max_num_of_states - lhs.num_of_states() < rhs.num_of_states()) { nfa::throw_state_overflow(); }
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
		return Nft::with_levels(lhs.levels.num_of_levels);
	}
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
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
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
		if (power & 1U) { result.concatenate(base); }

		// Shift to the next bit.
		power >>= 1;

		// If there are still bits to process, square `base` (i.e. base = base * base).
		if (power) { base.concatenate(base); }
	}

	return result;
}

} // Namespace mata::nft.
