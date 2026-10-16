/* tests-nft-intersection.cc -- Tests for intersection of NFTs
 */

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nft/builder.hh"
#include "mata/nft/nft.hh"

using namespace mata::nft;
using namespace mata::utils;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x)                                                                                             \
	x.initial = {1, 3};                                                                                                \
	x.final = {5};                                                                                                     \
	x.delta.add(1, 'a', 3);                                                                                            \
	x.delta.add(1, 'a', 10);                                                                                           \
	x.delta.add(1, 'b', 7);                                                                                            \
	x.delta.add(3, 'a', 7);                                                                                            \
	x.delta.add(3, 'b', 9);                                                                                            \
	x.delta.add(9, 'a', 9);                                                                                            \
	x.delta.add(7, 'b', 1);                                                                                            \
	x.delta.add(7, 'a', 3);                                                                                            \
	x.delta.add(7, 'c', 3);                                                                                            \
	x.delta.add(10, 'a', 7);                                                                                           \
	x.delta.add(10, 'b', 7);                                                                                           \
	x.delta.add(10, 'c', 7);                                                                                           \
	x.delta.add(7, 'a', 5);                                                                                            \
	x.delta.add(5, 'a', 5);                                                                                            \
	x.delta.add(5, 'c', 9);

// Automaton B
#define FILL_WITH_AUT_B(x)                                                                                             \
	x.initial = {4};                                                                                                   \
	x.final = {2, 12};                                                                                                 \
	x.delta.add(4, 'c', 8);                                                                                            \
	x.delta.add(4, 'a', 8);                                                                                            \
	x.delta.add(8, 'b', 4);                                                                                            \
	x.delta.add(4, 'a', 6);                                                                                            \
	x.delta.add(4, 'b', 6);                                                                                            \
	x.delta.add(6, 'a', 2);                                                                                            \
	x.delta.add(2, 'b', 2);                                                                                            \
	x.delta.add(2, 'a', 0);                                                                                            \
	x.delta.add(0, 'a', 2);                                                                                            \
	x.delta.add(2, 'c', 12);                                                                                           \
	x.delta.add(12, 'a', 14);                                                                                          \
	x.delta.add(14, 'b', 12);

// }}}

