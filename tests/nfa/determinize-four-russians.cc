/** @file
 * @brief Tests for the "Four Russians" subset construction (mata::nfa::algorithms::determinize_four_russians())
 *  with its block sizes and reductions, and for the choice of the algorithm in mata::nfa::determinize().
 */

#include <algorithm>
#include <initializer_list>
#include <random>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using namespace mata::nfa::algorithms;
using mata::Symbol;

namespace {

constexpr unsigned BLOCK_SIZES[]{0, 1, 2, 4, 8, 16, 32, 64};

Nfa random_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const std::vector<Symbol>& symbols,
	const size_t num_of_transitions,
	const size_t num_of_initial,
	const size_t num_of_final
) {
	Nfa aut(num_of_states);
	std::uniform_int_distribution<State> state_dist(0, static_cast<State>(num_of_states - 1));
	std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
	for (size_t i = 0; i < num_of_transitions; ++i) {
		aut.delta.add(state_dist(rng), symbols[symbol_dist(rng)], state_dist(rng));
	}
	for (size_t i = 0; i < num_of_initial; ++i) { aut.initial.insert(state_dist(rng)); }
	for (size_t i = 0; i < num_of_final; ++i) { aut.final.insert(state_dist(rng)); }
	return aut;
}

/// A random NFA whose symbols come in @p num_of_groups groups of @p copies symbols with the same transitions (one extra
///  transition splits the first group).
Nfa random_grouped_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const Symbol num_of_groups,
	const Symbol copies,
	const size_t num_of_transitions
) {
	Nfa aut(num_of_states);
	std::uniform_int_distribution<State> state_dist(0, static_cast<State>(num_of_states - 1));
	std::uniform_int_distribution<Symbol> group_dist(0, num_of_groups - 1);
	for (size_t i = 0; i < num_of_transitions; ++i) {
		const State source = state_dist(rng);
		const Symbol group = group_dist(rng);
		const State target = state_dist(rng);
		for (Symbol copy = 0; copy < copies; ++copy) { aut.delta.add(source, 100 * group + copy, target); }
	}
	aut.delta.add(state_dist(rng), 0, state_dist(rng));
	for (size_t i = 0; i < 3; ++i) { aut.initial.insert(state_dist(rng)); }
	for (size_t i = 0; i < num_of_states / 3 + 1; ++i) { aut.final.insert(state_dist(rng)); }
	return aut;
}

/// (a|b)* a (a|b)^k: the minimal DFA has 2^(k+1) states and so has the subset construction.
Nfa exponential_nfa(const size_t k) {
	Nfa aut(k + 2);
	aut.initial.insert(0);
	aut.final.insert(static_cast<State>(k + 1));
	aut.delta.add(0, 'a', 0);
	aut.delta.add(0, 'b', 0);
	aut.delta.add(0, 'a', 1);
	for (State q = 1; q <= k; ++q) {
		aut.delta.add(q, 'a', q + 1);
		aut.delta.add(q, 'b', q + 1);
	}
	return aut;
}

/// exponential_nfa(k) with @p fan extra transitions per chain state to sinks, so that its macrostates spread over
///  several words unless the construction renumbers the states.
Nfa fanned_exponential_nfa(const size_t k, const State fan) {
	const State stride = fan + 1;
	Nfa aut((k + 2) * stride);
	aut.initial.insert(0);
	aut.final.insert(static_cast<State>((k + 1) * stride));
	aut.delta.add(0, 'a', 0);
	aut.delta.add(0, 'b', 0);
	aut.delta.add(0, 'a', stride);
	for (State q = 1; q <= k; ++q) {
		aut.delta.add(q * stride, 'a', (q + 1) * stride);
		aut.delta.add(q * stride, 'b', (q + 1) * stride);
		for (State sink = 1; sink <= fan; ++sink) { aut.delta.add(q * stride, 'c', q * stride + sink); }
	}
	return aut;
}

/// (a|b)* a (a|b)^k plus a disjoint initial cycle of @p padding states: large macrostates whose block patterns repeat.
Nfa padded_exponential_nfa(const State k, const State padding) {
	Nfa aut(k + 2 + padding);
	aut.initial.insert(0);
	aut.delta.add(0, 'a', 0);
	aut.delta.add(0, 'b', 0);
	aut.delta.add(0, 'a', 1);
	for (State q = 1; q <= k; ++q) {
		aut.delta.add(q, 'a', q + 1);
		aut.delta.add(q, 'b', q + 1);
	}
	aut.final.insert(k + 1);
	for (State i = 0; i < padding; ++i) {
		const State q = k + 2 + i;
		aut.initial.insert(q);
		if (i % 7 == 0) { aut.final.insert(q); }
		aut.delta.add(q, 'a', k + 2 + (i + 1) % padding);
		aut.delta.add(q, 'b', k + 2 + (i + 1) % padding);
	}
	return aut;
}

