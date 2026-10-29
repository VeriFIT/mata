/* tests-nfa-segmentation.cc -- Tests for segmentation of NFAs
 */

#include <optional>
#include <random>
#include <type_traits>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using namespace mata::applications::strings;
using namespace mata::utils;
using Symbol = mata::Symbol;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x)                                                                                             \
	(x).initial = {1, 3};                                                                                              \
	(x).final = {5};                                                                                                   \
	(x).delta.add(1, 'a', 3);                                                                                          \
	(x).delta.add(1, 'a', 10);                                                                                         \
	(x).delta.add(1, 'b', 7);                                                                                          \
	(x).delta.add(3, 'a', 7);                                                                                          \
	(x).delta.add(3, 'b', 9);                                                                                          \
	(x).delta.add(9, 'a', 9);                                                                                          \
	(x).delta.add(7, 'b', 1);                                                                                          \
	(x).delta.add(7, 'a', 3);                                                                                          \
	(x).delta.add(7, 'c', 3);                                                                                          \
	(x).delta.add(10, 'a', 7);                                                                                         \
	(x).delta.add(10, 'b', 7);                                                                                         \
	(x).delta.add(10, 'c', 7);                                                                                         \
	(x).delta.add(7, 'a', 5);                                                                                          \
	(x).delta.add(5, 'a', 5);                                                                                          \
	(x).delta.add(5, 'c', 9);

// Automaton B
#define FILL_WITH_AUT_B(x)                                                                                             \
	(x).initialstates = {4};                                                                                           \
	(x).finalstates = {2, 12};                                                                                         \
	(x).delta.add(4, 'c', 8);                                                                                          \
	(x).delta.add(4, 'a', 8);                                                                                          \
	(x).delta.add(8, 'b', 4);                                                                                          \
	(x).delta.add(4, 'a', 6);                                                                                          \
	(x).delta.add(4, 'b', 6);                                                                                          \
	(x).delta.add(6, 'a', 2);                                                                                          \
	(x).delta.add(2, 'b', 2);                                                                                          \
	(x).delta.add(2, 'a', 0);                                                                                          \
	(x).delta.add(0, 'a', 2);                                                                                          \
	(x).delta.add(2, 'c', 12);                                                                                         \
	(x).delta.add(12, 'a', 14);                                                                                        \
	(x).delta.add(14, 'b', 12);

// }}}

TEST_CASE("mata::nfa::Segmentation::get_epsilon_depths()") {
	Nfa aut('q' + 1);
	constexpr Symbol epsilon{'c'};
	const std::set<Symbol> epsilons({epsilon});

	SECTION("Automaton A") {
		FILL_WITH_AUT_A(aut);
		auto segmentation{seg_nfa::Segmentation{aut, epsilons}};
		const auto& epsilon_depth_transitions{segmentation.get_epsilon_depths()};
		REQUIRE(
			epsilon_depth_transitions ==
			seg_nfa::Segmentation::EpsilonDepthTransitions{
				{0, std::vector<Transition>{{10, epsilon, 7}, {7, epsilon, 3}, {5, epsilon, 9}}}
			}
		);
	}

	SECTION("Small automaton with depths") {
		aut.initial.insert(1);
		aut.final.insert(8);
		aut.delta.add(1, epsilon, 2);
		aut.delta.add(2, 'a', 3);
		aut.delta.add(2, 'b', 4);
		aut.delta.add(3, 'b', 6);
		aut.delta.add(4, 'a', 6);
		aut.delta.add(6, epsilon, 7);
		aut.delta.add(7, epsilon, 8);

		auto segmentation{seg_nfa::Segmentation{aut, epsilons}};
		const auto& epsilon_depth_transitions{segmentation.get_epsilon_depths()};

		REQUIRE(
			epsilon_depth_transitions == seg_nfa::Segmentation::EpsilonDepthTransitions{
											 {0, std::vector<Transition>{{1, epsilon, 2}}},
											 {1, std::vector<Transition>{{6, epsilon, 7}}},
											 {2, std::vector<Transition>{{7, epsilon, 8}}},
										 }
		);
	}
}

