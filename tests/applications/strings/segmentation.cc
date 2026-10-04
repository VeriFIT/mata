/* tests-nfa-segmentation.cc -- Tests for segmentation of NFAs
 */

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
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

TEST_CASE("mata::nfa::Segmentation keeps exactly one copy of every epsilon transition - issue #795") {
	SECTION("no epsilon transitions") {
		Nfa aut{3};
		aut.initial = {0};
		aut.final = {2};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'b', 2);

		seg_nfa::Segmentation segmentation{aut, {EPSILON}};
		CHECK(segmentation.get_epsilon_depths().empty());
		CHECK(!segmentation.get_epsilon_transitions(0).has_value());
		CHECK(!segmentation.get_epsilon_transitions(1).has_value());
	}

	SECTION("one epsilon transition at depth zero") {
		Nfa aut{3};
		aut.initial = {0};
		aut.final = {2};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, EPSILON, 2);

		seg_nfa::Segmentation segmentation{aut, {EPSILON}};
		const auto& depths{segmentation.get_epsilon_depths()};
		REQUIRE(depths.size() == 1);
		REQUIRE(depths.count(0) == 1);
		REQUIRE(depths.at(0).size() == 1);
		CHECK(depths.at(0)[0] == Transition{1, EPSILON, 2});

		const auto state_1_transitions{segmentation.get_epsilon_transitions(1)};
		REQUIRE(state_1_transitions.has_value());
		CHECK(std::distance(state_1_transitions->first, state_1_transitions->second) == 1);
		CHECK(*state_1_transitions->first == Transition{1, EPSILON, 2});
		CHECK(!segmentation.get_epsilon_transitions(0).has_value());
		CHECK(!segmentation.get_epsilon_transitions(2).has_value());
	}

	SECTION("several epsilon transitions of one state are consecutive at one depth") {
		Nfa aut{5};
		aut.initial = {0};
		aut.final = {4};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, EPSILON, 2);
		aut.delta.add(1, EPSILON, 3);
		aut.delta.add(2, 'b', 4);
		aut.delta.add(3, 'b', 4);

		seg_nfa::Segmentation segmentation{aut, {EPSILON}};
		REQUIRE(segmentation.get_epsilon_depths().size() == 1);
		const auto state_1_transitions{segmentation.get_epsilon_transitions(1)};
		REQUIRE(state_1_transitions.has_value());
		REQUIRE(std::distance(state_1_transitions->first, state_1_transitions->second) == 2);
		CHECK(state_1_transitions->first[0] == Transition{1, EPSILON, 2});
		CHECK(state_1_transitions->first[1] == Transition{1, EPSILON, 3});
		CHECK(!segmentation.get_epsilon_transitions(2).has_value());
		CHECK(!segmentation.get_epsilon_transitions(3).has_value());
	}

	SECTION("several epsilon symbols from one state form one span") {
		Nfa aut{4};
		aut.initial = {0};
		aut.final = {3};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, EPSILON, 2);
		aut.delta.add(1, EPSILON - 1, 3);

		seg_nfa::Segmentation segmentation{aut, {EPSILON, EPSILON - 1}};
		REQUIRE(segmentation.get_epsilon_depths().size() == 1);
		const auto state_1_transitions{segmentation.get_epsilon_transitions(1)};
		REQUIRE(state_1_transitions.has_value());
		REQUIRE(std::distance(state_1_transitions->first, state_1_transitions->second) == 2);
		// Symbol posts are processed in symbol order: EPSILON - 1 first, EPSILON second.
		CHECK(state_1_transitions->first[0] == Transition{1, EPSILON - 1, 3});
		CHECK(state_1_transitions->first[1] == Transition{1, EPSILON, 2});
	}
}