TEST_CASE("mata::nft::intersection() with jump_mode == JumpMode::AppendDontCares") { // {{{
	Nft a, b, res;
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	SECTION("Intersection of empty automata") {
		res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of empty automata 2") {
		res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
	}

	a.add_state(5);
	b.add_state(6);

	SECTION("Intersection of automata with no transitions") {
		a.initial = {1, 3};
		a.final = {3, 5};

		b.initial = {4, 6};
		b.final = {4, 2};

		REQUIRE(!a.initial.empty());
		REQUIRE(!b.initial.empty());
		REQUIRE(!a.final.empty());
		REQUIRE(!b.final.empty());

		res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

		REQUIRE(!res.initial.empty());
		REQUIRE(!res.final.empty());

		State init_fin_st = prod_map[{3, 4}];

		REQUIRE(res.initial[init_fin_st]);
		REQUIRE(res.final[init_fin_st]);
	}

	a.add_state(10);
	b.add_state(14);

	SECTION("Intersection of automata with some transitions") {
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

		REQUIRE(res.initial[prod_map[{1, 4}]]);
		REQUIRE(res.initial[prod_map[{3, 4}]]);
		REQUIRE(res.final[prod_map[{5, 2}]]);

		// for (const auto& c : prod_map) std::cout << c.first.first << "," << c.first.second << " -> " << c.second <<
		// "\n"; std::cout << prod_map[{7, 2}] << " " <<  prod_map[{1, 2}] << '\n';
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
		REQUIRE(res.delta.contains(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
		REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
		REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
		// REQUIRE(res.delta.contains(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
		REQUIRE(res.delta.contains(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
		// REQUIRE(res.delta.contains(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.delta.contains(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
		REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
		REQUIRE(res.delta.contains(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
	}

	SECTION("Intersection of automata with some transitions but without a final state") {
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);
		b.final = {12};

		res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

		REQUIRE(res.initial[prod_map[{1, 4}]]);
		REQUIRE(res.initial[prod_map[{3, 4}]]);
		REQUIRE(res.is_lang_empty());
	}

	Nft expected;
	SECTION("Intersection of transducers with epsilon transitions.") {
		SECTION("The intersection results in an empty language.") {
			a = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			a.delta.add(0, EPSILON, 1);
			a.delta.add(1, 'b', 2);

			b = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			b.delta.add(0, 'b', 1);
			b.delta.add(1, EPSILON, 2);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(!res.initial.empty());
			CHECK(res.final.empty());
			CHECK(res.is_lang_empty());
		}

		SECTION("Epsilon is treated as an alphabet symbol.") {
			a = Nft::with_levels({2, {0, 1, 1, 0, 0}}, 5, {0}, {3, 4});
			a.delta.add(0, EPSILON, 1);
			a.delta.add(0, 'b', 2);
			a.delta.add(1, 'a', 3);
			a.delta.add(2, 'a', 4);
			a.delta.add(4, EPSILON, 4);

			b = Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3});
			b.delta.add(0, EPSILON, 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(1, 'a', 3);
			b.delta.add(1, 'b', 3);
			b.delta.add(2, 'a', 3);

			expected = Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3});
			expected.delta.add(0, EPSILON, 1);
			expected.delta.add(0, 'b', 2);
			expected.delta.add(1, 'a', 3);
			expected.delta.add(2, 'a', 3);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}
	}

	SECTION("Intersection of linear transducers with multiple levels.") {
		SECTION("Intersection 1") {
			a = Nft::with_levels({4, {0, 1, 3, 0}}, 4, {0}, {3});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'b', 2);
			a.delta.add(2, 'c', 3);

			b = Nft::with_levels({4, {0, 2, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'b', 2);

			expected = Nft::with_levels({4, {0, 1, 2, 3, 0}}, 5, {0}, {4});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'b', 2);
			expected.delta.add(2, 'b', 3);
			expected.delta.add(3, 'c', 4);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("Intersection 2") {
			a = Nft::with_levels({2, {0, 0}}, 2, {0}, {1});
			a.delta.add(0, 'a', 1);

			b = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'b', 2);

			expected = b;

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("Intersection 3") {
			a = Nft::with_levels({5, {0, 2, 3, 0}}, 4, {0}, {3});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'b', 2);
			a.delta.add(2, 'a', 3);

			b = Nft::with_levels({5, {0, 1, 3, 4, 0}}, 5, {0}, {4});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'c', 2);
			b.delta.add(2, 'b', 3);
			b.delta.add(3, 'a', 4);

			std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;
			Nft res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

			REQUIRE(!res.initial.empty());
			REQUIRE(res.final.empty());
			REQUIRE(res.delta.contains(prod_map[{0, 0}], 'a', prod_map[{1, 1}]));
			REQUIRE(res.delta.contains(prod_map[{1, 1}], 'c', prod_map[{1, 2}]));
			REQUIRE(res.delta.contains(prod_map[{1, 2}], 'b', prod_map[{2, 2}]));
			CHECK(res.is_lang_empty());
		}
	}

	SECTION("Intersection of complex transducers with multiple levels and an epsilon transition") {
		a = Nft::with_levels({3, {0, 1, 1, 2, 2, 0, 0, 0}}, 8, {0}, {5, 6, 7});
		a.delta.add(0, 'a', 1);
		a.delta.add(0, 'b', 2);
		a.delta.add(0, 'a', 4);
		a.delta.add(1, 'c', 3);
		a.delta.add(2, 'a', 4);
		a.delta.add(2, 'c', 7);
		a.delta.add(3, 'a', 5);
		a.delta.add(4, 'b', 6);
		a.delta.add(5, 'a', 3);
		a.delta.add(6, EPSILON, 4);
		a.delta.add(7, 'c', 2);

		b = Nft::with_levels({3, {0, 1, 2, 0, 0}}, 5, {0}, {3, 4});
		b.delta.add(0, 'a', 1);
		b.delta.add(0, 'b', 1);
		b.delta.add(0, 'a', 3);
		b.delta.add(1, 'a', 2);
		b.delta.add(1, 'c', 4);
		b.delta.add(2, 'b', 4);
		b.delta.add(3, 'c', 3);
		b.delta.add(4, EPSILON, 4);

		expected = Nft::with_levels({3, {0, 1, 1, 2, 0, 0, 2, 1, 2, 0, 2, 0}}, 12, {0}, {4, 5, 9, 11});
		expected.delta.add(0, 'b', 1);
		expected.delta.add(0, 'a', 2);
		expected.delta.add(0, 'a', 7);
		expected.delta.add(0, 'a', 10);
		expected.delta.add(1, 'a', 3);
		expected.delta.add(1, 'c', 4);
		expected.delta.add(2, 'a', 3);
		expected.delta.add(2, 'c', 6);
		expected.delta.add(3, 'b', 5);
		expected.delta.add(5, EPSILON, 6);
		expected.delta.add(6, 'b', 5);
		expected.delta.add(7, 'c', 8);
		expected.delta.add(8, 'a', 9);
		expected.delta.add(10, 'b', 11);

		res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

		CHECK(are_equivalent(res, expected));
	}

	SECTION("Intersection of transducers with the DONT_CARE symbol") {
		SECTION("DONT_CARE is in the lhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, DONT_CARE, 1);
			a.delta.add(1, 'c', 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, 'a', 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(0, 'a', 3);
			b.delta.add(1, 'c', 4);
			b.delta.add(2, 'd', 5);
			b.delta.add(3, 'e', 6);

			expected = Nft::with_levels({3, {0, 1, 1, 1, 2, 2, 2, 0, 0, 0}}, 10, {0}, {7, 8, 9});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(0, 'b', 2);
			expected.delta.add(0, 'a', 3);
			expected.delta.add(1, 'c', 4);
			expected.delta.add(2, 'd', 5);
			expected.delta.add(3, 'e', 6);
			expected.delta.add(4, 'c', 7);
			expected.delta.add(5, 'c', 8);
			expected.delta.add(6, 'c', 9);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in the rhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'c', 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, DONT_CARE, 1);
			b.delta.add(0, DONT_CARE, 2);
			b.delta.add(0, DONT_CARE, 3);
			b.delta.add(1, 'c', 4);
			b.delta.add(2, 'd', 5);
			b.delta.add(3, 'e', 6);

			expected = Nft::with_levels({3, {0, 1, 2, 2, 2, 0, 0, 0}}, 8, {0}, {5, 6, 7});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'c', 2);
			expected.delta.add(1, 'd', 3);
			expected.delta.add(1, 'e', 4);
			expected.delta.add(2, 'c', 5);
			expected.delta.add(3, 'c', 6);
			expected.delta.add(4, 'c', 7);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in both lhs and rhs. In lhs, DONT_CARE is at a higher level than it is in rhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, DONT_CARE, 1);
			a.delta.add(1, DONT_CARE, 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, DONT_CARE, 1);
			b.delta.add(0, DONT_CARE, 2);
			b.delta.add(0, DONT_CARE, 3);
			b.delta.add(1, 'c', 4);
			b.delta.add(2, 'd', 5);
			b.delta.add(3, 'e', 6);

			expected = Nft::with_levels({3, {0, 1, 1, 1, 2, 2, 2, 0, 0, 0}}, 10, {0}, {7, 8, 9});
			expected.delta.add(0, DONT_CARE, 1);
			expected.delta.add(0, DONT_CARE, 2);
			expected.delta.add(0, DONT_CARE, 3);
			expected.delta.add(1, 'c', 4);
			expected.delta.add(2, 'd', 5);
			expected.delta.add(3, 'e', 6);
			expected.delta.add(4, DONT_CARE, 7);
			expected.delta.add(5, DONT_CARE, 8);
			expected.delta.add(6, DONT_CARE, 9);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in the rhs at a higher level than it is in the lhs.") {
			a = Nft::with_levels({3, {0, 1, 0}}, 3, {0}, {2});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, DONT_CARE, 2);

			b = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, DONT_CARE, 2);

			expected = Nft::with_levels({3, {0, 1, 2, 0}}, 4, {0}, {3});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, DONT_CARE, 2);
			expected.delta.add(2, DONT_CARE, 3);

			res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

			CHECK(are_equivalent(res, expected));
		}
	}
} // }}}

