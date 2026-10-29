// TODO: some header

#include <algorithm>
#include <functional>
#include <random>
#include <ranges>
#include <set>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using namespace mata::applications::strings;
using namespace mata::utils;
using namespace mata::nfa::builder;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x)                                                                                             \
	(x).initialstates = {1, 3};                                                                                        \
	(x).finalstates = {5};                                                                                             \
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

template <class T> void unused(const T&) {}

TEST_CASE("mata::nfa::SegNfa::noodlify()") {
	Nfa aut{20};

	SECTION("Small automaton") {
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 'a', 1);

		Nfa noodle{2};
		noodle.initial.insert(0);
		noodle.delta.add(0, 'a', 1);
		noodle.final.insert(1);
		auto result{seg_nfa::noodlify(aut, 'c')};
		REQUIRE(result.size() == 1);
		REQUIRE(result[0].size() == 1);
		CHECK(are_equivalent(result[0][0].to_nfa(), noodle));

		auto result_segments{seg_nfa::noodlify_for_equation({aut}, aut)};
		REQUIRE(result_segments.size() == 1);
		REQUIRE(result_segments[0].size() == 1);
		CHECK(are_equivalent(result_segments[0][0].to_nfa(), noodle));
	}

	SECTION("1-2-3 epsilon transitions") {
		aut.initial.insert(0);
		aut.final.insert({4, 5, 6, 7});
		aut.delta.add(0, 'e', 1);
		aut.delta.add(1, 'e', 2);
		aut.delta.add(1, 'e', 3);
		aut.delta.add(2, 'e', 4);
		aut.delta.add(2, 'e', 5);
		aut.delta.add(2, 'e', 6);
		aut.delta.add(3, 'e', 7);

		auto noodles{seg_nfa::noodlify(aut, 'e')};

		CHECK(noodles.size() == 4);
	}

	SECTION("6-5-6 epsilon transitions") {
		aut.initial.insert({0, 1, 2});
		aut.final.insert({11, 12, 13, 14, 15, 16});
		aut.delta.add(0, 'e', 3);
		aut.delta.add(0, 'e', 4);
		aut.delta.add(0, 'e', 5);
		aut.delta.add(1, 'e', 3);
		aut.delta.add(1, 'e', 4);
		aut.delta.add(2, 'e', 5);

		aut.delta.add(3, 'e', 6);
		aut.delta.add(3, 'e', 7);
		aut.delta.add(4, 'e', 8);
		aut.delta.add(4, 'e', 9);
		aut.delta.add(5, 'e', 10);

		aut.delta.add(6, 'e', 11);
		aut.delta.add(7, 'e', 12);
		aut.delta.add(8, 'e', 13);
		aut.delta.add(8, 'e', 14);
		aut.delta.add(9, 'e', 15);
		aut.delta.add(10, 'e', 16);

		auto noodles{seg_nfa::noodlify(aut, 'e')};

		CHECK(noodles.size() == 12);
	}

	SECTION("1-2-3-3 epsilon transitions") {
		aut.initial.insert(0);
		aut.final.insert(7);
		aut.delta.add(0, 'e', 1);

		aut.delta.add(1, 'e', 2);
		aut.delta.add(1, 'e', 3);

		aut.delta.add(2, 'e', 4);
		aut.delta.add(3, 'e', 5);
		aut.delta.add(3, 'e', 6);

		aut.delta.add(4, 'e', 7);
		aut.delta.add(5, 'e', 7);
		aut.delta.add(6, 'e', 7);

		auto noodles{seg_nfa::noodlify(aut, 'e')};

		CHECK(noodles.size() == 3);
	}
}