std::vector<State> sorted_states(const mata::utils::SparseSet<State>& states) {
	std::vector<State> result(states.begin(), states.end());
	std::sort(result.begin(), result.end());
	return result;
}

/// All transitions of @p aut in the order of its Delta (SymbolPost::operator== compares the symbols only).
std::vector<std::tuple<State, Symbol, State>> transitions_of(const Nfa& aut) {
	std::vector<std::tuple<State, Symbol, State>> result;
	for (const Transition& transition : aut.delta.transitions()) {
		result.emplace_back(transition.source, transition.symbol, transition.target);
	}
	return result;
}

/// Checks that determinize_four_russians() with @p block_size gives @p classic, the result of determinize_classical()
///  with the subset map @p classic_map, up to the numbering of the states.
void check_same_subset_construction(
	const Nfa& aut,
	const unsigned block_size,
	const Nfa& classic,
	const std::unordered_map<StateSet, State>& classic_map
) {
	std::unordered_map<StateSet, State> four_russians_map;
	const Nfa four_russians = determinize_four_russians(aut, {.block_size = block_size}, &four_russians_map);

	REQUIRE(four_russians.num_of_states() == classic.num_of_states());
	REQUIRE(four_russians_map.size() == classic_map.size());
	CHECK(four_russians.delta.num_of_transitions() == classic.delta.num_of_transitions());
	CHECK(four_russians.final.size() == classic.final.size());
	CHECK(four_russians.initial.size() == 1);
	CHECK(four_russians.is_deterministic());

	// Mismatches are counted, not checked one by one: the automata have up to tens of thousands of transitions.
	std::vector<State> classic_to_four_russians(classic.num_of_states(), 0);
	size_t num_of_missing_sets = 0;
	size_t num_of_different_states = 0;
	for (const auto& [subset, classic_state] : classic_map) {
		const auto it = four_russians_map.find(subset);
		if (it == four_russians_map.end()) {
			++num_of_missing_sets;
			continue;
		}
		classic_to_four_russians[classic_state] = it->second;
		const bool same_final = classic.final.contains(classic_state) == four_russians.final.contains(it->second);
		const bool same_initial = classic.initial.contains(classic_state) == four_russians.initial.contains(it->second);
		if (!same_final || !same_initial) { ++num_of_different_states; }
	}
	REQUIRE(num_of_missing_sets == 0);
	CHECK(num_of_different_states == 0);
	size_t num_of_missing_transitions = 0;
	for (const Transition& transition : classic.delta.transitions()) {
		if (!four_russians.delta.contains(
				classic_to_four_russians[transition.source], transition.symbol,
				classic_to_four_russians[transition.target]
			)) {
			++num_of_missing_transitions;
		}
	}
	CHECK(num_of_missing_transitions == 0);
}

/// Checks that determinize_four_russians() with @p block_size gives the DFA and the subset map of
///  determinize_classical(), up to the numbering of the states.
void check_same_subset_construction(const Nfa& aut, const unsigned block_size) {
	std::unordered_map<StateSet, State> classic_map;
	const Nfa classic = determinize_classical(aut, &classic_map);
	check_same_subset_construction(aut, block_size, classic, classic_map);
}

/// Checks check_same_subset_construction() for every block size.
void check_same_subset_construction_for_all_block_sizes(const Nfa& aut) {
	std::unordered_map<StateSet, State> classic_map;
	const Nfa classic = determinize_classical(aut, &classic_map);
	for (const unsigned block_size : BLOCK_SIZES) {
		check_same_subset_construction(aut, block_size, classic, classic_map);
	}
}

} // namespace

