/* tests-nfa-intersection.cc -- Tests for intersection of NFAs
 */

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
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

TEST_CASE("mata::nfa::intersection()") { // {{{
	Nfa a, b, res;
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	SECTION("Intersection of empty automata") {
		res = intersection(a, b, EPSILON, &prod_map);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of empty automata 2") {
		res = intersection(a, b);

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

		res = intersection(a, b, EPSILON, &prod_map);

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

		res = intersection(a, b, EPSILON, &prod_map);

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

		res = intersection(a, b, EPSILON, &prod_map);

		REQUIRE(res.initial[prod_map[{1, 4}]]);
		REQUIRE(res.initial[prod_map[{3, 4}]]);
		REQUIRE(res.is_lang_empty());
	}
} // }}}

TEST_CASE("mata::nfa::intersection() with preserving epsilon transitions") {
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nfa a{6};
	a.initial.insert(0);
	a.final.insert({1, 4, 5});
	a.delta.add(0, EPSILON, 1);
	a.delta.add(1, 'a', 1);
	a.delta.add(1, 'b', 1);
	a.delta.add(1, 'c', 2);
	a.delta.add(2, 'b', 4);
	a.delta.add(2, EPSILON, 3);
	a.delta.add(3, 'a', 5);

	Nfa b{10};
	b.initial.insert(0);
	b.final.insert({2, 4, 8, 7});
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'a', 2);
	b.delta.add(2, 'a', 4);
	b.delta.add(2, EPSILON, 3);
	b.delta.add(3, 'b', 4);
	b.delta.add(0, 'c', 5);
	b.delta.add(5, 'a', 8);
	b.delta.add(5, EPSILON, 6);
	b.delta.add(6, 'a', 9);
	b.delta.add(6, 'b', 7);

	Nfa result{intersection(a, b, EPSILON, &prod_map)};

	// Check states.
	CHECK(result.is_state(prod_map[{0, 0}]));
	CHECK(result.is_state(prod_map[{1, 0}]));
	CHECK(result.is_state(prod_map[{1, 1}]));
	CHECK(result.is_state(prod_map[{1, 2}]));
	CHECK(result.is_state(prod_map[{1, 3}]));
	CHECK(result.is_state(prod_map[{1, 4}]));
	CHECK(result.is_state(prod_map[{2, 5}]));
	CHECK(result.is_state(prod_map[{3, 5}]));
	CHECK(result.is_state(prod_map[{2, 6}]));
	CHECK(result.is_state(prod_map[{3, 6}]));
	CHECK(result.is_state(prod_map[{4, 7}]));
	CHECK(result.is_state(prod_map[{5, 9}]));
	CHECK(result.is_state(prod_map[{5, 8}]));
	CHECK(result.num_of_states() == 13);

	CHECK(result.initial[prod_map[{0, 0}]]);
	CHECK(result.initial.size() == 1);

	CHECK(result.final[prod_map[{1, 2}]]);
	CHECK(result.final[prod_map[{1, 4}]]);
	CHECK(result.final[prod_map[{4, 7}]]);
	CHECK(result.final[prod_map[{5, 8}]]);
	CHECK(result.final.size() == 4);

	// Check transitions.
	CHECK(result.delta.num_of_transitions() == 14);

	CHECK(result.delta.contains(prod_map[{0, 0}], EPSILON, prod_map[{1, 0}]));
	CHECK(result.delta.state_post(prod_map[{0, 0}]).num_of_moves() == 1);

	CHECK(result.delta.contains(prod_map[{1, 0}], 'b', prod_map[{1, 1}]));
	CHECK(result.delta.contains(prod_map[{1, 0}], 'a', prod_map[{1, 2}]));
	CHECK(result.delta.contains(prod_map[{1, 0}], 'c', prod_map[{2, 5}]));
	CHECK(result.delta.state_post(prod_map[{1, 0}]).num_of_moves() == 3);

	CHECK(result.delta.state_post(prod_map[{1, 1}]).empty());

	CHECK(result.delta.contains(prod_map[{1, 2}], EPSILON, prod_map[{1, 3}]));
	CHECK(result.delta.contains(prod_map[{1, 2}], 'a', prod_map[{1, 4}]));
	CHECK(result.delta.state_post(prod_map[{1, 2}]).num_of_moves() == 2);

	CHECK(result.delta.contains(prod_map[{1, 3}], 'b', prod_map[{1, 4}]));
	CHECK(result.delta.state_post(prod_map[{1, 3}]).num_of_moves() == 1);

	CHECK(result.delta.state_post(prod_map[{1, 4}]).empty());

	CHECK(result.delta.contains(prod_map[{2, 5}], EPSILON, prod_map[{3, 5}]));
	CHECK(result.delta.contains(prod_map[{2, 5}], EPSILON, prod_map[{2, 6}]));
	CHECK(result.delta.state_post(prod_map[{2, 5}]).num_of_moves() == 2);

	CHECK(result.delta.contains(prod_map[{3, 5}], 'a', prod_map[{5, 8}]));
	CHECK(result.delta.contains(prod_map[{3, 5}], EPSILON, prod_map[{3, 6}]));
	CHECK(result.delta.state_post(prod_map[{3, 5}]).num_of_moves() == 2);

	CHECK(result.delta.contains(prod_map[{2, 6}], 'b', prod_map[{4, 7}]));
	CHECK(result.delta.contains(prod_map[{2, 6}], EPSILON, prod_map[{3, 6}]));
	CHECK(result.delta.state_post(prod_map[{2, 6}]).num_of_moves() == 2);

	CHECK(result.delta.contains(prod_map[{3, 6}], 'a', prod_map[{5, 9}]));
	CHECK(result.delta.state_post(prod_map[{3, 6}]).num_of_moves() == 1);

	CHECK(result.delta.state_post(prod_map[{4, 7}]).empty());

	CHECK(result.delta.state_post(prod_map[{5, 9}]).empty());

	CHECK(result.delta.state_post(prod_map[{5, 8}]).empty());
}