TEST_CASE("mata::nfa::seg_nfa::segs_one_initial_final() keeps the language of reduce(trim()) per pair") {
	// Reference: one automaton per (initial, final) pair. The implementation shares one automaton per final state and
	//  gives each pair its state in it, so every pair must still have the language of its reference automaton.
	const State unused_state{1'000};
	const auto expected_segments = [&](const std::vector<Nfa>& segments, const bool include_empty) {
		std::map<std::pair<State, State>, Nfa> expected;
		// std::nullopt for init or fin stands for all initial or final states of the segment, with key unused_state.
		const auto add = [&](const Nfa& segment, const std::optional<State> init, const std::optional<State> fin) {
			const SparseSet<State> inits{init.value_or(0)};
			const SparseSet<State> fins{fin.value_or(0)};
			std::optional<std::reference_wrapper<const SparseSet<State>>> inits_ref, fins_ref;
			if (init) { inits_ref = inits; }
			if (fin) { fins_ref = fins; }
			if (Nfa reduced = reduce(trim(segment, nullptr, inits_ref, fins_ref));
				reduced.num_of_states() > 0 || include_empty) {
				expected[{init.value_or(unused_state), fin.value_or(unused_state)}] = std::move(reduced);
			}
		};
		for (size_t i{0}; i < segments.size(); ++i) {
			const Nfa& segment{segments[i]};
			if (i == 0) {
				for (const State fin : segment.final) { add(segment, std::nullopt, fin); }
			} else if (i + 1 == segments.size()) {
				for (const State init : segment.initial) { add(segment, init, std::nullopt); }
			} else {
				for (const State init : segment.initial) {
					for (const State fin : segment.final) { add(segment, init, fin); }
				}
			}
		}
		return expected;
	};

	for (unsigned seed{0}; seed < 40; ++seed) {
		std::mt19937 rng{seed};
		std::vector<Nfa> segments;
		for (size_t i{0}; i < 4; ++i) {
			Nfa segment{create_random_nfa_tabakov_vardi(15, 2, 1.5, 0.3, seed * 4 + static_cast<unsigned>(i))};
			segment.initial.clear();
			segment.final.clear();
			for (State q{0}; q < 15; ++q) {
				if (rng() % 3 == 0) { segment.initial.insert(q); }
				if (rng() % 3 == 0) { segment.final.insert(q); }
			}
			// Twins are simulation-equivalent to a smaller state, which a pair may reach without reaching the twin's
			//  original: the class representative of the shared simulation then lies outside of the pair.
			for (size_t twins{0}; twins < 4; ++twins) {
				const State original{static_cast<State>(rng() % 15)};
				const State twin{segment.add_state()};
				const StatePost original_post{segment.delta[original]};
				for (const SymbolPost& symbol_post : original_post) {
					for (const State target : symbol_post.targets) {
						segment.delta.add(twin, symbol_post.symbol, target);
					}
				}
				if (segment.final.contains(original)) { segment.final.insert(twin); }
				segment.delta.add(static_cast<State>(rng() % 15), static_cast<mata::Symbol>(rng() % 2), twin);
				if (rng() % 2 == 0) { segment.initial.insert(twin); }
			}
			segments.push_back(std::move(segment));
		}

		for (const bool include_empty : {false, true}) {
			const std::map<std::pair<State, State>, Nfa> expected{expected_segments(segments, include_empty)};
			std::map<std::pair<State, State>, seg_nfa::NoodleSegment> actual;
			seg_nfa::segs_one_initial_final(segments, include_empty, unused_state, actual);

			REQUIRE(actual.size() == expected.size());
			for (const auto& [key, expected_nfa] : expected) {
				INFO("seed " << seed << ", pair (" << key.first << ", " << key.second << ")");
				REQUIRE(actual.contains(key));
				const seg_nfa::NoodleSegment& segment{actual.at(key)};
				const Nfa segment_nfa{segment.to_nfa()};
				CHECK(segment_nfa.initial.size() == (expected_nfa.num_of_states() > 0 ? 1 : 0));
				CHECK(are_equivalent(segment_nfa, expected_nfa));
				CHECK(segment.is_epsilon_only() == (expected_nfa.num_of_states() > 0 && is_lang_eps(expected_nfa)));
			}

			// One automaton per final state of the first and the inner segments, one for the last segment and one
			//  for the empty pairs, instead of one per pair.
			std::unordered_set<const Nfa*> automata;
			for (const seg_nfa::NoodleSegment& segment : actual | std::views::values) {
				automata.insert(segment.automaton.get());
			}
			size_t num_of_finals{0};
			for (size_t i{0}; i + 1 < segments.size(); ++i) { num_of_finals += segments[i].final.size(); }
			CHECK(automata.size() <= num_of_finals + 2);
		}
	}
}

TEST_CASE("mata::nfa::SegNfa::noodlify_for_equation()") {
	SECTION("Empty input") {
		CHECK(seg_nfa::noodlify_for_equation(std::vector<std::reference_wrapper<Nfa>>{}, Nfa{}).empty());
	}

	SECTION("Empty left side") {
		Nfa right{1};
		right.initial.insert(0);
		right.final.insert(0);
		CHECK(seg_nfa::noodlify_for_equation(std::vector<std::reference_wrapper<Nfa>>{}, right).empty());
	}

	SECTION("Empty right side") {
		Nfa left{1};
		left.initial.insert(0);
		left.final.insert(0);
		CHECK(seg_nfa::noodlify_for_equation({left}, Nfa{}).empty());
	}

	SECTION("Small automata without initial and final states") {
		Nfa left{1};
		Nfa right{2};
		CHECK(seg_nfa::noodlify_for_equation({left}, right).empty());
	}

	SECTION("Small automata") {
		Nfa left1{1};
		left1.initial.insert(0);
		left1.final.insert(0);
		Nfa left2{1};
		left2.initial.insert(0);
		left2.final.insert(0);
		Nfa right{2};
		right.initial.insert(0);
		right.final.insert(0);

		Nfa noodle{2};
		noodle.initial.insert(0);
		noodle.delta.add(0, 0, 1);
		noodle.final.insert(1);
		auto result{seg_nfa::noodlify_for_equation({left1, left2}, right)};
		REQUIRE(result.size() == 1);
	}

	SECTION("Larger automata") {
		Nfa left1{2};
		left1.initial.insert(0);
		left1.final.insert(1);
		left1.delta.add(0, 'a', 1);
		Nfa left2{2};
		left2.initial.insert(0);
		left2.final.insert(1);
		left2.delta.add(0, 'b', 1);
		Nfa right_side{3};
		right_side.initial.insert(0);
		right_side.delta.add(0, 'a', 1);
		right_side.delta.add(1, 'b', 2);
		right_side.final.insert(2);

		Nfa noodle{4};
		noodle.initial.insert(0);
		noodle.final.insert(3);
		noodle.delta.add(0, 'a', 1);
		noodle.delta.add(1, 'c', 2); // The automatically chosen epsilon symbol (one larger than 'b').
		noodle.delta.add(2, 'b', 3);
		auto result{seg_nfa::noodlify_for_equation({left1, left2}, right_side)};
		REQUIRE(result.size() == 1);
		// CHECK(are_equivalent(result[0], noodle));
	}

	SECTION("Single noodle") {
		Nfa left{10};
		left.initial.insert(0);
		left.final.insert(9);
		left.delta.add(0, 108, 1);
		left.delta.add(1, 111, 2);
		left.delta.add(2, 99, 3);
		left.delta.add(3, 97, 4);
		left.delta.add(4, 108, 5);
		left.delta.add(5, 104, 6);
		left.delta.add(6, 111, 7);
		left.delta.add(7, 115, 8);
		left.delta.add(8, 116, 9);

		Nfa right_side{1};
		right_side.initial.insert(0);
		right_side.final.insert(0);
		right_side.delta.add(0, 44, 0);
		right_side.delta.add(0, 47, 0);
		right_side.delta.add(0, 58, 0);
		right_side.delta.add(0, 85, 0);
		right_side.delta.add(0, 90, 0);
		right_side.delta.add(0, 97, 0);
		right_side.delta.add(0, 99, 0);
		right_side.delta.add(0, 104, 0);
		right_side.delta.add(0, 108, 0);
		right_side.delta.add(0, 111, 0);
		right_side.delta.add(0, 115, 0);
		right_side.delta.add(0, 116, 0);
		right_side.delta.add(0, 117, 0);
		right_side.delta.add(0, 122, 0);

		auto result{seg_nfa::noodlify_for_equation({left}, right_side)};
		REQUIRE(result.size() == 1);
		REQUIRE(result[0].size() == 1);
		CHECK(are_equivalent(result[0][0].to_nfa(), left));
	}

	SECTION("Larger automata with separate noodles") {
		Nfa left1{3};
		left1.initial.insert(0);
		left1.final.insert({1, 2});
		left1.delta.add(0, 'a', 1);
		left1.delta.add(0, 'b', 2);
		Nfa left2{2};
		left2.initial.insert(0);
		left2.final.insert(1);
		left2.delta.add(0, 'a', 1);
		Nfa left3{2};
		left3.initial.insert(0);
		left3.final.insert(1);
		left3.delta.add(0, 'b', 1);

		Nfa noodle1_segment1{2};
		noodle1_segment1.initial.insert(0);
		noodle1_segment1.final.insert(1);
		noodle1_segment1.delta.add(0, 'a', 1);

		Nfa noodle1_segment2{2};
		noodle1_segment2.initial.insert(0);
		noodle1_segment2.final.insert(1);
		noodle1_segment2.delta.add(0, 'a', 1);

		Nfa noodle1_segment3{2};
		noodle1_segment3.initial.insert(0);
		noodle1_segment3.final.insert(1);
		noodle1_segment3.delta.add(0, 'b', 1);

		std::vector<std::shared_ptr<Nfa>> noodle1_segments{
			std::make_shared<Nfa>(noodle1_segment1), std::make_shared<Nfa>(noodle1_segment2),
			std::make_shared<Nfa>(noodle1_segment3)
		};

		SECTION("Full intersection") {
			Nfa right_side{7};
			right_side.initial.insert(0);
			right_side.delta.add(0, 'a', 1);
			right_side.delta.add(1, 'a', 2);
			right_side.delta.add(2, 'b', 3);
			right_side.delta.add(0, 'b', 4);
			right_side.delta.add(4, 'a', 5);
			right_side.delta.add(5, 'b', 6);
			right_side.final.insert({3, 6});

			Nfa noodle2_segment1{2};
			noodle2_segment1.initial.insert(0);
			noodle2_segment1.final.insert(1);
			noodle2_segment1.delta.add(0, 'b', 1);

			Nfa noodle2_segment2{2};
			noodle2_segment2.initial.insert(0);
			noodle2_segment2.final.insert(1);
			noodle2_segment2.delta.add(0, 'a', 1);

			Nfa noodle2_segment3{2};
			noodle2_segment3.initial.insert(0);
			noodle2_segment3.final.insert(1);
			noodle2_segment3.delta.add(0, 'b', 1);

			std::vector<std::shared_ptr<Nfa>> noodle2_segments{
				std::make_shared<Nfa>(noodle2_segment1), std::make_shared<Nfa>(noodle2_segment2),
				std::make_shared<Nfa>(noodle2_segment3)
			};

			std::vector<std::vector<std::shared_ptr<Nfa>>> expected{noodle1_segments, noodle2_segments};

			auto result{seg_nfa::noodlify_for_equation({left1, left2, left3}, right_side)};
			REQUIRE(result.size() == 2);

			CHECK(are_equivalent(result[0][0].to_nfa(), *expected[0][0]));
			CHECK(are_equivalent(result[0][1].to_nfa(), *expected[0][1]));
			CHECK(are_equivalent(result[0][2].to_nfa(), *expected[0][2]));

			CHECK(are_equivalent(result[1][0].to_nfa(), *expected[1][0]));
			CHECK(are_equivalent(result[1][1].to_nfa(), *expected[1][1]));
			CHECK(are_equivalent(result[1][2].to_nfa(), *expected[1][2]));
		}

		SECTION("Partial intersection") {
			Nfa right_side{7};
			right_side.initial.insert(0);
			right_side.delta.add(0, 'a', 1);
			right_side.delta.add(1, 'a', 2);
			right_side.delta.add(2, 'b', 3);
			right_side.delta.add(0, 'b', 4);
			right_side.delta.add(4, 'a', 5);
			right_side.delta.add(5, 'b', 6);
			right_side.final.insert(3);

			auto result{seg_nfa::noodlify_for_equation({left1, left2, left3}, right_side)};
			REQUIRE(result.size() == 1);
			CHECK(are_equivalent(result[0][0].to_nfa(), *noodle1_segments[0]));
			CHECK(are_equivalent(result[0][1].to_nfa(), *noodle1_segments[1]));
			CHECK(are_equivalent(result[0][2].to_nfa(), *noodle1_segments[2]));
		}
	}
}

TEST_CASE("mata::nfa::SegNfa::noodlify_for_equation() both sides") {
	SECTION("Empty input") {
		CHECK(
			seg_nfa::noodlify_for_equation(std::vector<std::shared_ptr<Nfa>>{}, std::vector<std::shared_ptr<Nfa>>{})
				.empty()
		);
	}

	SECTION("Simple automata") {
		Nfa x, y, z, w;
		x = create_from_regex("a*");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("(a|b)*");

		auto res = std::vector<std::vector<std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>>>(
			{{{x, {0, 0}}, {x, {0, 1}}, {y, {1, 1}}}, {{x, {0, 0}}, {y, {1, 0}}, {y, {1, 1}}}}
		);
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(x), std::make_shared<Nfa>(y)},
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(z), std::make_shared<Nfa>(w)}
		);
		for (size_t i = 0; i < noodles.size(); i++) {
			for (size_t j = 0; j < noodles[i].size(); j++) {
				CHECK(noodles[i][j].second == res[i][j].second);
				CHECK(are_equivalent(noodles[i][j].first.to_nfa(), res[i][j].first, nullptr));
				auto used_symbols{noodles[i][j].first.to_nfa().delta.get_used_symbols()};
				CHECK(used_symbols.find(EPSILON) == used_symbols.end());
			}
		}
	}

	SECTION("Simple automata -- epsilon result") {
		Nfa x, y, z, w, astar;
		x = create_from_regex("a+");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("(a|b)+");
		astar = create_from_regex("a*");

		auto res = std::vector<std::vector<std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>>>({
			{{x, {0, 1}}, {z, {1, 1}}},
			{{x, {0, 0}}, {w, {1, 1}}},
			{{x, {0, 0}}, {x, {0, 1}}, {z, {1, 1}}},
			{{x, {0, 0}}, {z, {1, 0}}, {w, {1, 1}}},
		});
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(x), std::make_shared<Nfa>(y)},
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(z), std::make_shared<Nfa>(w)}
		);
		for (size_t i = 0; i < noodles.size(); i++) {
			for (size_t j = 0; j < noodles[i].size(); j++) {
				CHECK(noodles[i][j].second == res[i][j].second);
				CHECK(are_equivalent(noodles[i][j].first.to_nfa(), res[i][j].first, nullptr));
				auto used_symbols{noodles[i][j].first.to_nfa().delta.get_used_symbols()};
				CHECK(used_symbols.find(EPSILON) == used_symbols.end());
			}
		}
	}

	SECTION("Simple automata -- epsilon input") {
		Nfa x, y, z, w;
		x = create_from_regex("");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("(a|b)*");

		auto res = std::vector<std::vector<std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>>>({});
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(x)},
			std::vector<std::shared_ptr<Nfa>>{
				std::make_shared<Nfa>(y), std::make_shared<Nfa>(z), std::make_shared<Nfa>(w)
			}
		);
		CHECK(noodles.size() == 1);
		for (size_t i = 0; i < noodles.size(); i++) {
			for (size_t j = 0; j < noodles[i].size(); j++) {
				CHECK(noodles[i][j].second == res[i][j].second);
				CHECK(are_equivalent(noodles[i][j].first.to_nfa(), res[i][j].first, nullptr));
				auto used_symbols{noodles[i][j].first.to_nfa().delta.get_used_symbols()};
				CHECK(used_symbols.find(EPSILON) == used_symbols.end());
			}
		}
	}

	SECTION("Simple automata -- epsilon input 2") {
		Nfa x, y, z, w;
		x = create_from_regex("");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("(a|b)*");

		auto res = std::vector<std::vector<std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>>>({
			{{y, {1, 1}}},
			{{y, {1, 0}}, {y, {1, 1}}},
		});
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(x), std::make_shared<Nfa>(y)},
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(z), std::make_shared<Nfa>(w)}
		);
		CHECK(noodles.size() == 2);
		for (size_t i = 0; i < noodles.size(); i++) {
			for (size_t j = 0; j < noodles[i].size(); j++) {
				CHECK(noodles[i][j].second == res[i][j].second);
				CHECK(are_equivalent(noodles[i][j].first.to_nfa(), res[i][j].first, nullptr));
				auto used_symbols{noodles[i][j].first.to_nfa().delta.get_used_symbols()};
				CHECK(used_symbols.find(EPSILON) == used_symbols.end());
			}
		}
	}

	SECTION("Simple automata -- regex 1") {
		Nfa x, y, z, u;
		x = create_from_regex("a");
		y = create_from_regex("ab*");
		z = create_from_regex("ab*");
		u = create_from_regex("a*");

		auto res = std::vector<std::vector<std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>>>({
			{{x, {0, 0}}, {x, {1, 1}}},
		});
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(x), std::make_shared<Nfa>(y)},
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(z), std::make_shared<Nfa>(u)}
		);
		CHECK(noodles.size() == 1);
		for (size_t i = 0; i < noodles.size(); i++) {
			for (size_t j = 0; j < noodles[i].size(); j++) {
				CHECK(noodles[i][j].second == res[i][j].second);
				CHECK(are_equivalent(noodles[i][j].first.to_nfa(), res[i][j].first, nullptr));
				auto used_symbols{noodles[i][j].first.to_nfa().delta.get_used_symbols()};
				CHECK(used_symbols.find(EPSILON) == used_symbols.end());
			}
		}
	}

	SECTION("Bug from noodler") {
		Nfa l1 = mata::nfa::builder::parse_from_mata(std::string(R"V0G0N(
@NFA-explicit
%Alphabet-auto
%Initial q0
%Final q2
q0 0 q1
q0 1 q1
q0 37 q1
q0 38 q1
q0 43 q1
q1 0 q2
q1 1 q2
q1 37 q2
q1 38 q2
q1 43 q2
)V0G0N"));
		Nfa l2 = mata::nfa::builder::parse_from_mata(std::string(R"V0G0N(
@NFA-explicit
%Alphabet-auto
%Initial q0
%Final q2
q0 0 q1
q0 1 q1
q0 37 q1
q0 38 q1
q0 43 q1
q1 0 q2
q1 1 q2
q1 37 q2
q1 38 q2
q1 43 q2
)V0G0N"));
		Nfa r = mata::nfa::builder::parse_from_mata(std::string(R"V0G0N(
@NFA-explicit
%Alphabet-auto
%Initial q0
%Final q4
q0 0 q1
q1 0 q2
q2 0 q3
q3 0 q4
)V0G0N"));
		std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles = seg_nfa::noodlify_for_equation(
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(l1), std::make_shared<Nfa>(l2)},
			std::vector<std::shared_ptr<Nfa>>{std::make_shared<Nfa>(r)}, false, {{"reduce", "forward"}}
		);
		CHECK(noodles.size() == 1);
		mata::Word word{0, 0};
		CHECK(noodles[0][0].first.to_nfa().get_word().value() == word);
		CHECK(noodles[0][1].first.to_nfa().get_word().value() == word);
	}
}

TEST_CASE("mata::nfa::SegNfa::noodlify_mult_eps() reports each noodle once") {
	// Shapes that finish the same noodle more than once. The result is one noodle per shape, in completion order.
	const auto segment_shapes = [](const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& noodles) {
		std::vector<std::vector<std::pair<size_t, seg_nfa::VisitedEpsilonsCounterVector>>> shapes{};
		for (const auto& noodle : noodles) {
			auto& shape{shapes.emplace_back()};
			for (const auto& [segment, counters] : noodle) {
				shape.emplace_back(segment.to_nfa().delta.num_of_transitions(), counters);
			}
		}
		return shapes;
	};

	SECTION("two epsilon transitions with different symbols between the same states") {
		Nfa aut{4};
		aut.initial.insert(0);
		aut.final.insert(3);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, EPSILON, 2);
		aut.delta.add(1, EPSILON - 1, 2);
		aut.delta.add(2, 'b', 3);

		const auto noodles{seg_nfa::noodlify_mult_eps(aut, {EPSILON, EPSILON - 1}, false)};
		REQUIRE(noodles.size() == 1);
		CHECK(
			segment_shapes(noodles) ==
			std::vector<std::vector<std::pair<size_t, seg_nfa::VisitedEpsilonsCounterVector>>>{
				{{1, {0, 0}}, {1, {1, 0}}}
			}
		);
	}

	SECTION("two routes through segments whose language is {epsilon}") {
		// The segments { 2 } and { 3 } accept only epsilon and are left out of the noodle, so both routes from
		// state 1 to state 4 finish the same two-segment noodle.
		Nfa aut{6};
		aut.initial.insert(0);
		aut.final.insert(5);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, EPSILON, 2);
		aut.delta.add(1, EPSILON, 3);
		aut.delta.add(2, EPSILON, 4);
		aut.delta.add(3, EPSILON, 4);
		aut.delta.add(4, 'b', 5);

		const auto noodles{seg_nfa::noodlify_mult_eps(aut, {EPSILON}, false)};
		REQUIRE(noodles.size() == 1);
		CHECK(
			segment_shapes(noodles) ==
			std::vector<std::vector<std::pair<size_t, seg_nfa::VisitedEpsilonsCounterVector>>>{{{1, {0}}, {1, {2}}}}
		);
	}

	SECTION("distinct noodles are all reported, in completion order") {
		// Two branches that stay distinct: both noodles survive the deduplication.
		Nfa aut{6};
		aut.initial.insert(0);
		aut.final.insert(5);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'b', 2);
		aut.delta.add(1, EPSILON, 3);
		aut.delta.add(2, EPSILON, 3);
		aut.delta.add(3, 'c', 5);

		const auto noodles{seg_nfa::noodlify_mult_eps(aut, {EPSILON}, false)};
		REQUIRE(noodles.size() == 2);
		// The 'a' branch completes first, then the 'b' branch; each noodle keeps its own branch.
		const std::vector<std::pair<mata::Word, mata::Word>> branches{{{'a'}, {'b'}}, {{'b'}, {'a'}}};
		for (size_t i{0}; i < noodles.size(); ++i) {
			const auto& [taken, other] = branches[i];
			REQUIRE(noodles[i].size() == 2);
			CHECK(noodles[i][0].first.to_nfa().is_in_lang(taken));
			CHECK(!noodles[i][0].first.to_nfa().is_in_lang(other));
			CHECK(noodles[i][0].second == seg_nfa::VisitedEpsilonsCounterVector{0});
			CHECK(noodles[i][1].first.to_nfa().is_in_lang(mata::Word{'c'}));
			CHECK(noodles[i][1].second == seg_nfa::VisitedEpsilonsCounterVector{1});
		}
	}
}