TEST_CASE("mata::nft::intersection() with jump_mode == JumpMode::RepeatSymbol") {
	Nft a, b, res;
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	SECTION("Intersection of empty automata") {
		res = intersection(a, b, &prod_map, JumpMode::AppendDontCares);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of empty automata 2") {
		res = intersection(a, b, nullptr, JumpMode::AppendDontCares);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
	}

	a.add_state(5);
	b.add_state(6);

	SECTION("Intersection of automata with no transitions") {
		a.initial = {1, 3};
		a.final = {3, 5};

		b.initial = {4, 6};
		b.final = {4, 2};

		REQUIRE(!a.initial.empty());
		REQUIRE(!b.initial.empty());
		REQUIRE(!a.final.empty());
		REQUIRE(!b.final.empty());

		res = intersection(a, b, &prod_map);

		REQUIRE(!res.initial.empty());
		REQUIRE(!res.final.empty());

		State init_fin_st = prod_map[{3, 4}];

		REQUIRE(res.initial[init_fin_st]);
		REQUIRE(res.final[init_fin_st]);
	}

	a.add_state(10);
	b.add_state(14);

	SECTION("Intersection of automata with some transitions") {
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		res = intersection(a, b, &prod_map);

		REQUIRE(res.initial[prod_map[{1, 4}]]);
		REQUIRE(res.initial[prod_map[{3, 4}]]);
		REQUIRE(res.final[prod_map[{5, 2}]]);

		// for (const auto& c : prod_map) std::cout << c.first.first << "," << c.first.second << " -> " << c.second <<
		// "\n"; std::cout << prod_map[{7, 2}] << " " <<  prod_map[{1, 2}] << '\n';
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
		REQUIRE(res.delta.contains(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
		REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
		REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
		// REQUIRE(res.delta.contains(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
		REQUIRE(res.delta.contains(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
		// REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
		// REQUIRE(res.delta.contains(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.delta.contains(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.delta.contains(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
		REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
		REQUIRE(res.delta.contains(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
	}

	SECTION("Intersection of automata with some transitions but without a final state") {
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);
		b.final = {12};

		res = intersection(a, b, &prod_map);

		REQUIRE(res.initial[prod_map[{1, 4}]]);
		REQUIRE(res.initial[prod_map[{3, 4}]]);
		REQUIRE(res.is_lang_empty());
	}

	Nft expected;
	SECTION("Intersection of transducers with epsilon transitions.") {
		SECTION("The intersection results in an empty language.") {
			a = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			a.delta.add(0, EPSILON, 1);
			a.delta.add(1, 'b', 2);

			b = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			b.delta.add(0, 'b', 1);
			b.delta.add(1, EPSILON, 2);

			res = intersection(a, b);

			CHECK(!res.initial.empty());
			CHECK(res.final.empty());
			CHECK(res.is_lang_empty());
		}

		SECTION("Epsilon is treated as an alphabet symbol.") {
			a = Nft::with_levels({2, {0, 1, 1, 0, 0}}, 5, {0}, {3, 4});
			a.delta.add(0, EPSILON, 1);
			a.delta.add(0, 'b', 2);
			a.delta.add(1, 'a', 3);
			a.delta.add(2, 'a', 4);
			a.delta.add(4, EPSILON, 4);

			b = Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3});
			b.delta.add(0, EPSILON, 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(1, 'a', 3);
			b.delta.add(1, 'b', 3);
			b.delta.add(2, 'a', 3);

			expected = Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3});
			expected.delta.add(0, EPSILON, 1);
			expected.delta.add(0, 'b', 2);
			expected.delta.add(1, 'a', 3);
			expected.delta.add(2, 'a', 3);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}
	}

	SECTION("Intersection of linear transducers with multiple levels.") {
		SECTION("Intersection 1") {
			a = Nft::with_levels({4, {0, 1, 3, 0}}, 4, {0}, {3});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'b', 2);
			a.delta.add(2, 'c', 3);

			b = Nft::with_levels({4, {0, 2, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'b', 2);

			expected = Nft::with_levels({4, {0, 1, 2, 3, 0}}, 5, {0}, {4});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'b', 2);
			expected.delta.add(2, 'b', 3);
			expected.delta.add(3, 'c', 4);

			res = intersection(a, b);

			CHECK(res.is_lang_empty());
			CHECK(!are_equivalent(res, expected));
		}

		SECTION("Intersection 2") {
			a = Nft::with_levels({4, {0, 1, 3, 0}}, 4, {0}, {3});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'a', 2);
			a.delta.add(2, 'a', 3);

			b = Nft::with_levels({4, {0, 2, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'a', 2);

			expected = Nft::with_levels({4, {0, 1, 2, 3, 0}}, 5, {0}, {4});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'a', 2);
			expected.delta.add(2, 'a', 3);
			expected.delta.add(3, 'a', 4);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("Intersection 3") {
			a = Nft::with_levels({2, {0, 0}}, 2, {0}, {1});
			a.delta.add(0, 'a', 1);

			b = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'b', 2);

			expected = b;

			res = intersection(a, b);

			CHECK(!are_equivalent(res, expected));
		}

		SECTION("Intersection 4") {
			a = Nft::with_levels({2, {0, 0}}, 2, {0}, {1});
			a.delta.add(0, 'a', 1);

			b = Nft::with_levels({2, {0, 1, 0}}, 3, {0}, {2});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'a', 2);

			expected = b;

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("Intersection 5") {
			a = Nft::with_levels({5, {0, 2, 3, 0}}, 4, {0}, {3});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'a', 2);
			a.delta.add(2, 'b', 3);

			b = Nft::with_levels({5, {0, 1, 3, 4, 0}}, 5, {0}, {4});
			b.delta.add(0, 'a', 1);
			b.delta.add(1, 'a', 2);
			b.delta.add(2, 'b', 3);
			b.delta.add(3, 'b', 4);

			expected = Nft::with_levels({5, {0, 1, 2, 3, 4, 0}}, 6, {0}, {5});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'a', 2);
			expected.delta.add(2, 'a', 3);
			expected.delta.add(3, 'b', 4);
			expected.delta.add(4, 'b', 5);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}
	}

	SECTION("Intersection of complex transducers with multiple levels and an epsilon transition") {
		a = Nft::with_levels({3, {0, 1, 1, 2, 2, 0, 0, 0}}, 8, {0}, {5, 6, 7});
		a.delta.add(0, 'a', 1);
		a.delta.add(0, 'b', 2);
		a.delta.add(0, 'a', 4);
		a.delta.add(1, 'c', 3);
		a.delta.add(2, 'a', 4);
		a.delta.add(2, 'c', 7);
		a.delta.add(3, 'a', 5);
		a.delta.add(4, 'b', 6);
		a.delta.add(5, 'a', 3);
		a.delta.add(6, EPSILON, 4);
		a.delta.add(7, 'c', 2);

		b = Nft::with_levels({3, {0, 1, 2, 0, 0}}, 5, {0}, {3, 4});
		b.delta.add(0, 'a', 1);
		b.delta.add(0, 'b', 1);
		b.delta.add(0, 'a', 3);
		b.delta.add(1, 'a', 2);
		b.delta.add(1, 'c', 4);
		b.delta.add(2, 'b', 4);
		b.delta.add(3, EPSILON, 3);
		b.delta.add(4, 'c', 4);

		expected = Nft::with_levels({3, {0, 1, 2, 0, 1, 0}}, 6, {0}, {3, 5});
		expected.delta.add(0, 'a', 1);
		expected.delta.add(0, 'b', 1);
		expected.delta.add(0, 'b', 4);
		expected.delta.add(1, 'a', 2);
		expected.delta.add(2, 'b', 3);
		expected.delta.add(4, 'c', 5);
		expected.delta.add(5, 'c', 4);

		res = intersection(a, b);

		CHECK(are_equivalent(res, expected));
	}

	SECTION("Intersection of transducers with the DONT_CARE symbol") {
		SECTION("DONT_CARE is in the lhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, DONT_CARE, 1);
			a.delta.add(1, 'c', 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, 'a', 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(0, 'a', 3);
			b.delta.add(1, 'c', 4);
			b.delta.add(2, 'd', 5);
			b.delta.add(3, 'e', 6);

			expected = Nft::with_levels({3, {0, 1, 2, 0}}, 4, {0}, {3});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'c', 2);
			expected.delta.add(2, 'c', 3);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in the rhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, 'c', 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, 'a', 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(0, 'a', 3);
			b.delta.add(1, DONT_CARE, 4);
			b.delta.add(2, DONT_CARE, 5);
			b.delta.add(3, DONT_CARE, 6);

			expected = Nft::with_levels({3, {0, 1, 2, 0}}, 4, {0}, {3});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'a', 2);
			expected.delta.add(2, 'c', 3);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in both lhs and rhs. In lhs, DONT_CARE is at a smaller level than it is in rhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, DONT_CARE, 1);
			a.delta.add(1, 'c', 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, 'a', 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(0, 'a', 3);
			b.delta.add(1, DONT_CARE, 4);
			b.delta.add(2, DONT_CARE, 5);
			b.delta.add(3, DONT_CARE, 6);

			expected = Nft::with_levels({3, {0, 1, 2, 0}}, 3, {0}, {3});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(0, 'b', 1);
			expected.delta.add(1, DONT_CARE, 2);
			expected.delta.add(2, 'c', 3);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}

		SECTION("DONT_CARE is in the rhs at a smaller level than it is in the lhs.") {
			a = Nft::with_levels({3, {0, 2, 0}}, 3, {0}, {2});
			a.delta.add(0, 'a', 1);
			a.delta.add(1, DONT_CARE, 2);

			b = Nft::with_levels({3, {0, 1, 1, 1, 0, 0, 0}}, 7, {0}, {4, 5, 6});
			b.delta.add(0, 'a', 1);
			b.delta.add(0, 'b', 2);
			b.delta.add(0, 'a', 3);
			b.delta.add(1, DONT_CARE, 4);
			b.delta.add(2, DONT_CARE, 5);
			b.delta.add(3, DONT_CARE, 6);

			expected = Nft::with_levels({3, {0, 1, 2, 0}}, 4, {0}, {3});
			expected.delta.add(0, 'a', 1);
			expected.delta.add(1, 'a', 2);
			expected.delta.add(2, DONT_CARE, 3);

			res = intersection(a, b);

			CHECK(are_equivalent(res, expected));
		}
	}
}