TEST_CASE("mata::nfa::intersection() with product states revisited out of order") {
	// The product states are numbered in the order in which the product first reaches them, so a symbol post can
	//  reach an older (smaller) product state after a newer (greater) one. The targets of the resulting symbol post
	//  still have to be sorted, otherwise lookups in it (binary search) miss transitions.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nfa a{4};
	a.initial.insert(0);
	a.final.insert({1, 2, 3});
	a.delta.add(0, 'a', 3);
	a.delta.add(0, 'b', 1);
	a.delta.add(0, 'b', 2);
	a.delta.add(0, 'b', 3);

	Nfa b{4};
	b.initial.insert(0);
	b.final.insert({1, 2, 3});
	b.delta.add(0, 'a', 3);
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'b', 2);
	b.delta.add(0, 'b', 3);

	const Nfa result{intersection(a, b, EPSILON, &prod_map)};

	// (3, 3) is created over 'a', hence it is the smallest of the product states reached over 'b', even though it is
	//  the last target pair of 'b'.
	const State source{prod_map[{0, 0}]};
	CHECK(result.num_of_states() == 10);
	CHECK(result.delta.num_of_transitions() == 10);
	CHECK(result.delta.state_post(source).num_of_moves() == 10);
	CHECK(result.delta.contains(source, 'a', prod_map[{3, 3}]));
	for (const State lhs_target : {State{1}, State{2}, State{3}}) {
		for (const State rhs_target : {State{1}, State{2}, State{3}}) {
			CHECK(result.delta.contains(source, 'b', prod_map[{lhs_target, rhs_target}]));
		}
	}
	CHECK(mata::utils::is_sorted(result.delta.state_post(source).find('b')->targets.to_vector()));
}