TEST_CASE("mata::nfa::SegNfa::noodlify_for_equation() for profiling", "[.profiling][noodlify]") {
	Nfa left1{3};
	left1.initial.insert(0);
	left1.final.insert({1, 2});
	left1.delta.add(0, 'a', 1);
	left1.delta.add(0, 'b', 2);
	Nfa left2{2};
	left2.initial.insert(0);
	left2.final.insert(1);
	left2.delta.add(0, 'a', 1);
	Nfa left3{2};
	left3.initial.insert(0);
	left3.final.insert(1);
	left3.delta.add(0, 'b', 1);

	Nfa right_side{7};
	right_side.initial.insert(0);
	right_side.delta.add(0, 'a', 1);
	right_side.delta.add(1, 'a', 2);
	right_side.delta.add(2, 'b', 3);
	right_side.delta.add(0, 'b', 4);
	right_side.delta.add(4, 'a', 5);
	right_side.delta.add(5, 'b', 6);
	right_side.final.insert({3, 6});

	std::vector<Nfa*> left_side{&left1, &left2, &left3};
	for (size_t i{}; i < 10'000; ++i) { seg_nfa::noodlify_for_equation(left_side, right_side); }
}

TEST_CASE("mata::nfa::SegNfa::noodlify_for_transducer()") {
	Nfa x, y, z, w;
	Nft t;

	SECTION("Simple - 1 input, 1 output") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("(a|b)*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'b'}, 0);
		Nft copy = t;
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x)}, {std::make_shared<Nfa>(y)}
		);
		REQUIRE(noodles.size() == 1);
		REQUIRE(noodles[0].size() == 1);
		auto segment = noodles[0][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
	}

	SECTION("Simple - 1 input, 2 output") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'b'}, 0);
		Nft copy = t;
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x)}, {std::make_shared<Nfa>(y), std::make_shared<Nfa>(z)}
		);
		REQUIRE(noodles.size() == 1);
		REQUIRE(noodles[0].size() == 2);
		auto segment = noodles[0][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
		segment = noodles[0][1];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(z, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 1);
	}

	SECTION("Simple - 2 input, 1 output") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'b'}, 0);
		Nft copy = t;
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x), std::make_shared<Nfa>(z)}, {std::make_shared<Nfa>(y)}
		);
		REQUIRE(noodles.size() == 1);
		REQUIRE(noodles[0].size() == 2);
		auto segment = noodles[0][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
		segment = noodles[0][1];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 1);
		CHECK(segment.output_index == 0);
	}

	SECTION("Simple - 2 input, 2 output") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("(a|b)*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'b'}, 0);
		Nft copy = t;
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x), std::make_shared<Nfa>(z)},
			{std::make_shared<Nfa>(y), std::make_shared<Nfa>(w)}
		);
		REQUIRE(noodles.size() == 2);

		REQUIRE(noodles[0].size() == 3);
		auto segment = noodles[0][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
		segment = noodles[0][1];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		if (segment.input_index == 0) {
			CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
			CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
			CHECK(segment.input_index == 0);
			CHECK(segment.output_index == 1);
		} else {
			CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
			CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
			CHECK(segment.input_index == 1);
			CHECK(segment.output_index == 0);
		}
		segment = noodles[1][2];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
		CHECK(segment.input_index == 1);
		CHECK(segment.output_index == 1);

		REQUIRE(noodles[1].size() == 3);
		segment = noodles[1][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
		segment = noodles[1][1];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		if (segment.input_index == 0) {
			CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
			CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
			CHECK(segment.input_index == 0);
			CHECK(segment.output_index == 1);
		} else {
			CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
			CHECK(mata::nfa::are_equivalent(y, *segment.output_aut));
			CHECK(segment.input_index == 1);
			CHECK(segment.output_index == 0);
		}
		segment = noodles[1][2];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
		CHECK(segment.input_index == 1);
		CHECK(segment.output_index == 1);
	}

	SECTION("More complex - 2 input, 1 output") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("(a|b)*");
		z = create_from_regex("(a|b)*");
		w = create_from_regex("a*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'a'}, 0);
		Nft copy = t;
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x), std::make_shared<Nfa>(z)}, {std::make_shared<Nfa>(y)}
		);
		REQUIRE(noodles.size() == 1);
		REQUIRE(noodles[0].size() == 2);
		auto segment = noodles[0][0];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(x, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
		CHECK(segment.input_index == 0);
		CHECK(segment.output_index == 0);
		segment = noodles[0][1];
		CHECK(mata::nft::are_equivalent(*segment.transducer, copy));
		CHECK(mata::nfa::are_equivalent(z, *segment.input_aut));
		CHECK(mata::nfa::are_equivalent(w, *segment.output_aut));
		CHECK(segment.input_index == 1);
		CHECK(segment.output_index == 0);
	}

	SECTION("More complex - 2 input, 1 output, no noodles") {
		x = create_from_regex("(a|b)*");
		y = create_from_regex("b+");
		z = create_from_regex("(a|b)*");
		t = Nft::with_levels(2, 1, {0}, {0});
		t.add_transition_by_levels(0, {'a', 'a'}, 0);
		t.add_transition_by_levels(0, {'b', 'a'}, 0);
		auto noodles = seg_nfa::noodlify_for_transducer(
			std::make_shared<Nft>(t), {std::make_shared<Nfa>(x), std::make_shared<Nfa>(z)}, {std::make_shared<Nfa>(y)}
		);
		CHECK(noodles.empty());
	}
}