TEST_CASE("mata::nft::intersection() for profiling", "[.profiling],[intersection]") {
	Nft a{4};
	a.initial.insert(0);
	a.final.insert({0, 2, 3});
	a.delta.add(0, 'a', 0);
	a.delta.add(0, 'b', 0);
	a.delta.add(0, 'c', 1);
	a.delta.add(1, 'a', 3);
	a.delta.add(1, 'b', 2);

	Nft b{9};
	b.initial.insert(0);
	b.final.insert({2, 4, 8, 7});
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'a', 2);
	b.delta.add(0, 'c', 3);
	b.delta.add(2, 'a', 8);
	b.delta.add(2, 'b', 8);
	b.delta.add(3, 'a', 6);
	b.delta.add(3, 'a', 4);
	b.delta.add(3, 'a', 7);

	for (size_t i{0}; i < 10'000; ++i) { Nft result{intersection(a, b, nullptr, JumpMode::AppendDontCares)}; }
}

TEST_CASE("mata::nft::Move semantics", "[.profiling][std::move]") {
	Nft b{10};
	b.initial.insert(0);
	b.final.insert({2, 4, 8, 7});
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'a', 2);
	b.delta.add(0, 'c', 3);
	b.delta.add(2, 'a', 8);
	b.delta.add(2, 'b', 8);
	b.delta.add(3, 'a', 6);
	b.delta.add(3, 'a', 4);
	b.delta.add(3, 'a', 7);

	for (size_t i{0}; i < 1'000'000; ++i) {
		Nft a{std::move(b)};
		a.initial.insert(1);
		b = std::move(a);
	}
}