TEST_CASE("mata::nfa::Segmentation::get_epsilon_transitions()") {
	constexpr Symbol epsilon{'c'};
	constexpr Symbol epsilon2{'d'};
	Nfa aut{};
	aut.initial = {1};
	aut.final = {6};
	aut.delta.add(1, epsilon, 2);
	aut.delta.add(1, epsilon, 3);
	aut.delta.add(1, epsilon2, 4);
	aut.delta.add(2, 'a', 5);
	aut.delta.add(5, epsilon, 6);
	const seg_nfa::Segmentation segmentation{aut, {epsilon, epsilon2}};

	CHECK(
		segmentation.get_epsilon_transitions_by_depth() == std::vector<std::vector<Transition>>{
															   {{1, epsilon, 2}, {1, epsilon, 3}, {1, epsilon2, 4}},
															   {{5, epsilon, 6}},
														   }
	);
	// All ε-transitions of one state, over several ε symbols, form one view.
	const auto state_1_transitions{segmentation.get_epsilon_transitions(1)};
	CHECK(
		std::vector<Transition>{state_1_transitions.begin(), state_1_transitions.end()} ==
		std::vector<Transition>{{1, epsilon, 2}, {1, epsilon, 3}, {1, epsilon2, 4}}
	);
	const auto state_5_transitions{segmentation.get_epsilon_transitions(5)};
	CHECK(
		std::vector<Transition>{state_5_transitions.begin(), state_5_transitions.end()} ==
		std::vector<Transition>{{5, epsilon, 6}}
	);
	CHECK(segmentation.get_epsilon_transitions(2).empty());
	CHECK(segmentation.get_epsilon_transitions(6).empty());
	CHECK(segmentation.get_epsilon_transitions(1'000).empty());
	CHECK(
		segmentation.get_epsilon_depths() ==
		seg_nfa::Segmentation::EpsilonDepthTransitions{
			{0, std::vector<Transition>{{1, epsilon, 2}, {1, epsilon, 3}, {1, epsilon2, 4}}},
			{1, std::vector<Transition>{{5, epsilon, 6}}},
		}
	);
}

TEST_CASE("mata::nfa::Segmentation copies and moves") {
	static_assert(std::is_copy_constructible_v<seg_nfa::Segmentation>);
	static_assert(std::is_move_constructible_v<seg_nfa::Segmentation>);
	constexpr Symbol epsilon{'c'};
	Nfa aut{};
	aut.initial = {1};
	aut.final = {6};
	aut.delta.add(1, epsilon, 2);
	aut.delta.add(1, epsilon, 3);
	aut.delta.add(2, 'a', 5);
	aut.delta.add(3, 'b', 5);
	aut.delta.add(5, epsilon, 6);
	const seg_nfa::Segmentation::EpsilonDepthTransitions expected_depths{
		{0, std::vector<Transition>{{1, epsilon, 2}, {1, epsilon, 3}}},
		{1, std::vector<Transition>{{5, epsilon, 6}}},
	};
	const std::vector<Transition> expected_state_1{{1, epsilon, 2}, {1, epsilon, 3}};

	// The source is destroyed before the copy is queried, so the copy must not refer to its storage.
	const auto check = [&](const bool query_source_first, const bool move) {
		CAPTURE(query_source_first, move);
		std::optional<seg_nfa::Segmentation> source{std::in_place, aut, std::set<Symbol>{epsilon}};
		if (query_source_first) {
			CHECK(source->get_epsilon_depths() == expected_depths);
			CHECK(source->get_segments().size() == 3);
		}
		seg_nfa::Segmentation copy{move ? seg_nfa::Segmentation{std::move(*source)} : seg_nfa::Segmentation{*source}};
		source.reset();
		CHECK(copy.get_epsilon_depths() == expected_depths);
		const auto state_1_transitions{copy.get_epsilon_transitions(1)};
		CHECK(std::vector<Transition>{state_1_transitions.begin(), state_1_transitions.end()} == expected_state_1);
		CHECK(copy.get_segments().size() == 3);
	};
	check(false, false);
	check(true, false);
	check(false, true);
	check(true, true);
}

TEST_CASE("mata::nfa::Segmentation::split_segment_automaton()") {
	Symbol epsilon{'c'};
	const std::set<Symbol> epsilons({epsilon});
	SECTION("Large automaton") {
		Nfa aut(100);
		aut.initial.insert(1);
		aut.final.insert(11);
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'b', 3);
		aut.delta.add(3, 'c', 4);
		aut.delta.add(4, 'a', 7);
		aut.delta.add(7, 'b', 8);
		aut.delta.add(8, 'a', 7);
		aut.delta.add(8, 'b', 4);
		aut.delta.add(4, 'c', 5);
		aut.delta.add(5, 'a', 6);
		aut.delta.add(5, 'b', 6);
		aut.delta.add(6, 'c', 10);
		aut.delta.add(9, 'a', 11);
		aut.delta.add(10, 'b', 11);

		auto segmentation{seg_nfa::Segmentation{aut, epsilons}};
		auto segments{segmentation.get_segments()};
		REQUIRE(segments.size() == 4);

		REQUIRE(segments[0].initial[0]);
		REQUIRE(segments[0].final[1]);
		REQUIRE(segments[0].delta.contains(0, 'b', 1));
		REQUIRE(!segments[0].delta.contains(0, 'a', 2));

		REQUIRE(segments[1].initial[0]);
		REQUIRE(segments[1].final[0]);
		REQUIRE(segments[1].delta.contains(0, 'a', 1));
		REQUIRE(!segments[1].delta.contains(0, 'a', 2));
		REQUIRE(!segments[1].delta.contains(0, 'c', 3));
		REQUIRE(segments[1].delta.contains(1, 'b', 2));
		REQUIRE(segments[1].delta.contains(2, 'b', 0));
		REQUIRE(segments[1].delta.contains(2, 'a', 1));

		REQUIRE(segments[2].initial[0]);
		REQUIRE(segments[2].final[1]);
		REQUIRE(segments[2].delta.contains(0, 'a', 1));
		REQUIRE(segments[2].delta.contains(0, 'b', 1));

		REQUIRE(segments[3].initial[0]);
		REQUIRE(segments[3].final[1]);
		REQUIRE(segments[3].delta.contains(0, 'b', 1));
	}

	SECTION("Correctly make states final and initial") {
		Nfa aut(100);
		aut.initial.insert(0);
		aut.final.insert({4, 6});
		aut.delta.add(0, epsilon, 2);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, epsilon, 3);
		aut.delta.add(3, 'b', 5);
		aut.delta.add(2, epsilon, 4);
		aut.delta.add(5, epsilon, 6);

		auto segmentation{seg_nfa::Segmentation{aut, epsilons}};
		auto segments{segmentation.get_segments()};
		CHECK(segments.size() == 3);

		CHECK(segments[0].initial.size() == 1);
		CHECK(segments[0].initial[0]);
		CHECK(segments[0].final.size() == 2);
		CHECK(segments[0].final[0]);
		CHECK(segments[0].final[1]);
		CHECK(segments[0].delta.num_of_transitions() == 1);
		CHECK(segments[0].delta.contains(0, 'a', 1));

		CHECK(segments[1].initial.size() == 2);
		CHECK(segments[1].initial[0]);
		CHECK(segments[1].initial[1]);
		CHECK(segments[1].final.size() == 2);
		CHECK(segments[1].final[0]);
		CHECK(segments[1].final[2]);
		CHECK(segments[1].delta.num_of_transitions() == 1);
		CHECK(segments[1].delta.contains(1, 'b', 2));

		CHECK(segments[2].initial.size() == 2);
		CHECK(segments[2].initial[0]);
		CHECK(segments[2].initial[1]);
		CHECK(segments[2].final.size() == 2);
		CHECK(segments[2].final[0]);
		CHECK(segments[2].final[1]);
		CHECK(segments[2].delta.num_of_transitions() == 0);
	}
}