TEST_CASE("mata::nfa::intersection() with epsilon self-loops on both sides") {
	// Both operands have an epsilon self-loop, so both epsilon posts of the product state contain the product state
	//  itself: merging them must not duplicate it.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;

	Nfa a{2};
	a.initial.insert(0);
	a.final.insert({0, 1});
	a.delta.add(0, EPSILON, 0);
	a.delta.add(0, EPSILON, 1);

	Nfa b{2};
	b.initial.insert(0);
	b.final.insert({0, 1});
	b.delta.add(0, EPSILON, 0);
	b.delta.add(0, EPSILON, 1);

	const Nfa result{intersection(a, b, EPSILON, &prod_map)};

	const State source{prod_map[{0, 0}]};
	CHECK(result.num_of_states() == 4);
	CHECK(result.delta.state_post(source).num_of_moves() == 3);
	CHECK(result.delta.contains(source, EPSILON, source));
	CHECK(result.delta.contains(source, EPSILON, prod_map[{1, 0}]));
	CHECK(result.delta.contains(source, EPSILON, prod_map[{0, 1}]));
	CHECK(result.delta.num_of_transitions() == 7);
}

TEST_CASE("mata::nfa::intersection() for profiling", "[.profiling],[intersection]") {
	Nfa a{6};
	a.initial.insert(0);
	a.final.insert({1, 4, 5});
	a.delta.add(0, EPSILON, 1);
	a.delta.add(1, 'a', 1);
	a.delta.add(1, 'b', 1);
	a.delta.add(1, 'c', 2);
	a.delta.add(2, 'b', 4);
	a.delta.add(2, EPSILON, 3);
	a.delta.add(3, 'a', 5);

	Nfa b{10};
	b.initial.insert(0);
	b.final.insert({2, 4, 8, 7});
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'a', 2);
	b.delta.add(2, 'a', 4);
	b.delta.add(2, EPSILON, 3);
	b.delta.add(3, 'b', 4);
	b.delta.add(0, 'c', 5);
	b.delta.add(5, 'a', 8);
	b.delta.add(5, EPSILON, 6);
	b.delta.add(6, 'a', 9);
	b.delta.add(6, 'b', 7);

	for (size_t i{0}; i < 10'000; ++i) { Nfa result{intersection(a, b)}; }
}

TEST_CASE("Move semantics", "[.profiling][std::move]") {
	Nfa b{10};
	b.initial.insert(0);
	b.final.insert({2, 4, 8, 7});
	b.delta.add(0, 'b', 1);
	b.delta.add(0, 'a', 2);
	b.delta.add(2, 'a', 4);
	b.delta.add(2, EPSILON, 3);
	b.delta.add(3, 'b', 4);
	b.delta.add(0, 'c', 5);
	b.delta.add(5, 'a', 8);
	b.delta.add(5, EPSILON, 6);
	b.delta.add(6, 'a', 9);
	b.delta.add(6, 'b', 7);

	for (size_t i{0}; i < 1'000'000; ++i) {
		Nfa a{std::move(b)};
		a.initial.insert(1);
		b = std::move(a);
	}
}