TEST_CASE("mata::nft::intersection() reserves product targets only for pairs that can be appended") {
	// In JumpMode::RepeatSymbol the class product skips every pair whose target on either side sits at an
	//  already-passed level: the lhs 'a' targets on level 2 (<= rhs source level 2) and symmetrically on the
	//  other side. Only the two level-0 targets per side can pair, so the surviving post must reserve their
	//  2x2 pairs, not all 5x3 collected before the skips were checked.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nft lhs{Nft::with_levels({3, {0, 2, 0, 0, 2, 2, 2}}, 7, {0}, {2})};
	lhs.delta.add(0, 'x', 1);
	lhs.delta.add(1, 'a', 2);
	lhs.delta.add(1, 'a', 3);
	lhs.delta.add(1, 'a', 4);
	lhs.delta.add(1, 'a', 5);
	lhs.delta.add(1, 'a', 6);

	Nft rhs{Nft::with_levels({3, {0, 2, 0, 0, 2}}, 5, {0}, {2})};
	rhs.delta.add(0, 'x', 1);
	rhs.delta.add(1, 'a', 2);
	rhs.delta.add(1, 'a', 3);
	rhs.delta.add(1, 'a', 4);

	const Nft result{intersection(lhs, rhs, &prod_map, JumpMode::RepeatSymbol)};

	const State middle{prod_map[{1, 1}]};
	const StatePost& middle_post{result.delta.state_post(middle)};
	const auto symbol_post{middle_post.find('a')};
	REQUIRE(symbol_post != middle_post.end());
	CHECK(symbol_post->targets.size() == 4);
	CHECK(symbol_post->targets.capacity() == 4);
	CHECK(result.delta.num_of_transitions() == 5);
}