namespace {
/// The untrimmed segments as built by copying @p aut whole per segment and removing the ε-transitions of the segment's
///  depth from it.
std::vector<Nfa> reference_untrimmed_segments(const Nfa& aut, const std::set<Symbol>& epsilons) {
	const seg_nfa::Segmentation segmentation{aut, epsilons};
	const std::vector<std::vector<Transition>>& by_depth{segmentation.get_epsilon_transitions_by_depth()};
	std::vector<Nfa> segments(by_depth.size() + 1, aut);
	for (size_t i{0}; i < segments.size(); ++i) {
		if (i != 0) { segments[i].initial.clear(); }
		if (i + 1 != segments.size()) { segments[i].final.clear(); }
	}
	for (size_t depth{0}; depth < by_depth.size(); ++depth) {
		for (const Transition& transition : by_depth[depth]) {
			segments[depth].final.insert(transition.source);
			segments[depth].delta.remove(transition);
			segments[depth + 1].initial.insert(transition.target);
		}
	}
	return segments;
}

std::vector<State> in_order(const SparseSet<State>& states) { return {states.begin(), states.end()}; }

/// Check that the segments of @p aut agree with @c reference_untrimmed_segments() on everything reachable.
void check_segments_match_reference(const Nfa& aut, const std::set<Symbol>& epsilons) {
	const std::vector<Nfa> expected{reference_untrimmed_segments(aut, epsilons)};
	seg_nfa::Segmentation segmentation{aut, epsilons};
	const std::vector<Nfa>& untrimmed{segmentation.get_untrimmed_segments()};
	REQUIRE(untrimmed.size() == expected.size());
	for (size_t i{0}; i < expected.size(); ++i) {
		CAPTURE(i);
		CHECK(in_order(untrimmed[i].initial) == in_order(expected[i].initial));
		CHECK(in_order(untrimmed[i].final) == in_order(expected[i].final));
		CHECK(untrimmed[i].num_of_states() == expected[i].num_of_states());
		const StateSet reachable{expected[i].get_reachable_states()};
		for (const State state : expected[i].get_reachable_states()) {
			CAPTURE(state);
			CHECK(untrimmed[i].delta[state] == expected[i].delta[state]);
		}
		// A segment holds only what its initial states reach: the posts of the other states must be empty, not the
		//  posts of a copy of the whole automaton without the ε-transitions.
		for (State state{0}; state < untrimmed[i].num_of_states(); ++state) {
			if (reachable.contains(state)) { continue; }
			CAPTURE(i, state);
			CHECK(untrimmed[i].delta[state].empty());
		}
	}
	const std::vector<Nfa>& trimmed{segmentation.get_segments()};
	REQUIRE(trimmed.size() == expected.size());
	for (size_t i{0}; i < expected.size(); ++i) {
		CAPTURE(i);
		CHECK(trimmed[i].is_identical(trim(expected[i])));
	}
}

/// A random untrimmed segment automaton: segments of random size with loops, dead ends, ε-transitions over
///  @p epsilons to the next segment (some parallel), plus states no initial state reaches, some with ε-transitions.
Nfa random_untrimmed_segment_automaton(
	const unsigned seed, const size_t num_of_segments, const std::vector<Symbol>& epsilons
) {
	std::mt19937 rng{seed};
	std::vector<State> segment_begin{0};
	for (size_t i{0}; i < num_of_segments; ++i) {
		segment_begin.push_back(segment_begin.back() + 2 + static_cast<State>(rng() % 7));
	}
	const State num_of_segment_states{segment_begin.back()};
	constexpr State num_of_unreachable{5};
	// Leave a few states without posts at the end.
	Nfa aut{num_of_segment_states + num_of_unreachable + 3};
	const auto random_state = [&](const size_t segment) {
		const State size{segment_begin[segment + 1] - segment_begin[segment]};
		return static_cast<State>(segment_begin[segment] + rng() % size);
	};
	for (size_t segment{0}; segment < num_of_segments; ++segment) {
		for (State state{segment_begin[segment]}; state < segment_begin[segment + 1]; ++state) {
			for (const Symbol symbol : {Symbol{'a'}, Symbol{'b'}}) {
				for (size_t i{0}; i < 2; ++i) {
					if (rng() % 3 == 0) { aut.delta.add(state, symbol, random_state(segment)); }
				}
			}
			for (size_t i{0}; segment + 1 < num_of_segments && i < 2; ++i) {
				if (rng() % 3 == 0) { continue; }
				const State target{random_state(segment + 1)};
				aut.delta.add(state, epsilons[rng() % epsilons.size()], target);
				if (rng() % 4 == 0) { aut.delta.add(state, epsilons[rng() % epsilons.size()], target); }
			}
		}
	}
	for (State state{num_of_segment_states}; state < num_of_segment_states + num_of_unreachable; ++state) {
		const size_t segment{rng() % num_of_segments};
		aut.delta.add(state, 'a', random_state(segment));
		if (segment + 1 < num_of_segments) { aut.delta.add(state, epsilons.front(), random_state(segment + 1)); }
	}
	for (size_t i{0}; i < 2; ++i) { aut.initial.insert(random_state(0)); }
	for (size_t i{0}; i < 3; ++i) { aut.final.insert(random_state(num_of_segments - 1)); }
	return aut;
}
} // namespace