namespace {
using mata::Symbol;
using SegmentAndCounters = std::pair<Nfa, seg_nfa::VisitedEpsilonsCounterVector>;

/// A random automaton of @p num_of_segments segments of 6 states, connected by ε-transitions over @p epsilons, some
///  of them parallel with different symbols, trimmed.
Nfa random_segment_automaton(const unsigned seed, const size_t num_of_segments, const std::vector<Symbol>& epsilons) {
	std::mt19937 rng{seed};
	constexpr State size{6};
	const auto random_state = [&](const State base) { return static_cast<State>(base + rng() % size); };
	Nfa aut{size * num_of_segments};
	for (size_t seg_id{0}; seg_id < num_of_segments; ++seg_id) {
		const State base{static_cast<State>(seg_id * size)};
		for (State q{base}; q < base + size; ++q) {
			for (const Symbol symbol : {Symbol{'a'}, Symbol{'b'}}) {
				if (rng() % 2 == 0) { aut.delta.add(q, symbol, random_state(base)); }
			}
			for (size_t i{0}; seg_id + 1 < num_of_segments && i < 2; ++i) {
				if (rng() % 3 == 0) { continue; }
				const State target{random_state(base + size)};
				aut.delta.add(q, epsilons[rng() % epsilons.size()], target);
				if (rng() % 4 == 0) { aut.delta.add(q, epsilons[rng() % epsilons.size()], target); }
			}
		}
	}
	aut.initial.insert(random_state(0));
	aut.initial.insert(random_state(0));
	const State last{static_cast<State>((num_of_segments - 1) * size)};
	for (size_t i{0}; i < 3; ++i) { aut.final.insert(random_state(last)); }
	aut.trim();
	return aut;
}

/// The noodles of @p aut by brute force: every route through the segments in the order of the ε-transitions and the
///  final states, one reduced automaton per (initial, final) pair, segments with the language {ε} left out, and of
///  noodles with the same pairs only the first.
std::vector<std::vector<SegmentAndCounters>> reference_noodles(const Nfa& aut, const std::set<Symbol>& epsilons) {
	seg_nfa::Segmentation segmentation{aut, epsilons};
	const std::vector<Nfa>& segments{segmentation.get_untrimmed_segments()};
	const seg_nfa::VisitedEpsilonsCounterVector zeros(epsilons.size(), 0);
	const State unused_state{static_cast<State>(aut.num_of_states())};

	std::vector<std::vector<SegmentAndCounters>> result{};
	if (segments.size() == 1) {
		if (Nfa whole{trim(segments[0])}; whole.num_of_states() > 0) { result.push_back({{std::move(whole), zeros}}); }
		return result;
	}
	std::set<std::vector<std::pair<State, State>>> seen{};
	std::vector<SegmentAndCounters> noodle{};
	std::vector<std::pair<State, State>> keys{};
	const std::function<void(size_t, State)> visit = [&](const size_t seg_id, const State init) {
		const Nfa& segment{segments[seg_id]};
		const bool last{seg_id + 1 == segments.size()};
		std::vector<State> finals{unused_state};
		if (!last) { finals.assign(segment.final.begin(), segment.final.end()); }
		for (const State fin : finals) {
			const SparseSet<State> inits{init}, fins{fin};
			std::optional<std::reference_wrapper<const SparseSet<State>>> inits_ref, fins_ref;
			if (seg_id > 0) { inits_ref = inits; }
			if (!last) { fins_ref = fins; }
			Nfa part{reduce(trim(segment, nullptr, inits_ref, fins_ref))};
			if (part.num_of_states() == 0) { continue; }
			const bool kept{!is_lang_eps(part)};
			if (kept) {
				seg_nfa::VisitedEpsilonsCounterVector counters{zeros};
				if (seg_id > 0) {
					const auto visited{segmentation.get_visited_eps().find(init)};
					counters = visited == segmentation.get_visited_eps().end()
								   ? seg_nfa::VisitedEpsilonsCounterVector{}
								   : seg_nfa::process_eps_map(visited->second);
				}
				noodle.emplace_back(std::move(part), std::move(counters));
				keys.emplace_back(init, fin);
			}
			if (last) {
				if (seen.insert(keys).second) { result.push_back(noodle); }
			} else {
				for (const Transition& transition : segmentation.get_epsilon_transitions(fin)) {
					visit(seg_id + 1, transition.target);
				}
			}
			if (kept) {
				noodle.pop_back();
				keys.pop_back();
			}
		}
	};
	visit(0, unused_state);
	return result;
}

void check_noodles(
	const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& actual,
	const std::vector<std::vector<SegmentAndCounters>>& expected
) {
	REQUIRE(actual.size() == expected.size());
	for (size_t i{0}; i < actual.size(); ++i) {
		INFO("noodle " << i);
		REQUIRE(actual[i].size() == expected[i].size());
		for (size_t j{0}; j < actual[i].size(); ++j) {
			CHECK(actual[i][j].second == expected[i][j].second);
			CHECK(are_equivalent(actual[i][j].first.to_nfa(), expected[i][j].first));
		}
	}
}

#ifdef __cpp_lib_generator
/// Whether @p actual holds the noodles of @p expected in some order.
bool same_noodles_in_any_order(
	const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& actual,
	const std::vector<std::vector<SegmentAndCounters>>& expected
) {
	if (actual.size() != expected.size()) { return false; }
	std::vector<bool> used(expected.size(), false);
	for (const auto& noodle : actual) {
		bool found{false};
		for (size_t i{0}; i < expected.size() && !found; ++i) {
			if (used[i] || expected[i].size() != noodle.size()) { continue; }
			found = std::ranges::all_of(std::views::iota(size_t{0}, noodle.size()), [&](const size_t j) {
				return noodle[j].second == expected[i][j].second &&
					   are_equivalent(noodle[j].first.to_nfa(), expected[i][j].first);
			});
			used[i] = found;
		}
		if (!found) { return false; }
	}
	return true;
}

std::vector<seg_nfa::NoodleWithEpsilonsCounter>
	lazy_noodles(const Nfa& aut, const std::set<Symbol>& epsilons, const seg_nfa::NoodleOrder order) {
	std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles{};
	for (auto&& noodle : seg_nfa::noodlify_mult_eps_lazy(aut, epsilons, false, order)) {
		noodles.push_back(std::move(noodle));
	}
	return noodles;
}

/// Whether @p lhs and @p rhs are the same noodles in the same order.
bool same_noodles_in_order(
	const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& lhs,
	const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& rhs
) {
	return std::ranges::equal(lhs, rhs, [](const auto& lhs_noodle, const auto& rhs_noodle) {
		return std::ranges::equal(lhs_noodle, rhs_noodle, [](const auto& lhs_segment, const auto& rhs_segment) {
			return lhs_segment.second == rhs_segment.second &&
				   are_equivalent(lhs_segment.first.to_nfa(), rhs_segment.first.to_nfa());
		});
	});
}

size_t num_of_reachable_states(const seg_nfa::NoodleSegment& segment) { return segment.to_nfa().num_of_states(); }
#endif
} // namespace