TEST_CASE("mata::nft::intersection() reserves DONT_CARE product targets only for pairs that can be appended") {
	// The same skip in process_dont_care(): of the 5x3 targets only the two level-0 targets per side pass the
	//  level filter, so the surviving 'b' post must reserve their 2x2 pairs, not all 5x3.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nft lhs{Nft::with_levels({3, {0, 2, 0, 0, 2, 2, 2}}, 7, {0}, {2})};
	lhs.delta.add(0, 'x', 1);
	lhs.delta.add(1, DONT_CARE, 2);
	lhs.delta.add(1, DONT_CARE, 3);
	lhs.delta.add(1, DONT_CARE, 4);
	lhs.delta.add(1, DONT_CARE, 5);
	lhs.delta.add(1, DONT_CARE, 6);

	Nft rhs{Nft::with_levels({3, {0, 2, 0, 0, 2}}, 5, {0}, {2})};
	rhs.delta.add(0, 'x', 1);
	rhs.delta.add(1, 'b', 2);
	rhs.delta.add(1, 'b', 3);
	rhs.delta.add(1, 'b', 4);

	const Nft result{intersection(lhs, rhs, &prod_map, JumpMode::RepeatSymbol)};

	const State middle{prod_map[{1, 1}]};
	const StatePost& middle_post{result.delta.state_post(middle)};
	const auto symbol_post{middle_post.find('b')};
	REQUIRE(symbol_post != middle_post.end());
	CHECK(symbol_post->targets.size() == 4);
	CHECK(symbol_post->targets.capacity() == 4);
	CHECK(result.delta.num_of_transitions() == 5);
}