TEST_CASE("mata::nfa::Segmentation segments match copying the whole automaton per segment") {
	SECTION("Handcrafted") {
		const Symbol epsilon{'c'};
		const std::set<Symbol> epsilons{epsilon};
		Nfa aut(100);
		aut.initial.insert(1);
		aut.final.insert(11);
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'b', 3);
		aut.delta.add(3, 'c', 4);
		aut.delta.add(4, 'a', 7);
		aut.delta.add(7, 'b', 8);
		aut.delta.add(8, 'a', 7);
		aut.delta.add(8, 'b', 4);
		aut.delta.add(4, 'c', 5);
		aut.delta.add(5, 'a', 6);
		aut.delta.add(5, 'b', 6);
		aut.delta.add(6, 'c', 10);
		aut.delta.add(9, 'a', 11);
		aut.delta.add(9, 'c', 4);
		aut.delta.add(10, 'b', 11);
		check_segments_match_reference(aut, epsilons);

		Nfa no_epsilon{};
		no_epsilon.initial = {0};
		no_epsilon.final = {1, 4};
		no_epsilon.delta.add(0, 'a', 1);
		no_epsilon.delta.add(2, 'c', 3);
		check_segments_match_reference(no_epsilon, epsilons);
	}

	SECTION("Random segment automata") {
		const std::vector<Symbol> epsilons{'x', 'y'};
		for (unsigned seed{0}; seed < 200; ++seed) {
			CAPTURE(seed);
			const size_t num_of_segments{1 + seed % 6};
			check_segments_match_reference(
				random_untrimmed_segment_automaton(seed, num_of_segments, epsilons), {epsilons.begin(), epsilons.end()}
			);
		}
	}

	SECTION("Random automata that are not segment automata") {
		// ε-transitions of other depths are reachable within a segment here.
		for (unsigned seed{0}; seed < 200; ++seed) {
			CAPTURE(seed);
			const Nfa aut{builder::create_random_nfa_tabakov_vardi(12, 3, 1.2, 0.3, seed)};
			check_segments_match_reference(aut, {0});
			check_segments_match_reference(aut, {0, 1});
		}
	}
}