TEST_CASE("mata::nfa::SegNfa::noodlify_mult_eps() matches a brute-force enumeration") {
	const std::vector<Symbol> epsilons{EPSILON, EPSILON - 1};
	const std::set<Symbol> epsilon_set{epsilons.begin(), epsilons.end()};
	size_t num_of_noodles{0}, num_with_left_out{0};
#ifdef __cpp_lib_generator
	size_t num_of_shuffled{0};
#endif
	for (unsigned seed{0}; seed < 300; ++seed) {
		const size_t num_of_segments{1 + seed % 4};
		const Nfa aut{random_segment_automaton(seed, num_of_segments, epsilons)};
		if (aut.num_of_states() == 0) { continue; }
		INFO("seed " << seed);
		const std::vector<std::vector<SegmentAndCounters>> expected{reference_noodles(aut, epsilon_set)};
		const std::vector<seg_nfa::NoodleWithEpsilonsCounter> actual{seg_nfa::noodlify_mult_eps(aut, epsilon_set)};
		check_noodles(actual, expected);
		num_of_noodles += expected.size();
		const bool has_left_out{std::ranges::any_of(expected, [&](const auto& noodle) {
			return noodle.size() < seg_nfa::Segmentation{aut, epsilon_set}.get_untrimmed_segments().size();
		})};
		num_with_left_out += has_left_out;

#ifdef __cpp_lib_generator
		const std::vector<seg_nfa::NoodleWithEpsilonsCounter> forward{lazy_noodles(aut, epsilon_set, {})};
		check_noodles(forward, expected);

		using Kind = seg_nfa::NoodleOrder::Kind;
		const std::vector<seg_nfa::NoodleWithEpsilonsCounter> reverse{
			lazy_noodles(aut, epsilon_set, {.kind = Kind::Reverse})
		};
		if (has_left_out) {
			CHECK(same_noodles_in_any_order(reverse, expected));
		} else {
			check_noodles(reverse, {expected.rbegin(), expected.rend()});
		}

		// The other orders permute the noodles; a noodle with a left out segment may be finished by another route.
		for (const seg_nfa::NoodleOrder order :
			 {seg_nfa::NoodleOrder{.kind = Kind::SmallestFirst}, seg_nfa::NoodleOrder{.kind = Kind::LargestFirst},
			  seg_nfa::NoodleOrder{.kind = Kind::Random, .seed = seed}}) {
			INFO("order " << static_cast<int>(order.kind));
			const std::vector<seg_nfa::NoodleWithEpsilonsCounter> permuted{lazy_noodles(aut, epsilon_set, order)};
			CHECK(permuted.size() == expected.size());
			if (has_left_out) { continue; }
			CHECK(same_noodles_in_any_order(permuted, expected));
			if (permuted.empty() || order.kind == Kind::Random) { continue; }
			// The search is greedy: the first noodle starts with a segment of extreme size among all noodles.
			std::vector<size_t> first_sizes{};
			for (const auto& noodle : permuted) {
				first_sizes.push_back(num_of_reachable_states(noodle.front().first));
			}
			const auto [min, max]{std::ranges::minmax(first_sizes)};
			CHECK(first_sizes.front() == (order.kind == Kind::SmallestFirst ? min : max));
		}
		// A seed gives one order, so that runs can be repeated.
		const std::vector<seg_nfa::NoodleWithEpsilonsCounter> random{
			lazy_noodles(aut, epsilon_set, {.kind = Kind::Random, .seed = seed})
		};
		CHECK(same_noodles_in_order(random, lazy_noodles(aut, epsilon_set, {.kind = Kind::Random, .seed = seed})));
		num_of_shuffled += !same_noodles_in_order(random, forward);

		// Stopping early leaves the rest of the search undone.
		for (auto&& noodle : seg_nfa::noodlify_mult_eps_lazy(aut, epsilon_set)) {
			check_noodles({std::move(noodle)}, {expected.front()});
			break;
		}
#endif
	}
	// The automata exercise the search: many noodles, some with left out segments.
	CHECK(num_of_noodles > 1'000);
	CHECK(num_with_left_out > 10);
#ifdef __cpp_lib_generator
	CHECK(num_of_shuffled > 10);
#endif
}

#ifdef __cpp_lib_generator
TEST_CASE("mata::nfa::SegNfa::noodlify_for_transducer_lazy() emits the noodles of noodlify_for_transducer()") {
	Nft t{Nft::with_levels(2, 1, {0}, {0})};
	t.add_transition_by_levels(0, {'a', 'a'}, 0);
	t.add_transition_by_levels(0, {'b', 'b'}, 0);
	t.add_transition_by_levels(0, {'b', 'a'}, 0);
	const auto inputs = [] {
		return std::vector{
			std::make_shared<Nfa>(create_from_regex("(a|b)*")), std::make_shared<Nfa>(create_from_regex("a*b"))
		};
	};
	const auto outputs = [] {
		return std::vector{
			std::make_shared<Nfa>(create_from_regex("a*")), std::make_shared<Nfa>(create_from_regex("(a|b)*"))
		};
	};
	const auto eager{seg_nfa::noodlify_for_transducer(std::make_shared<Nft>(t), inputs(), outputs())};
	REQUIRE(eager.size() > 1);
	using Kind = seg_nfa::NoodleOrder::Kind;
	for (const Kind kind : {Kind::Forward, Kind::Reverse}) {
		std::vector<seg_nfa::TransducerNoodle> lazy{};
		for (auto&& noodle : seg_nfa::noodlify_for_transducer_lazy(
				 std::make_shared<Nft>(t), inputs(), outputs(), true, true, {.kind = kind}
			 )) {
			lazy.push_back(std::move(noodle));
		}
		if (kind == Kind::Reverse) { std::ranges::reverse(lazy); }
		REQUIRE(lazy.size() == eager.size());
		for (size_t i{0}; i < lazy.size(); ++i) {
			REQUIRE(lazy[i].size() == eager[i].size());
			for (size_t j{0}; j < lazy[i].size(); ++j) {
				CHECK(lazy[i][j].input_index == eager[i][j].input_index);
				CHECK(lazy[i][j].output_index == eager[i][j].output_index);
				CHECK(mata::nft::are_equivalent(*lazy[i][j].transducer, *eager[i][j].transducer));
				CHECK(are_equivalent(*lazy[i][j].input_aut, *eager[i][j].input_aut));
				CHECK(are_equivalent(*lazy[i][j].output_aut, *eager[i][j].output_aut));
			}
		}
	}
}

TEST_CASE("mata::nfa::SegNfa::noodlify_mult_eps_lazy() orders the candidates of every node by size") {
	// Three segments of single-word candidates, every candidate continuing to all candidates of the next segment.
	//  Each segment has candidates of equal and of different sizes, the ties not adjacent in the forward order.
	Nfa aut{25};
	aut.initial.insert(0);
	aut.final.insert({21, 24});
	// Segment 0: a, bb, c.
	aut.delta.add(0, 'a', 1);
	aut.delta.add(0, 'b', 2);
	aut.delta.add(2, 'b', 3);
	aut.delta.add(0, 'c', 4);
	for (const State fin : std::initializer_list<State>{1, 3, 4}) { aut.delta.add(fin, EPSILON, 10); }
	// Segment 1: x, yyy, z.
	aut.delta.add(10, 'x', 11);
	aut.delta.add(10, 'y', 12);
	aut.delta.add(12, 'y', 13);
	aut.delta.add(13, 'y', 14);
	aut.delta.add(10, 'z', 15);
	for (const State fin : std::initializer_list<State>{11, 14, 15}) {
		aut.delta.add(fin, EPSILON, 20);
		aut.delta.add(fin, EPSILON, 22);
	}
	// Segment 2: e, ff.
	aut.delta.add(20, 'e', 21);
	aut.delta.add(22, 'f', 23);
	aut.delta.add(23, 'f', 24);

	using Kind = seg_nfa::NoodleOrder::Kind;
	using Noodle = std::vector<mata::Word>;
	const auto words_of = [](const std::vector<seg_nfa::NoodleWithEpsilonsCounter>& noodles) {
		std::vector<Noodle> result{};
		for (const auto& noodle : noodles) {
			Noodle& words{result.emplace_back()};
			for (const auto& [segment, counters] : noodle) {
				const std::optional<mata::Word> word{segment.to_nfa().get_word()};
				REQUIRE(word.has_value());
				// The size the orders compare is the length of the word plus one.
				CHECK(num_of_reachable_states(segment) == word->size() + 1);
				words.push_back(*word);
			}
		}
		return result;
	};
	const auto product = [](const std::vector<std::vector<mata::Word>>& candidates) {
		std::vector<Noodle> result{};
		for (const mata::Word& first : candidates[0]) {
			for (const mata::Word& second : candidates[1]) {
				for (const mata::Word& third : candidates[2]) { result.push_back({first, second, third}); }
			}
		}
		return result;
	};

	const std::vector<Noodle> forward{words_of(lazy_noodles(aut, {EPSILON}, {}))};
	// The candidates of each segment in the forward order: the order of their first occurrence.
	std::vector<std::vector<mata::Word>> candidates(3);
	for (const Noodle& noodle : forward) {
		REQUIRE(noodle.size() == 3);
		for (size_t seg_id{0}; seg_id < 3; ++seg_id) {
			if (std::ranges::find(candidates[seg_id], noodle[seg_id]) == candidates[seg_id].end()) {
				candidates[seg_id].push_back(noodle[seg_id]);
			}
		}
	}
	REQUIRE(product(candidates) == forward);
	REQUIRE(forward.size() == 18);

	for (const Kind kind : {Kind::SmallestFirst, Kind::LargestFirst}) {
		INFO("order " << static_cast<int>(kind));
		std::vector<std::vector<mata::Word>> sorted{candidates};
		for (std::vector<mata::Word>& level : sorted) {
			std::ranges::stable_sort(level, [&](const mata::Word& lhs, const mata::Word& rhs) {
				return kind == Kind::SmallestFirst ? lhs.size() < rhs.size() : lhs.size() > rhs.size();
			});
		}
		CHECK(words_of(lazy_noodles(aut, {EPSILON}, {.kind = kind})) == product(sorted));
	}
}
#endif