TEST_CASE("mata::nft::intersection() with an empty operand keeps the level count - issue #763") {
	const Nft empty3 = Nft::with_levels(3);
	const Nft sigma_star3 = mata::nft::builder::create_sigma_star_nft(size_t{3});

	const Nft result = intersection(empty3, sigma_star3);
	CHECK(result.is_lang_empty());
	CHECK(result.levels.num_of_levels == 3);
}

TEST_CASE("mata::nft::intersection() between NFTs with different level counts throws - issue #763") {
	const Nft two = Nft::with_levels(2);
	const Nft three = Nft::with_levels(3);
	CHECK_THROWS_AS(intersection(two, three), std::invalid_argument);
}

TEST_CASE("mata::nft::intersection() deduplicates repeated targets within one symbol post") {
	// Under JumpMode::RepeatSymbol a side waits in its source state whenever the other side's target is deeper, so
	//  two different combinations of targets of one symbol can give the same product state. Unlike in the NFA
	//  product, the targets collected for a symbol post therefore need deduplication, not only sorting.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	// Both targets of 'a' in lhs sit on level 2, the single target of 'a' in rhs on level 1. lhs is the deeper side
	//  in both combinations, hence it waits in state 0 twice and both yield the pair (0, 1).
	Nft lhs{Nft::with_levels({3, {0, 2, 2}}, 3, {0}, {1, 2})};
	lhs.delta.add(0, 'a', 1);
	lhs.delta.add(0, 'a', 2);

	Nft rhs{Nft::with_levels({3, {0, 1}}, 2, {0}, {1})};
	rhs.delta.add(0, 'a', 1);

	const Nft result{intersection(lhs, rhs, &prod_map, JumpMode::RepeatSymbol)};

	const State source{prod_map[{0, 0}]};
	const StatePost& source_post{result.delta.state_post(source)};
	const auto symbol_post{source_post.find('a')};
	REQUIRE(symbol_post != source_post.end());
	CHECK(symbol_post->targets.size() == 1);
	CHECK(symbol_post->targets.front() == prod_map[{0, 1}]);
	CHECK(result.delta.num_of_transitions() == 1);
}