TEST_CASE("mata::nfa::algorithms::determinize_four_russians()") {
	Nfa aut(3);
	Nfa result;
	std::unordered_map<StateSet, State> subset_map;

	SECTION("empty automaton") {
		for (const unsigned block_size : BLOCK_SIZES) {
			result = determinize_four_russians(aut, {.block_size = block_size});
			REQUIRE(result.final.empty());
			REQUIRE(result.delta.empty());
			CHECK(result.is_lang_empty());
		}
	}

	SECTION("simple automaton 1") {
		aut.initial = {1};
		aut.final = {1};
		result = determinize_four_russians(aut, {}, &subset_map);

		REQUIRE(result.initial[subset_map[{1}]]);
		REQUIRE(result.final[subset_map[{1}]]);
		REQUIRE(result.delta.empty());
	}

	SECTION("simple automaton 2") {
		aut.initial = {1};
		aut.final = {2};
		aut.delta.add(1, 'a', 2);
		result = determinize_four_russians(aut, {}, &subset_map);

		REQUIRE(result.initial[subset_map[{1}]]);
		REQUIRE(result.final[subset_map[{2}]]);
		REQUIRE(result.delta.contains(subset_map[{1}], 'a', subset_map[{2}]));
	}

	SECTION("no initial states") {
		aut.final = {2};
		aut.delta.add(1, 'a', 2);
		for (const unsigned block_size : BLOCK_SIZES) {
			result = determinize_four_russians(aut, {.block_size = block_size});
			CHECK(result.num_of_states() == 1);
			CHECK(result.is_lang_empty());
			check_same_subset_construction(aut, block_size);
		}
	}

	SECTION("invalid block size") {
		CHECK_THROWS_AS(determinize_four_russians(aut, {.block_size = 3}), std::invalid_argument);
		CHECK_THROWS_AS(determinize_four_russians(aut, {.block_size = 128}), std::invalid_argument);
	}

	SECTION("exponential blow-up") {
		for (const size_t k : std::vector<size_t>{0, 1, 5, 9, 12}) {
			const Nfa nfa = exponential_nfa(k);
			for (const unsigned block_size : BLOCK_SIZES) {
				result = determinize_four_russians(nfa, {.block_size = block_size});
				CHECK(result.num_of_states() == (size_t{1} << (k + 1)));
				CHECK(result.is_deterministic());
				if (k <= 9) { CHECK(are_equivalent(result, nfa)); }
			}
		}
		check_same_subset_construction(exponential_nfa(10), 0);
		check_same_subset_construction(exponential_nfa(10), 16);
	}

	SECTION("macrostate_discover callback") {
		const Nfa nfa = exponential_nfa(6);
		for (const unsigned block_size : BLOCK_SIZES) {
			subset_map.clear();
			std::vector<StateSet> discovered;
			result = determinize_four_russians(
				nfa, {.block_size = block_size}, &subset_map,
				[&](const Nfa& partial, const State state, const StateSet& macrostate) {
					CHECK(state < partial.num_of_states());
					CHECK(state == discovered.size());
					discovered.push_back(macrostate);
					return true;
				}
			);
			REQUIRE(discovered.size() == result.num_of_states());
			for (State state = 0; state < discovered.size(); ++state) {
				CHECK(subset_map.at(discovered[state]) == state);
			}

			// Stop after the 10th discovered macrostate.
			size_t calls = 0;
			result = determinize_four_russians(
				nfa, {.block_size = block_size}, nullptr,
				[&](const Nfa&, const State, const StateSet&) { return ++calls < 10; }
			);
			CHECK(calls == 10);
			CHECK(result.num_of_states() == 10);
		}
	}

	SECTION("renumbering the NFA states for locality keeps the subset map and the callback in the input's states") {
		for (const auto& [k, fan] : std::vector<std::pair<size_t, State>>{{12, 9}, {13, 20}}) {
			const Nfa nfa = fanned_exponential_nfa(k, fan);
			for (const unsigned block_size : std::vector<unsigned>{0, 1, 8, 64}) {
				check_same_subset_construction(nfa, block_size);
			}
			subset_map.clear();
			std::vector<StateSet> discovered;
			size_t num_of_wrong_states = 0;
			result = determinize_four_russians(
				nfa, {}, &subset_map, [&](const Nfa&, const State state, const StateSet& macrostate) {
					if (state != discovered.size()) { ++num_of_wrong_states; }
					discovered.push_back(macrostate);
					return true;
				}
			);
			REQUIRE(discovered.size() == result.num_of_states());
			REQUIRE(result.num_of_states() == determinize_classical(nfa).num_of_states());
			const auto final_state = static_cast<State>((k + 1) * (fan + 1));
			for (State state = 0; state < discovered.size(); ++state) {
				if (subset_map.at(discovered[state]) != state ||
					result.final.contains(state) != discovered[state].contains(final_state)) {
					++num_of_wrong_states;
				}
			}
			CHECK(num_of_wrong_states == 0);
		}
	}

	SECTION("Block size 4") {
		Nfa nfa(6);
		nfa.initial.insert(0);
		nfa.final.insert(5);
		for (const State target : std::initializer_list<State>{0, 2, 3, 5}) { nfa.delta.add(0, 'a', target); }
		for (const State target : std::initializer_list<State>{2, 3}) { nfa.delta.add(1, 'a', target); }
		for (const State target : std::initializer_list<State>{0, 1}) { nfa.delta.add(4, 'a', target); }
		nfa.delta.add(5, 'a', 4);
		for (const State target : std::initializer_list<State>{2, 3}) {
			nfa.delta.add(5, 'b', target);
			nfa.delta.add(5, 'c', target);
		}
		result = determinize_four_russians(nfa, {.block_size = 4}, &subset_map);
		const std::vector<StateSet> macrostates = {{0}, {0, 2, 3, 5}, {0, 2, 3, 4, 5}, {2, 3}, {0, 1, 2, 3, 4, 5}};
		REQUIRE(result.num_of_states() == macrostates.size());
		for (State m = 0; m < macrostates.size(); ++m) { CHECK(subset_map.at(macrostates[m]) == m); }
		CHECK(result.final == mata::utils::SparseSet<State>{1, 2, 4});
		CHECK(result.delta.num_of_transitions() == 10);
		for (const auto& [source, symbol, target] : std::vector<std::tuple<State, Symbol, State>>{
				 {0, 'a', 1},
				 {1, 'a', 2},
				 {1, 'b', 3},
				 {1, 'c', 3},
				 {2, 'a', 4},
				 {2, 'b', 3},
				 {2, 'c', 3},
				 {4, 'a', 4},
				 {4, 'b', 3},
				 {4, 'c', 3}
			 }) {
			CHECK(result.delta.contains(source, symbol, target));
		}
		check_same_subset_construction(nfa, 4);
	}

	SECTION("alphabet compression agree with the classical subset construction") {
		std::mt19937 rng(7);
		for (size_t round = 0; round < 40; ++round) {
			// Groups of symbols that behave identically, plus symbols that agree only in some states.
			const size_t num_of_states = 2 + round % 14;
			std::uniform_int_distribution<State> state_dist(0, static_cast<State>(num_of_states - 1));
			Nfa nfa(num_of_states);
			nfa.initial.insert(0);
			nfa.final.insert(state_dist(rng));
			for (Symbol group = 0; group < 6; ++group) {
				const size_t group_size = 1 + (group * 7 + round) % 9;
				for (size_t i = 0; i < num_of_states + num_of_states / 2; ++i) {
					const State source = state_dist(rng);
					const State target = state_dist(rng);
					for (Symbol k = 0; k < group_size; ++k) { nfa.delta.add(source, group * 100 + k, target); }
				}
			}
			// Symbol 1000 + k copies group 0 except in one state, so it must end up in its own class.
			const std::vector<Transition> transitions(nfa.delta.transitions().begin(), nfa.delta.transitions().end());
			for (const Transition& transition : transitions) {
				if (transition.symbol == 0 && transition.source != 0) {
					nfa.delta.add(transition.source, 1'000, transition.target);
				}
			}
			check_same_subset_construction_for_all_block_sizes(nfa);
			// The discovery callback (which disables compression) must still see every macrostate.
			size_t calls = 0;
			const Nfa with_callback =
				determinize_four_russians(nfa, {}, nullptr, [&](const Nfa&, const State, const StateSet&) {
					++calls;
					return true;
				});
			CHECK(calls == with_callback.num_of_states());
			CHECK(with_callback.num_of_states() == determinize_classical(nfa).num_of_states());
		}
	}

	SECTION("the largest symbols") {
		// The size of the bit vector of the used symbols must not overflow for them when size_t has 32 bits.
		Nfa nfa(3);
		nfa.initial.insert(0);
		nfa.final.insert(2);
		for (const Symbol symbol : {Limits::max_symbol - 1, Limits::max_symbol}) {
			nfa.delta.add(0, symbol, 1);
			nfa.delta.add(0, symbol, 2);
			nfa.delta.add(1, symbol, 2);
		}
		check_same_subset_construction_for_all_block_sizes(nfa);
	}

	SECTION("deterministic automata (copied without a subset construction)") {
		// A deterministic input is copied, with the states numbered as the subset construction (forced by a discovery
		//  callback) numbers them.
		std::mt19937 rng(11);
		const std::vector<Symbol> alphabet = {0, 1, 7, 100, 1'000, 65'535, 4'000'000'000U};
		for (size_t round = 0; round < 30; ++round) {
			const size_t num_of_states = 1 + round * 7;
			std::uniform_int_distribution<State> state_dist(0, static_cast<State>(num_of_states - 1));
			std::bernoulli_distribution has_transition(0.6);
			Nfa nfa(num_of_states);
			nfa.initial.insert(state_dist(rng));
			for (State q = 0; q < num_of_states; ++q) {
				if (q % 3 == 0) { nfa.final.insert(q); }
				for (const Symbol symbol : alphabet) {
					if (has_transition(rng)) { nfa.delta.add(q, symbol, state_dist(rng)); }
				}
			}
			REQUIRE(nfa.is_deterministic());
			check_same_subset_construction_for_all_block_sizes(nfa);

			std::unordered_map<StateSet, State> map;
			std::unordered_map<StateSet, State> constructed_map;
			result = determinize_four_russians(nfa, {}, &map);
			const Nfa constructed =
				determinize_four_russians(nfa, {}, &constructed_map, [](const Nfa&, const State, const StateSet&) {
					return true;
				});
			REQUIRE(result.num_of_states() == constructed.num_of_states());
			CHECK(map == constructed_map);
			CHECK(result.initial == constructed.initial);
			CHECK(result.final == constructed.final);
			CHECK(transitions_of(result) == transitions_of(constructed));
		}
		Nfa one_state(1);
		one_state.initial.insert(0);
		CHECK_THROWS_AS(determinize_four_russians(one_state, {.block_size = 3}), std::invalid_argument);
	}

	SECTION("random automata agree with the classical subset construction") {
		std::mt19937 rng(42);
		const std::vector<Symbol> small_alphabet = {'a', 'b'};
		const std::vector<Symbol> wide_alphabet = {0, 1, 7, 100, 1'000, 65'535, 4'000'000'000U};
		for (const auto& [num_of_states, density] : std::vector<std::pair<size_t, double>>{
				 {1, 1.0}, {5, 2.0}, {12, 1.5}, {20, 1.25}, {40, 1.5}, {70, 1.3}, {130, 1.2}, {300, 1.1}, {600, 1.05}
			 }) {
			for (const std::vector<Symbol>* alphabet : {&small_alphabet, &wide_alphabet}) {
				for (size_t round = 0; round < 3; ++round) {
					const size_t num_of_transitions =
						static_cast<size_t>(density * static_cast<double>(num_of_states * alphabet->size()));
					const Nfa nfa =
						random_nfa(rng, num_of_states, *alphabet, num_of_transitions, 1 + round, 1 + num_of_states / 4);
					size_t num_of_macrostates = 0;
					determinize_classical(nfa, nullptr, [&](const Nfa&, const State, const StateSet&) {
						return ++num_of_macrostates <= 20'000;
					});
					if (num_of_macrostates > 20'000) { continue; }
					check_same_subset_construction_for_all_block_sizes(nfa);
				}
			}
		}
	}
}

TEST_CASE("mata::nfa::algorithms::determinize_four_russians() numbers the states independently of the block size") {
	// Every block size gives the same DFA, numbering and subset map, with and without a discovery callback.
	std::mt19937 rng(19);
	std::vector<Nfa> automata;
	for (const size_t num_of_states : std::initializer_list<size_t>{6, 40, 150, 1'200}) {
		for (size_t round = 0; round < 3; ++round) {
			automata.push_back(random_grouped_nfa(rng, num_of_states, 3, 3, num_of_states + num_of_states / 2));
		}
	}
	automata.push_back(padded_exponential_nfa(8, 100));
	const auto keep_going = [](const Nfa&, const State, const StateSet&) { return true; };
	for (const Nfa& aut : automata) {
		std::unordered_map<StateSet, State> expected_map;
		const Nfa expected = determinize_four_russians(aut, {}, &expected_map);
		for (const unsigned block_size : BLOCK_SIZES) {
			for (const bool with_callback : {false, true}) {
				std::unordered_map<StateSet, State> map;
				const Nfa dfa = with_callback
									? determinize_four_russians(aut, {.block_size = block_size}, &map, keep_going)
									: determinize_four_russians(aut, {.block_size = block_size}, &map);
				CHECK(dfa.num_of_states() == expected.num_of_states());
				CHECK(sorted_states(dfa.initial) == sorted_states(expected.initial));
				CHECK(sorted_states(dfa.final) == sorted_states(expected.final));
				CHECK(transitions_of(dfa) == transitions_of(expected));
				CHECK(map == expected_map);
			}
		}
	}
}

TEST_CASE("mata::nfa::algorithms::determinize_four_russians() with a language-preserving reduction") {
	using Reduction = FourRussiansReduction;
	const auto determinize_reduced = [](const Nfa& aut, const Reduction reduction, const unsigned block_size = 0,
										std::unordered_map<StateSet, State>* subset_map = nullptr) {
		return determinize_four_russians(aut, {.block_size = block_size, .reduction = reduction}, subset_map);
	};
	constexpr Reduction REDUCTIONS[]{Reduction::Bisimulation, Reduction::Simulation, Reduction::Adaptive};

	SECTION("empty language") {
		for (const Reduction reduction : REDUCTIONS) {
			Nfa aut(3);
			CHECK(determinize_reduced(aut, reduction).is_lang_empty());
			aut.initial = {0};
			aut.final = {2};
			aut.delta.add(0, 'a', 1); // State 2 is unreachable, so no state is useful.
			const Nfa result = determinize_reduced(aut, reduction);
			CHECK(result.num_of_states() == 1);
			CHECK(result.is_lang_empty());
		}
	}

	// L(1) = a* is a subset of L(2) = (a|b)*: the macrostate {1, 2} of the subset construction becomes {2}.
	Nfa aut(3);
	aut.initial = {0};
	aut.final = {1, 2};
	aut.delta.add(0, 'a', 1);
	aut.delta.add(0, 'a', 2);
	aut.delta.add(0, 'b', 1);
	aut.delta.add(1, 'a', 1);
	aut.delta.add(2, 'a', 2);
	aut.delta.add(2, 'b', 2);
	REQUIRE(determinize_classical(aut).num_of_states() == 4);

	SECTION("a universal state is the whole macrostate") {
		// State 2 accepts all words over {a, b}: Reduction::Bisimulation finds it without the simulation.
		for (const Reduction reduction : REDUCTIONS) {
			std::unordered_map<StateSet, State> subset_map;
			const Nfa result = determinize_reduced(aut, reduction, 0, &subset_map);
			CHECK(result.num_of_states() == 3);
			CHECK(are_equivalent(result, aut));
			CHECK(subset_map.contains({2}));
		}
	}

	SECTION("a simulated state is pruned from its macrostates") {
		aut.delta.add(0, 'c', 0); // State 2 is no longer universal (no transition under c).
		REQUIRE(determinize_classical(aut).num_of_states() == 4);
		CHECK(determinize_reduced(aut, Reduction::Bisimulation).num_of_states() == 4); // No bisimilar states.
		std::unordered_map<StateSet, State> subset_map;
		const Nfa result = determinize_reduced(aut, Reduction::Simulation, 0, &subset_map);
		CHECK(result.num_of_states() == 3);
		CHECK(result.is_deterministic());
		CHECK(are_equivalent(result, aut));
		REQUIRE(subset_map.size() == 3);
		CHECK(subset_map.contains({0}));
		CHECK(subset_map.contains({1}));
		CHECK(subset_map.contains({2}));
	}

	SECTION("bisimilar states are merged") {
		// Two disjoint copies of (a|b)* a (a|b)^k, the second one with its states in reverse order: the result is the
		//  DFA of one copy.
		const size_t k = 5;
		const Nfa copy = exponential_nfa(k);
		const State last = 2 * k + 3;
		Nfa twice = copy;
		for (const Transition& transition : copy.delta.transitions()) {
			twice.delta.add(last - transition.source, transition.symbol, last - transition.target);
		}
		twice.initial.insert(last);
		twice.final.insert(last - (k + 1));
		REQUIRE(determinize_classical(twice).num_of_states() == (size_t{1} << (k + 1)));
		for (const Reduction reduction : REDUCTIONS) {
			std::unordered_map<StateSet, State> subset_map;
			const Nfa result = determinize_reduced(twice, reduction, 0, &subset_map);
			CHECK(result.num_of_states() == (size_t{1} << (k + 1)));
			CHECK(are_equivalent(result, copy));
			for (const auto& [macrostate, state] : subset_map) {
				// Each class of merged states stands for its smallest state, which is in the first copy.
				CHECK(std::ranges::all_of(macrostate, [&](const State q) { return q <= k + 1; }));
			}
		}
	}

	SECTION("exponential blow-up without bisimilar or simulated states") {
		for (const Reduction reduction : REDUCTIONS) {
			for (const unsigned block_size : BLOCK_SIZES) {
				const Nfa result = determinize_reduced(exponential_nfa(8), reduction, block_size);
				CHECK(result.num_of_states() == (size_t{1} << 9));
				CHECK(are_equivalent(result, exponential_nfa(8)));
			}
		}
	}

	SECTION("random automata keep their language with at most as many states") {
		std::mt19937 rng(5);
		const std::vector<Symbol> small_alphabet = {'a', 'b'};
		const std::vector<Symbol> wide_alphabet = {0, 1, 7, 100, 1'000, 65'535};
		for (const auto& [num_of_states, density] : std::vector<std::pair<size_t, double>>{
				 {1, 1.0}, {4, 2.0}, {8, 1.5}, {15, 1.3}, {30, 1.2}, {60, 1.1}, {120, 1.05}
			 }) {
			for (const std::vector<Symbol>* alphabet : {&small_alphabet, &wide_alphabet}) {
				for (size_t round = 0; round < 3; ++round) {
					const size_t num_of_transitions =
						static_cast<size_t>(density * static_cast<double>(num_of_states * alphabet->size()));
					Nfa nfa =
						random_nfa(rng, num_of_states, *alphabet, num_of_transitions, 1 + round, 1 + num_of_states / 4);
					if (round == 2) {
						// Symbols with the same transitions everywhere (one symbol class), and a copy of every state:
						//  bisimilar states.
						const std::vector<Transition> transitions(
							nfa.delta.transitions().begin(), nfa.delta.transitions().end()
						);
						for (const Transition& transition : transitions) {
							if (transition.symbol == alphabet->front()) {
								nfa.delta.add(transition.source, 12'345, transition.target);
							}
						}
						const Nfa original = nfa;
						const auto offset = static_cast<State>(num_of_states);
						for (const Transition& transition : original.delta.transitions()) {
							nfa.delta.add(transition.source + offset, transition.symbol, transition.target);
						}
						for (const State q : original.final) { nfa.final.insert(q + offset); }
						nfa.initial.insert(offset);
					}
					size_t num_of_macrostates = 0;
					const Nfa classic =
						determinize_classical(nfa, nullptr, [&](const Nfa&, const State, const StateSet&) {
							return ++num_of_macrostates <= 3'000;
						});
					if (num_of_macrostates > 3'000) { continue; }
					for (const unsigned block_size : {0U, 1U, 8U}) {
						size_t num_of_bisimulation_states = 0;
						size_t num_of_simulation_states = 0;
						for (const Reduction reduction : REDUCTIONS) {
							std::unordered_map<StateSet, State> subset_map;
							const Nfa result = determinize_reduced(nfa, reduction, block_size, &subset_map);
							CHECK(result.is_deterministic());
							CHECK(result.num_of_states() <= classic.num_of_states());
							CHECK(are_equivalent(result, classic));
							if (reduction == Reduction::Bisimulation) {
								num_of_bisimulation_states = result.num_of_states();
							} else if (reduction == Reduction::Simulation) {
								num_of_simulation_states = result.num_of_states();
								CHECK(num_of_simulation_states <= num_of_bisimulation_states);
							} else { // The result of one of them.
								const bool is_one_of_them = result.num_of_states() == num_of_bisimulation_states ||
															result.num_of_states() == num_of_simulation_states;
								CHECK(is_one_of_them);
							}
							REQUIRE(subset_map.size() == result.num_of_states());
							if (num_of_states > 30 || subset_map.size() > 200) { continue; }
							// Every macrostate (a set of states of the input) has the language of its state.
							for (const auto& [macrostate, state] : subset_map) {
								Nfa from_macrostate = nfa;
								from_macrostate.initial.clear();
								for (const State q : macrostate) { from_macrostate.initial.insert(q); }
								Nfa from_state = result;
								from_state.initial = {state};
								CHECK(are_equivalent(from_macrostate, from_state));
							}
						}
					}
				}
			}
		}
	}

	SECTION("the adaptive reduction switches to the simulation on large DFAs") {
		// Random automata with far more macrostates than the budget of Reduction::Adaptive, and smaller with the
		//  simulation.
		std::mt19937 rng(11);
		const std::vector<Symbol> alphabet = {'a', 'b'};
		size_t num_of_checked = 0;
		for (size_t round = 0; round < 300 && num_of_checked < 2; ++round) {
			const Nfa nfa = random_nfa(rng, 50, alphabet, 130, 1, 25);
			const Nfa bisimulation = determinize_reduced(nfa, Reduction::Bisimulation);
			if (bisimulation.num_of_states() < 6'000) { continue; }
			const Nfa simulation = determinize_reduced(nfa, Reduction::Simulation);
			if (simulation.num_of_states() == bisimulation.num_of_states()) { continue; }
			const Nfa adaptive = determinize_reduced(nfa, Reduction::Adaptive);
			CHECK(adaptive.num_of_states() == simulation.num_of_states());
			CHECK(adaptive.delta.num_of_transitions() == simulation.delta.num_of_transitions());
			++num_of_checked;
		}
		CHECK(num_of_checked == 2);
	}

	SECTION("invalid arguments") {
		for (const Reduction reduction : REDUCTIONS) {
			CHECK_THROWS_AS(determinize_reduced(exponential_nfa(2), reduction, 3), std::invalid_argument);
			CHECK_THROWS_AS(
				determinize_four_russians(
					exponential_nfa(2), {.reduction = reduction}, nullptr,
					[](const Nfa&, const State, const StateSet&) { return true; }
				),
				std::invalid_argument
			);
		}
	}
}

TEST_CASE("mata::nfa::algorithms::determinize_four_russians() over restricted symbols") {
	// The automaton over a and b, plus transitions over c that reach other macrostates.
	const Nfa without_c = padded_exponential_nfa(4, 10);
	Nfa aut = without_c;
	const auto num_of_states = static_cast<State>(aut.num_of_states());
	for (State q = 0; q < num_of_states; q += 3) { aut.delta.add(q, 'c', (q + 5) % num_of_states); }
	const mata::utils::OrdVector<Symbol> symbols = {'a', 'b'};
	const auto keep_going = [](const Nfa&, const State, const StateSet&) { return true; };

	for (const bool with_callback : {false, true}) {
		std::unordered_map<StateSet, State> map;
		std::unordered_map<StateSet, State> expected_map;
		const Nfa dfa = with_callback ? determinize_four_russians(aut, {}, &map, keep_going, &symbols)
									  : determinize_four_russians(aut, {}, &map, std::nullopt, &symbols);
		const Nfa expected = with_callback ? determinize_four_russians(without_c, {}, &expected_map, keep_going)
										   : determinize_four_russians(without_c, {}, &expected_map);
		CHECK(dfa.is_identical(expected));
		CHECK(map == expected_map);
	}
	constexpr FourRussiansOptions BISIMULATION = {.reduction = FourRussiansReduction::Bisimulation};
	CHECK(determinize_four_russians(aut, BISIMULATION, nullptr, std::nullopt, &symbols)
			  .is_identical(determinize_four_russians(without_c, BISIMULATION)));

	// The DFA of the classical construction over the same symbols.
	std::unordered_map<StateSet, State> classic_map;
	const Nfa classic = determinize_classical(aut, &classic_map, std::nullopt, &symbols);
	check_same_subset_construction(without_c, 0, classic, classic_map);

	// Without other symbols, the restriction changes nothing.
	CHECK(determinize_four_russians(without_c, {}, nullptr, std::nullopt, &symbols)
			  .is_identical(determinize_four_russians(without_c)));
}

TEST_CASE("mata::nfa::determinize() with the \"algorithm\" key") {
	const Nfa aut = padded_exponential_nfa(4, 10);
	std::unordered_map<StateSet, State> map;
	std::unordered_map<StateSet, State> expected_map;

	SECTION("four_russians, the default") {
		const Nfa expected = determinize_four_russians(aut, {}, &expected_map);
		CHECK(determinize(aut, &map).is_identical(expected));
		CHECK(map == expected_map);
		CHECK(determinize(aut, {{"algorithm", "four_russians"}}).is_identical(expected));
	}

	SECTION("classical") {
		const Nfa expected = determinize_classical(aut, &expected_map);
		CHECK(determinize(aut, &map, std::nullopt, nullptr, {{"algorithm", "classical"}}).is_identical(expected));
		CHECK(map == expected_map);
		CHECK(determinize(aut, {{"algorithm", "classical"}}).is_identical(expected));
	}

	SECTION("invalid parameters") {
		CHECK_THROWS_AS(determinize(aut, {{"algorithm", "subset"}}), std::runtime_error);
		CHECK_THROWS_AS(determinize(aut, {{"type", "classical"}}), std::runtime_error);
	}
}