TEST_CASE("mata::nfa::is_intersection_empty()") {
	Nfa lhs{};
	Nfa rhs{};

	SECTION("automata with empty languages") {
		CHECK(is_intersection_empty(lhs, rhs));
		lhs.initial = {0};
		CHECK(is_intersection_empty(lhs, rhs));
		rhs.initial = {0};
		rhs.final = {0};
		// lhs has an initial state but no final state: the intersection stays empty.
		CHECK(is_intersection_empty(lhs, rhs));
		lhs.final = {0};
		CHECK(!is_intersection_empty(lhs, rhs));
	}

	SECTION("a pair initial and final in both accepts the empty word") {
		lhs.initial = {0};
		lhs.final = {0};
		rhs.initial = {0, 1};
		rhs.final = {1};
		Run witness{};
		CHECK(!is_intersection_empty(lhs, rhs, &witness));
		CHECK(witness.word.empty());
	}

	SECTION("shared multi-symbol word with a witness that replays in both automata") {
		lhs = Nfa{3, {0}, {2}};
		lhs.delta.add(0, 'a', 1);
		lhs.delta.add(1, 'b', 2);
		rhs = Nfa{4, {0}, {3}};
		rhs.delta.add(0, 'a', 1);
		rhs.delta.add(1, 'b', 3);
		Run witness{};
		CHECK(!is_intersection_empty(lhs, rhs, &witness));
		CHECK(witness.word == mata::Word{'a', 'b'});
		CHECK(lhs.is_in_lang(witness));
		CHECK(rhs.is_in_lang(witness));
	}

	SECTION("disjoint alphabets stay disjoint") {
		lhs = Nfa{2, {0}, {1}};
		lhs.delta.add(0, 'a', 1);
		rhs = Nfa{2, {0}, {1}};
		rhs.delta.add(0, 'b', 1);
		CHECK(is_intersection_empty(lhs, rhs));
	}

	SECTION("epsilon moves advance one side only") {
		// lhs <EPSILON, 'a'> word, rhs <'a'> word share 'a' if the epsilon moves one side.
		lhs = Nfa{3, {0}, {2}};
		lhs.delta.add(0, EPSILON, 1);
		lhs.delta.add(1, 'a', 2);
		rhs = Nfa{2, {0}, {1}};
		rhs.delta.add(0, 'a', 1);
		Run witness{};
		CHECK(!is_intersection_empty(lhs, rhs, &witness));
		CHECK(witness.word == mata::Word{'a'});
		CHECK(lhs.is_in_lang(witness, true));
		CHECK(rhs.is_in_lang(witness, true));
	}

	SECTION("custom first_epsilon flips which moves are one-sided") {
		lhs = Nfa{3, {0}, {2}};
		lhs.delta.add(0, 100, 1);
		lhs.delta.add(1, 97, 2);
		rhs = Nfa{2, {0}, {1}};
		rhs.delta.add(0, 97, 1);
		// With the default epsilon, symbol 100 is an ordinary shared symbol absent from rhs: empty.
		CHECK(is_intersection_empty(lhs, rhs));
		// With first_epsilon = 100, the 100-move is one-sided: 'a' is shared and the witness is stripped.
		Run witness{};
		CHECK(!is_intersection_empty(lhs, rhs, &witness, 100));
		CHECK(witness.word == mata::Word{'a'});
	}

	SECTION("agreement with the materialized product on branching automata") {
		FILL_WITH_AUT_A(lhs);
		FILL_WITH_AUT_B(rhs);
		CHECK(is_intersection_empty(lhs, rhs) == intersection(lhs, rhs).is_lang_empty());
		lhs.final = {7};
		CHECK(is_intersection_empty(lhs, rhs) == intersection(lhs, rhs).is_lang_empty());
	}

	SECTION("a witness is required to prove nonempty, but is optional") {
		lhs = Nfa{2, {0}, {1}};
		lhs.delta.add(0, 'a', 1);
		rhs = Nfa{2, {0}, {1}};
		rhs.delta.add(0, 'a', 1);
		CHECK(!is_intersection_empty(lhs, rhs));
		CHECK(!is_intersection_empty(lhs, rhs, nullptr));
	}

	SECTION("several initial states on both sides") {
		lhs = Nfa{4, {0, 1}, {3}};
		lhs.delta.add(0, 'a', 2);
		lhs.delta.add(1, 'b', 2);
		lhs.delta.add(2, 'c', 3);
		rhs = Nfa{4, {0, 1}, {3}};
		rhs.delta.add(0, 'd', 2);
		rhs.delta.add(1, 'b', 2);
		rhs.delta.add(2, 'c', 3);
		Run witness{};
		CHECK(!is_intersection_empty(lhs, rhs, &witness));
		CHECK(witness.word == mata::Word{'b', 'c'});
	}
}