TEST_CASE("mata::nft::intersection() keeps exactly the symbols both state posts have") {
	// The product walks both state posts with a two-pointer merge. The operands below share one symbol per level and
	//  surround it with symbols only one of them has, so a merge that mismatches the posts shows up immediately.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nft lhs{Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3})};
	lhs.delta.add(0, 'a', 1); // Only in lhs.
	lhs.delta.add(0, 'c', 1);
	lhs.delta.add(0, 'c', 2);
	lhs.delta.add(0, 'e', 1); // Only in lhs.
	lhs.delta.add(1, 'x', 3); // Only in lhs.
	lhs.delta.add(1, 'z', 3);
	lhs.delta.add(2, 'z', 3);

	Nft rhs{Nft::with_levels({2, {0, 1, 1, 0}}, 4, {0}, {3})};
	rhs.delta.add(0, 'b', 1); // Only in rhs.
	rhs.delta.add(0, 'c', 1);
	rhs.delta.add(0, 'c', 2);
	rhs.delta.add(0, 'd', 1); // Only in rhs.
	rhs.delta.add(1, 'y', 3); // Only in rhs.
	rhs.delta.add(1, 'z', 3);
	rhs.delta.add(2, 'z', 3);

	const Nft result{intersection(lhs, rhs, &prod_map, JumpMode::AppendDontCares)};

	// 'c' leads to all four pairs of the two targets on each side, and each of them reaches (3, 3) over 'z'.
	CHECK(result.num_of_states() == 6);
	CHECK(result.delta.num_of_transitions() == 8);
	const State source{prod_map[{0, 0}]};
	CHECK(result.delta.state_post(source).num_of_moves() == 4);
	for (const State lhs_target : {State{1}, State{2}}) {
		for (const State rhs_target : {State{1}, State{2}}) {
			const State middle{prod_map[{lhs_target, rhs_target}]};
			CHECK(result.delta.contains(source, 'c', middle));
			CHECK(result.delta.state_post(middle).num_of_moves() == 1);
			CHECK(result.delta.contains(middle, 'z', prod_map[{3, 3}]));
		}
	}
	CHECK(result.final.contains(prod_map[{3, 3}]));
}
