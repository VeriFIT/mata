/* tests-nfa-concatenation.cc -- Tests for concatenation of NFAs
 */

#include <optional>
#include <random>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/builder.hh"

using namespace mata::nfa;
using namespace mata::applications::strings;
using namespace mata::utils;
using namespace mata::parser;

using Symbol = mata::Symbol;

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

TEST_CASE("mata::nfa::concatenate()") {
	Nfa lhs{};
	Nfa rhs{};
	Nfa result{};

	SECTION("Empty automaton without states") {
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("One empty automaton without states") {
		rhs.add_state();
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Other empty automaton without states") {
		lhs.add_state();
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("One empty automaton without states with other with initial states") {
		lhs.add_state();
		lhs.initial.insert(0);
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Other empty automaton without states with other with initial states") {
		rhs.add_state();
		rhs.initial.insert(0);
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("One empty automaton without states with other non-empty automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Other empty automaton without states with other non-empty automaton") {
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Empty automaton") {
		lhs.add_state();
		rhs.add_state();
		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Empty language") {
		lhs.add_state();
		lhs.initial.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);

		result = concatenate(lhs, rhs);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
	}

	SECTION("Empty language rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);

		result = concatenate(lhs, rhs);
		CHECK(result.is_lang_empty());
	}

	SECTION("Single state automata accepting an empty string") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);

		result = concatenate(lhs, rhs);

		CHECK(!result.is_lang_empty());
		CHECK(result.is_in_lang(Run{{}, {}}));
		CHECK(result.delta.empty());
	}

	SECTION("Empty language rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);

		result = concatenate(lhs, rhs);

		CHECK(!result.initial.empty());
		CHECK(!result.final.empty());
		CHECK(result.delta.empty());
	}

	SECTION("Simple two state rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);

		result = concatenate(lhs, rhs);

		CHECK(!result.initial.empty());
		CHECK(!result.final.empty());
	}

	SECTION("Simple two state automata") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);

		result = concatenate(lhs, rhs);

		CHECK(!result.initial.empty());
		CHECK(!result.final.empty());

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a'}) != shortest_words.end());
	}

	SECTION("Simple two state automata with higher state num for non-final state") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state(3);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);
		rhs.delta.add(0, 'c', 3);

		result = concatenate(lhs, rhs);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a'}) != shortest_words.end());
	}

	SECTION("Simple two state lhs automaton") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);
		rhs.delta.add(0, 'a', 0);

		result = concatenate(lhs, rhs);
		CHECK(result.is_in_lang(Run{{'b'}, {}}));
		CHECK(result.is_in_lang(Run{{'b', 'a'}, {}}));
		CHECK(result.is_in_lang(Run{{'b', 'a', 'a'}, {}}));
		CHECK(!result.is_in_lang(Run{{'a'}, {}}));
		CHECK(!result.is_in_lang(Run{{'a', 'b'}, {}}));
		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
		CHECK(shortest_words.find(std::vector<Symbol>{'b'}) != shortest_words.end());
	}

	SECTION("Automaton A concatenate automaton B") {
		lhs.add_state(10);
		FILL_WITH_AUT_A(lhs);
		rhs.add_state(14);
		FILL_WITH_AUT_B(rhs);

		result = concatenate(lhs, rhs);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 4);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'b', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'b', 'a'}) != shortest_words.end());
	}

	SECTION("Automaton B concatenate automaton A") {
		lhs.add_state(10);
		FILL_WITH_AUT_A(lhs);
		rhs.add_state(14);
		FILL_WITH_AUT_B(rhs);

		result = concatenate(rhs, lhs);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 4);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'b', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'b', 'a'}) != shortest_words.end());
	}

	SECTION("Sample automata") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		lhs.delta.add(0, 58, 0);
		lhs.delta.add(0, 65, 0);
		lhs.delta.add(0, 102, 0);
		lhs.delta.add(0, 112, 0);
		lhs.delta.add(0, 115, 0);
		lhs.delta.add(0, 116, 0);

		rhs.add_state(5);
		rhs.final.insert({0, 5});
		rhs.initial.insert(5);
		rhs.delta.add(1, 112, 0);
		rhs.delta.add(2, 116, 1);
		rhs.delta.add(3, 102, 2);
		rhs.delta.add(4, 115, 3);
		rhs.delta.add(5, 102, 2);
		rhs.delta.add(5, 112, 0);
		rhs.delta.add(5, 115, 3);
		rhs.delta.add(5, 116, 1);

		result = concatenate(lhs, rhs);
		CHECK(!result.is_lang_empty());
		// TODO: Add more checks.
	}
}

TEST_CASE("mata::nfa::concatenate() over epsilon symbol") {
	Nfa lhs{};
	Nfa rhs{};
	Nfa result{};

	SECTION("Empty automaton") {
		lhs.add_state();
		rhs.add_state();
		result = concatenate(lhs, rhs, true);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Empty language") {
		lhs.add_state();
		lhs.initial.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);

		result = concatenate(lhs, rhs, true);

		CHECK(result.num_of_states() == 0);
		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
	}

	SECTION("Empty language rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);

		result = concatenate(lhs, rhs, true);
		CHECK(result.is_lang_empty());
	}

	SECTION("Single state automata accepting an empty string") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial[0]);
		CHECK(result.final[1]);
		CHECK(result.num_of_states() == 2);
		CHECK(result.delta.num_of_transitions() == 1);
		CHECK(result.delta.contains(0, EPSILON, 1));
	}

	SECTION("Empty language rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial[0]);
		CHECK(result.final[2]);
		CHECK(result.num_of_states() == 3);
		CHECK(result.delta.num_of_transitions() == 1);
		CHECK(result.delta.contains(0, EPSILON, 1));
	}

	SECTION("Simple two state rhs automaton") {
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial[0]);
		CHECK(result.final[2]);
		CHECK(result.num_of_states() == 3);
		CHECK(result.delta.num_of_transitions() == 2);
		CHECK(result.delta.contains(1, 'a', 2));
		CHECK(result.delta.contains(0, EPSILON, 1));
	}

	SECTION("Simple two state automata") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial[0]);
		CHECK(result.final[3]);
		CHECK(result.num_of_states() == 4);
		CHECK(result.delta.num_of_transitions() == 3);
		CHECK(result.delta.contains(0, 'b', 1));
		CHECK(result.delta.contains(2, 'a', 3));
		CHECK(result.delta.contains(1, EPSILON, 2));

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', EPSILON, 'a'}) != shortest_words.end());
	}

	SECTION("Simple two state automata with higher state num for non-final state") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state(3);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);
		rhs.delta.add(0, 'c', 3);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial[0]);
		CHECK(result.final[3]);
		CHECK(result.num_of_states() == 6);
		CHECK(result.delta.num_of_transitions() == 4);
		CHECK(result.delta.contains(0, 'b', 1));
		CHECK(result.delta.contains(2, 'a', 3));
		CHECK(result.delta.contains(2, 'c', 5));
		CHECK(result.delta.contains(1, EPSILON, 2));

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', EPSILON, 'a'}) != shortest_words.end());
	}

	SECTION("Simple two state lhs automaton") {
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'b', 1);
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);
		rhs.delta.add(0, 'a', 0);

		StateRenaming lhs_renaming{};
		StateRenaming rhs_renaming{};
		result = concatenate(lhs, rhs, true, &lhs_renaming, &rhs_renaming);

		CHECK(rhs_renaming == StateRenaming{{0, 2}});

		CHECK(result.initial[0]);
		CHECK(result.final[2]);
		CHECK(result.num_of_states() == 3);
		CHECK(result.delta.num_of_transitions() == 3);
		CHECK(result.delta.contains(0, 'b', 1));
		CHECK(result.delta.contains(2, 'a', 2));
		CHECK(result.delta.contains(1, EPSILON, 2));

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 1);
	}

	SECTION("Automaton A concatenate automaton B") {
		lhs.add_state(10);
		FILL_WITH_AUT_A(lhs);
		rhs.add_state(14);
		FILL_WITH_AUT_B(rhs);

		result = concatenate(lhs, rhs, true);

		CHECK(result.initial.size() == 2);
		CHECK(result.initial[1]);
		CHECK(result.initial[3]);

		CHECK(result.num_of_states() == 26);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 4);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', EPSILON, 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', EPSILON, 'b', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', EPSILON, 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', EPSILON, 'b', 'a'}) != shortest_words.end());
	}

	SECTION("Automaton B concatenate automaton A") {
		lhs.add_state(10);
		FILL_WITH_AUT_A(lhs);
		rhs.add_state(14);
		FILL_WITH_AUT_B(rhs);

		result = concatenate(rhs, lhs, true);

		CHECK(result.num_of_states() == 26);

		CHECK(result.initial.size() == 1);
		CHECK(result.initial[4]);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 4);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', EPSILON, 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', EPSILON, 'b', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', EPSILON, 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', EPSILON, 'b', 'a'}) != shortest_words.end());
	}
}

TEST_CASE("(a|b)*") {
	Nfa aut1 = builder::create_from_regex("a*");
	Nfa aut2 = builder::create_from_regex("b*");
	Nfa aut3 = builder::create_from_regex("a*b*");
	auto concatenated_aut{concatenate(aut1, aut2)};
	CHECK(are_equivalent(concatenated_aut, aut3));
}

TEST_CASE("Bug with epsilon transitions") {
	Nfa nfa1{};
	nfa1.initial.insert(0);
	nfa1.final.insert(3);
	nfa1.delta.add(0, 97, 0);
	nfa1.delta.add(0, 98, 0);
	nfa1.delta.add(0, 99, 0);
	nfa1.delta.add(0, 100, 0);
	nfa1.delta.add(0, EPSILON, 1);
	nfa1.delta.add(1, 97, 2);
	nfa1.delta.add(2, 98, 3);

	Nfa nfa2{};
	nfa2.initial.insert(0);
	nfa2.final.insert(0);
	nfa2.delta.add(0, 97, 0);
	nfa2.delta.add(0, 98, 0);
	nfa2.delta.add(0, 99, 0);
	nfa2.delta.add(0, 100, 0);

	auto result{concatenate(nfa1, nfa2, true)};

	Nfa expected{nfa1};
	expected.delta.add(3, EPSILON, 4);
	expected.delta.add(4, 97, 4);
	expected.delta.add(4, 98, 4);
	expected.delta.add(4, 99, 4);
	expected.delta.add(4, 100, 4);
	expected.final = {4};

	CHECK(are_equivalent(result, expected));
}

TEST_CASE("mata::nfa::concatenate() fills the state renaming maps") {
	// A chain 0 --a--> 1 --a--> 2, initial 0, final 2.
	Nfa lhs{};
	lhs.add_state(2);
	lhs.initial.insert(0);
	lhs.final.insert(2);
	lhs.delta.add(0, 'a', 1);
	lhs.delta.add(1, 'a', 2);
	// A one-state loop over 'b', initial and final.
	Nfa rhs{};
	rhs.add_state();
	rhs.initial.insert(0);
	rhs.final.insert(0);
	rhs.delta.add(0, 'b', 0);

	const StateRenaming expected_lhs_renaming{{0, 0}, {1, 1}, {2, 2}};
	const StateRenaming expected_rhs_renaming{{0, 3}};

	SECTION("an ordinary call fills both maps") {
		StateRenaming lhs_renaming{{7, 7}}; // Stale entries must not survive a requested map.
		StateRenaming rhs_renaming{{7, 7}};
		const Nfa result{concatenate(lhs, rhs, true, &lhs_renaming, &rhs_renaming)};

		CHECK(lhs_renaming == expected_lhs_renaming);
		CHECK(rhs_renaming == expected_rhs_renaming);
		CHECK(result.num_of_states() == 4);
		CHECK(result.delta.contains(2, EPSILON, 3));
	}

	SECTION("a requested map is filled even when the result is empty") {
		// No final state in rhs: the concatenation is empty, yet the maps still describe the full concatenation.
		Nfa no_final_rhs{rhs};
		no_final_rhs.final.clear();

		StateRenaming lhs_renaming{};
		StateRenaming rhs_renaming{};
		const Nfa result{concatenate(lhs, no_final_rhs, true, &lhs_renaming, &rhs_renaming)};

		CHECK(result.num_of_states() == 0);
		CHECK(lhs_renaming == expected_lhs_renaming);
		CHECK(rhs_renaming == expected_rhs_renaming);
	}

	SECTION("an operand without states maps only its own states") {
		StateRenaming lhs_renaming{{7, 7}};
		StateRenaming rhs_renaming{{7, 7}};
		const Nfa result{concatenate(lhs, Nfa{}, true, &lhs_renaming, &rhs_renaming)};

		CHECK(result.num_of_states() == 0);
		CHECK(lhs_renaming == expected_lhs_renaming);
		CHECK(rhs_renaming.empty());
	}

	SECTION("nullptr maps are skipped") {
		CHECK_NOTHROW(concatenate(lhs, rhs, true));
		CHECK_NOTHROW(concatenate(lhs, rhs, true, nullptr, nullptr));
		StateRenaming lhs_renaming{};
		StateRenaming rhs_renaming{};
		CHECK_NOTHROW(concatenate(lhs, rhs, true, nullptr, &rhs_renaming));
		CHECK(rhs_renaming == expected_rhs_renaming);
		CHECK_NOTHROW(concatenate(lhs, rhs, true, &lhs_renaming, nullptr));
		CHECK(lhs_renaming == expected_lhs_renaming);
	}
}

TEST_CASE("mata::nfa::concatenate() inplace") {
	SECTION("Empty automaton without states") {
		Nfa lhs{};
		Nfa rhs{};
		Nfa result{};
		result = lhs.concatenate(rhs);

		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("One empty automaton without states") {
		Nfa lhs{};
		Nfa rhs{};
		Nfa result{};
		rhs.add_state();
		result = lhs.concatenate(rhs);

		CHECK(result.initial.empty());
		CHECK(result.final.empty());
		CHECK(result.delta.empty());
		CHECK(result.is_lang_empty());
	}

	SECTION("Automaton A concatenate automaton B") {
		Nfa lhs{};
		Nfa rhs{};
		Nfa result{};
		lhs.add_state(10);
		FILL_WITH_AUT_A(lhs);
		rhs.add_state(14);
		FILL_WITH_AUT_B(rhs);

		result = lhs.concatenate(rhs);

		auto shortest_words{get_shortest_words(result)};
		CHECK(shortest_words.size() == 4);
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'b', 'a', 'b', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'a', 'a'}) != shortest_words.end());
		CHECK(shortest_words.find(std::vector<Symbol>{'a', 'a', 'b', 'a'}) != shortest_words.end());
	}

	SECTION("Sample automata") {
		Nfa lhs{};
		Nfa rhs{};
		Nfa result{};
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		lhs.delta.add(0, 58, 0);
		lhs.delta.add(0, 65, 0);
		lhs.delta.add(0, 102, 0);
		lhs.delta.add(0, 112, 0);
		lhs.delta.add(0, 115, 0);
		lhs.delta.add(0, 116, 0);

		rhs.add_state(5);
		rhs.final.insert({0, 5});
		rhs.initial.insert(5);
		rhs.delta.add(1, 112, 0);
		rhs.delta.add(2, 116, 1);
		rhs.delta.add(3, 102, 2);
		rhs.delta.add(4, 115, 3);
		rhs.delta.add(5, 102, 2);
		rhs.delta.add(5, 112, 0);
		rhs.delta.add(5, 115, 3);
		rhs.delta.add(5, 116, 1);

		result = lhs.concatenate(rhs);
		CHECK(!result.is_lang_empty());
		// TODO: Add more checks.
	}

	SECTION("Delta smaller than states") {
		Nfa lhs{};
		Nfa rhs{};

		lhs.delta.add(0, 65, 5);
		lhs.initial.insert(0);
		lhs.initial.insert(7);
		lhs.final.insert(5);
		lhs.final.insert(7);

		rhs.delta.add(0, 65, 7);
		rhs.initial.insert(0);
		rhs.final.insert(7);

		Nfa result = lhs.concatenate(rhs);
		CHECK(!result.is_lang_empty());
	}

	SECTION("the same automata") {
		Nfa lhs{};

		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		lhs.delta.add(0, 58, 0);
		lhs.delta.add(0, 65, 0);
		lhs.delta.add(0, 102, 0);
		lhs.delta.add(0, 112, 0);
		lhs.delta.add(0, 115, 0);
		lhs.delta.add(0, 116, 0);

		size_t lhs_size = lhs.num_of_states();
		Nfa result = lhs.concatenate(lhs);
		CHECK(result.num_of_states() == lhs_size * 2);
	}
}

TEST_CASE("Concat_inplace performance", "[.profiling]") {
	Nfa base;
	base.initial.insert(0);
	base.final.insert(4);
	base.delta.add(0, 45, 1);
	base.delta.add(0, 46, 1);
	base.delta.add(0, 48, 1);
	base.delta.add(0, 49, 1);
	base.delta.add(0, 50, 1);
	base.delta.add(0, 51, 1);
	base.delta.add(0, 52, 1);
	base.delta.add(0, 53, 1);
	base.delta.add(0, 54, 1);
	base.delta.add(0, 55, 1);
	base.delta.add(0, 56, 1);
	base.delta.add(0, 57, 1);
	base.delta.add(0, 65, 1);
	base.delta.add(0, 66, 1);
	base.delta.add(0, 67, 1);
	base.delta.add(0, 68, 1);
	base.delta.add(0, 69, 1);
	base.delta.add(0, 70, 1);
	base.delta.add(0, 71, 1);
	base.delta.add(0, 72, 1);
	base.delta.add(0, 73, 1);
	base.delta.add(0, 74, 1);
	base.delta.add(0, 75, 1);
	base.delta.add(0, 76, 1);
	base.delta.add(0, 77, 1);
	base.delta.add(0, 78, 1);
	base.delta.add(0, 79, 1);
	base.delta.add(0, 80, 1);
	base.delta.add(0, 81, 1);
	base.delta.add(0, 82, 1);
	base.delta.add(0, 83, 1);
	base.delta.add(0, 84, 1);
	base.delta.add(0, 85, 1);
	base.delta.add(0, 86, 1);
	base.delta.add(0, 87, 1);
	base.delta.add(0, 88, 1);
	base.delta.add(0, 89, 1);
	base.delta.add(0, 90, 1);
	base.delta.add(0, 95, 1);
	base.delta.add(0, 97, 1);
	base.delta.add(0, 98, 1);
	base.delta.add(0, 99, 1);
	base.delta.add(0, 100, 1);
	base.delta.add(0, 101, 1);
	base.delta.add(0, 102, 1);
	base.delta.add(0, 103, 1);
	base.delta.add(0, 104, 1);
	base.delta.add(0, 105, 1);
	base.delta.add(0, 106, 1);
	base.delta.add(0, 107, 1);
	base.delta.add(0, 108, 1);
	base.delta.add(0, 109, 1);
	base.delta.add(0, 110, 1);
	base.delta.add(0, 111, 1);
	base.delta.add(0, 112, 1);
	base.delta.add(0, 113, 1);
	base.delta.add(0, 114, 1);
	base.delta.add(0, 115, 1);
	base.delta.add(0, 116, 1);
	base.delta.add(0, 117, 1);
	base.delta.add(0, 118, 1);
	base.delta.add(0, 119, 1);
	base.delta.add(0, 120, 1);
	base.delta.add(0, 121, 1);
	base.delta.add(0, 122, 1);
	base.delta.add(0, 124, 1);
	base.delta.add(1, 45, 2);
	base.delta.add(1, 46, 2);
	base.delta.add(1, 48, 2);
	base.delta.add(1, 49, 2);
	base.delta.add(1, 50, 2);
	base.delta.add(1, 51, 2);
	base.delta.add(1, 52, 2);
	base.delta.add(1, 53, 2);
	base.delta.add(1, 54, 2);
	base.delta.add(1, 55, 2);
	base.delta.add(1, 56, 2);
	base.delta.add(1, 57, 2);
	base.delta.add(1, 65, 2);
	base.delta.add(1, 66, 2);
	base.delta.add(1, 67, 2);
	base.delta.add(1, 68, 2);
	base.delta.add(1, 69, 2);
	base.delta.add(1, 70, 2);
	base.delta.add(1, 71, 2);
	base.delta.add(1, 72, 2);
	base.delta.add(1, 73, 2);
	base.delta.add(1, 74, 2);
	base.delta.add(1, 75, 2);
	base.delta.add(1, 76, 2);
	base.delta.add(1, 77, 2);
	base.delta.add(1, 78, 2);
	base.delta.add(1, 79, 2);
	base.delta.add(1, 80, 2);
	base.delta.add(1, 81, 2);
	base.delta.add(1, 82, 2);
	base.delta.add(1, 83, 2);
	base.delta.add(1, 84, 2);
	base.delta.add(1, 85, 2);
	base.delta.add(1, 86, 2);
	base.delta.add(1, 87, 2);
	base.delta.add(1, 88, 2);
	base.delta.add(1, 89, 2);
	base.delta.add(1, 90, 2);
	base.delta.add(1, 95, 2);
	base.delta.add(1, 97, 2);
	base.delta.add(1, 98, 2);
	base.delta.add(1, 99, 2);
	base.delta.add(1, 100, 2);
	base.delta.add(1, 101, 2);
	base.delta.add(1, 102, 2);
	base.delta.add(1, 103, 2);
	base.delta.add(1, 104, 2);
	base.delta.add(1, 105, 2);
	base.delta.add(1, 106, 2);
	base.delta.add(1, 107, 2);
	base.delta.add(1, 108, 2);
	base.delta.add(1, 109, 2);
	base.delta.add(1, 110, 2);
	base.delta.add(1, 111, 2);
	base.delta.add(1, 112, 2);
	base.delta.add(1, 113, 2);
	base.delta.add(1, 114, 2);
	base.delta.add(1, 115, 2);
	base.delta.add(1, 116, 2);
	base.delta.add(1, 117, 2);
	base.delta.add(1, 118, 2);
	base.delta.add(1, 119, 2);
	base.delta.add(1, 120, 2);
	base.delta.add(1, 121, 2);
	base.delta.add(1, 122, 2);
	base.delta.add(1, 124, 2);
	base.delta.add(2, 45, 3);
	base.delta.add(2, 46, 3);
	base.delta.add(2, 48, 3);
	base.delta.add(2, 49, 3);
	base.delta.add(2, 50, 3);
	base.delta.add(2, 51, 3);
	base.delta.add(2, 52, 3);
	base.delta.add(2, 53, 3);
	base.delta.add(2, 54, 3);
	base.delta.add(2, 55, 3);
	base.delta.add(2, 56, 3);
	base.delta.add(2, 57, 3);
	base.delta.add(2, 65, 3);
	base.delta.add(2, 66, 3);
	base.delta.add(2, 67, 3);
	base.delta.add(2, 68, 3);
	base.delta.add(2, 69, 3);
	base.delta.add(2, 70, 3);
	base.delta.add(2, 71, 3);
	base.delta.add(2, 72, 3);
	base.delta.add(2, 73, 3);
	base.delta.add(2, 74, 3);
	base.delta.add(2, 75, 3);
	base.delta.add(2, 76, 3);
	base.delta.add(2, 77, 3);
	base.delta.add(2, 78, 3);
	base.delta.add(2, 79, 3);
	base.delta.add(2, 80, 3);
	base.delta.add(2, 81, 3);
	base.delta.add(2, 82, 3);
	base.delta.add(2, 83, 3);
	base.delta.add(2, 84, 3);
	base.delta.add(2, 85, 3);
	base.delta.add(2, 86, 3);
	base.delta.add(2, 87, 3);
	base.delta.add(2, 88, 3);
	base.delta.add(2, 89, 3);
	base.delta.add(2, 90, 3);
	base.delta.add(2, 95, 3);
	base.delta.add(2, 97, 3);
	base.delta.add(2, 98, 3);
	base.delta.add(2, 99, 3);
	base.delta.add(2, 100, 3);
	base.delta.add(2, 101, 3);
	base.delta.add(2, 102, 3);
	base.delta.add(2, 103, 3);
	base.delta.add(2, 104, 3);
	base.delta.add(2, 105, 3);
	base.delta.add(2, 106, 3);
	base.delta.add(2, 107, 3);
	base.delta.add(2, 108, 3);
	base.delta.add(2, 109, 3);
	base.delta.add(2, 110, 3);
	base.delta.add(2, 111, 3);
	base.delta.add(2, 112, 3);
	base.delta.add(2, 113, 3);
	base.delta.add(2, 114, 3);
	base.delta.add(2, 115, 3);
	base.delta.add(2, 116, 3);
	base.delta.add(2, 117, 3);
	base.delta.add(2, 118, 3);
	base.delta.add(2, 119, 3);
	base.delta.add(2, 120, 3);
	base.delta.add(2, 121, 3);
	base.delta.add(2, 122, 3);
	base.delta.add(2, 124, 3);
	base.delta.add(3, 45, 4);
	base.delta.add(3, 46, 4);
	base.delta.add(3, 48, 4);
	base.delta.add(3, 49, 4);
	base.delta.add(3, 50, 4);
	base.delta.add(3, 51, 4);
	base.delta.add(3, 52, 4);
	base.delta.add(3, 53, 4);
	base.delta.add(3, 54, 4);
	base.delta.add(3, 55, 4);
	base.delta.add(3, 56, 4);
	base.delta.add(3, 57, 4);
	base.delta.add(3, 65, 4);
	base.delta.add(3, 66, 4);
	base.delta.add(3, 67, 4);
	base.delta.add(3, 68, 4);
	base.delta.add(3, 69, 4);
	base.delta.add(3, 70, 4);
	base.delta.add(3, 71, 4);
	base.delta.add(3, 72, 4);
	base.delta.add(3, 73, 4);
	base.delta.add(3, 74, 4);
	base.delta.add(3, 75, 4);
	base.delta.add(3, 76, 4);
	base.delta.add(3, 77, 4);
	base.delta.add(3, 78, 4);
	base.delta.add(3, 79, 4);
	base.delta.add(3, 80, 4);
	base.delta.add(3, 81, 4);
	base.delta.add(3, 82, 4);
	base.delta.add(3, 83, 4);
	base.delta.add(3, 84, 4);
	base.delta.add(3, 85, 4);
	base.delta.add(3, 86, 4);
	base.delta.add(3, 87, 4);
	base.delta.add(3, 88, 4);
	base.delta.add(3, 89, 4);
	base.delta.add(3, 90, 4);
	base.delta.add(3, 95, 4);
	base.delta.add(3, 97, 4);
	base.delta.add(3, 98, 4);
	base.delta.add(3, 99, 4);
	base.delta.add(3, 100, 4);
	base.delta.add(3, 101, 4);
	base.delta.add(3, 102, 4);
	base.delta.add(3, 103, 4);
	base.delta.add(3, 104, 4);
	base.delta.add(3, 105, 4);
	base.delta.add(3, 106, 4);
	base.delta.add(3, 107, 4);
	base.delta.add(3, 108, 4);
	base.delta.add(3, 109, 4);
	base.delta.add(3, 110, 4);
	base.delta.add(3, 111, 4);
	base.delta.add(3, 112, 4);
	base.delta.add(3, 113, 4);
	base.delta.add(3, 114, 4);
	base.delta.add(3, 115, 4);
	base.delta.add(3, 116, 4);
	base.delta.add(3, 117, 4);
	base.delta.add(3, 118, 4);
	base.delta.add(3, 119, 4);
	base.delta.add(3, 120, 4);
	base.delta.add(3, 121, 4);
	base.delta.add(3, 122, 4);
	base.delta.add(3, 124, 4);

	Nfa concat;
	concat.initial.insert(1);
	concat.final.insert(0);
	concat.final.insert(1);
	concat.delta.add(1, 45, 0);
	concat.delta.add(1, 46, 0);
	concat.delta.add(1, 48, 0);
	concat.delta.add(1, 49, 0);
	concat.delta.add(1, 50, 0);
	concat.delta.add(1, 51, 0);
	concat.delta.add(1, 52, 0);
	concat.delta.add(1, 53, 0);
	concat.delta.add(1, 54, 0);
	concat.delta.add(1, 55, 0);
	concat.delta.add(1, 56, 0);
	concat.delta.add(1, 57, 0);
	concat.delta.add(1, 65, 0);
	concat.delta.add(1, 66, 0);
	concat.delta.add(1, 67, 0);
	concat.delta.add(1, 68, 0);
	concat.delta.add(1, 69, 0);
	concat.delta.add(1, 70, 0);
	concat.delta.add(1, 71, 0);
	concat.delta.add(1, 72, 0);
	concat.delta.add(1, 73, 0);
	concat.delta.add(1, 74, 0);
	concat.delta.add(1, 75, 0);
	concat.delta.add(1, 76, 0);
	concat.delta.add(1, 77, 0);
	concat.delta.add(1, 78, 0);
	concat.delta.add(1, 79, 0);
	concat.delta.add(1, 80, 0);
	concat.delta.add(1, 81, 0);
	concat.delta.add(1, 82, 0);
	concat.delta.add(1, 83, 0);
	concat.delta.add(1, 84, 0);
	concat.delta.add(1, 85, 0);
	concat.delta.add(1, 86, 0);
	concat.delta.add(1, 87, 0);
	concat.delta.add(1, 88, 0);
	concat.delta.add(1, 89, 0);
	concat.delta.add(1, 90, 0);
	concat.delta.add(1, 95, 0);
	concat.delta.add(1, 97, 0);
	concat.delta.add(1, 98, 0);
	concat.delta.add(1, 99, 0);
	concat.delta.add(1, 100, 0);
	concat.delta.add(1, 101, 0);
	concat.delta.add(1, 102, 0);
	concat.delta.add(1, 103, 0);
	concat.delta.add(1, 104, 0);
	concat.delta.add(1, 105, 0);
	concat.delta.add(1, 106, 0);
	concat.delta.add(1, 107, 0);
	concat.delta.add(1, 108, 0);
	concat.delta.add(1, 109, 0);
	concat.delta.add(1, 110, 0);
	concat.delta.add(1, 111, 0);
	concat.delta.add(1, 112, 0);
	concat.delta.add(1, 113, 0);
	concat.delta.add(1, 114, 0);
	concat.delta.add(1, 115, 0);
	concat.delta.add(1, 116, 0);
	concat.delta.add(1, 117, 0);
	concat.delta.add(1, 118, 0);
	concat.delta.add(1, 119, 0);
	concat.delta.add(1, 120, 0);
	concat.delta.add(1, 121, 0);
	concat.delta.add(1, 122, 0);
	concat.delta.add(1, 124, 0);

	for (auto i = 0; i < 1'000; i++) { base.concatenate(concat); }
}

TEST_CASE("mata::nfa::concatenate_nth_power()") {
	SECTION("Exponent 0 returns empty-string automaton") {
		Nfa aut{};
		aut.add_state(1);
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 'a', 1);

		CHECK(are_equivalent(concatenate_nth_power(aut, 0), builder::create_empty_string_nfa()));
	}

	SECTION("Exponent 1 returns the same language") {
		Nfa aut{};
		aut.add_state(1);
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 'a', 1);

		CHECK(are_equivalent(concatenate_nth_power(aut, 1), aut));
	}

	SECTION("Exponent 2 matches normal concatenate") {
		Nfa aut{};
		aut.add_state(2);
		aut.initial.insert(0);
		aut.final.insert(2);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'b', 2);

		Nfa expected = aut;
		expected.concatenate(aut);

		CHECK(are_equivalent(concatenate_nth_power(aut, 2), expected));
	}

	SECTION("Exponent 4 matches repeated normal concatenate") {
		Nfa aut{};
		aut.add_state(1);
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 'c', 1);

		Nfa expected = aut;
		expected.concatenate(aut);
		expected.concatenate(aut);
		expected.concatenate(aut);

		CHECK(are_equivalent(concatenate_nth_power(aut, 4), expected));
	}
}

TEST_CASE("mata::nfa::concatenate_eps() with custom epsilon and use_epsilon=false") {
	// Test that concatenate_eps with a custom epsilon and use_epsilon=false behaves like concatenate().
	Nfa x{2, {0}, {1}};
	x.delta.add(0, 1, 1);

	Nfa y{2, {0}, {1}};
	y.delta.add(0, 2, 1);

	const Symbol custom_epsilon = 100;

	// concatenate_eps with custom epsilon and use_epsilon=false should remove the custom epsilon transitions.
	const Nfa xy_concat = mata::nfa::algorithms::concatenate_eps(x, y, custom_epsilon, /*use_epsilon=*/false);
	const Nfa xy_normal = concatenate(x, y);

	// The two results should be equivalent.
	REQUIRE(are_equivalent(xy_concat, xy_normal));

	// The custom epsilon symbol should not be in the used symbols.
	REQUIRE(!xy_concat.delta.get_used_symbols().contains(custom_epsilon));

	// Test the language: [1,2] should be accepted, [1,100,2] should not.
	REQUIRE(xy_concat.is_in_lang({1, 2}));
	REQUIRE(!xy_concat.is_in_lang({1, custom_epsilon, 2}));
}

TEST_CASE("mata::nfa::concatenate_eps() with custom epsilon preserves operand epsilon transitions") {
	// Test that when custom epsilon != EPSILON, the operands' own EPSILON transitions are preserved.
	const Symbol custom_epsilon = 100;

	Nfa x{2, {0}, {1}};
	x.delta.add(0, 1, 1);
	x.delta.add(0, EPSILON, 1); // Operand has its own EPSILON transition.

	Nfa y{2, {0}, {1}};
	y.delta.add(0, 2, 1);

	// With use_epsilon=false and custom_epsilon, the operand's EPSILON should be preserved.
	const Nfa xy_concat = mata::nfa::algorithms::concatenate_eps(x, y, custom_epsilon, /*use_epsilon=*/false);

	// The operand's EPSILON transition should still be there (reached from state 0 of the concatenation).
	REQUIRE(xy_concat.delta.get_used_symbols().contains(EPSILON));
	// The custom epsilon should not be present.
	REQUIRE(!xy_concat.delta.get_used_symbols().contains(custom_epsilon));
}

TEST_CASE("mata::nfa::concatenate_eps() removes operand transitions over the connecting symbol") {
	// The guarantee is about the symbol, not about where the transition came from: an operand transition labelled
	// with the connecting epsilon is removed together with the connectors.
	const Symbol custom_epsilon = 100;

	Nfa x{3, {0}, {2}};
	x.delta.add(0, 1, 1);
	x.delta.add(1, custom_epsilon, 2); // Operand's own transition over the connecting symbol.

	Nfa y{2, {0}, {1}};
	y.delta.add(0, 2, 1);

	const Nfa xy_concat = mata::nfa::algorithms::concatenate_eps(x, y, custom_epsilon, /*use_epsilon=*/false);

	CHECK(!xy_concat.delta.get_used_symbols().contains(custom_epsilon));
	// x accepted 1.100, so with 100 treated as epsilon the concatenation accepts 1.2, not 1.100.2.
	CHECK(xy_concat.is_in_lang({1, 2}));
	CHECK(!xy_concat.is_in_lang({1, custom_epsilon, 2}));
}

TEST_CASE("mata::nfa::concatenate_eps() builds one epsilon connector post per lhs final") {
	Nfa lhs{};
	lhs.add_state(4);
	lhs.initial.insert(0);
	lhs.final = {1, 3};
	lhs.delta.add(0, 'a', 1);
	lhs.delta.add(0, 'a', 3);
	// Merge into an existing epsilon post, not just append to an empty one.
	lhs.delta.add(3, EPSILON, 2);
	// A symbol below a custom epsilon keeps the result post ordered by symbols otherwise.
	lhs.delta.add(1, 'b', 0);

	Nfa rhs{};
	rhs.add_state(3);
	rhs.initial = {0, 2};
	rhs.final.insert(1);
	rhs.delta.add(0, 'c', 1);
	rhs.delta.add(2, 'd', 1);
	rhs.delta.add(2, 'c', 0);

	const State lhs_states_num{static_cast<State>(lhs.num_of_states())};
	auto expected = [&](const Symbol epsilon) {
		Nfa reference{lhs.delta, lhs.initial, lhs.final};
		reference.delta.append_shifted(rhs.delta, lhs_states_num);
		reference.add_state(static_cast<State>(lhs.num_of_states() + rhs.num_of_states() - 1));
		for (const State lhs_final_state : lhs.final) {
			for (const State rhs_initial_state : rhs.initial) {
				reference.delta.add(lhs_final_state, epsilon, lhs_states_num + rhs_initial_state);
			}
		}
		reference.final.clear();
		for (const State rhs_final_state : rhs.final) { reference.final.insert(lhs_states_num + rhs_final_state); }
		return reference;
	};

	SECTION("default epsilon") {
		const Nfa result{algorithms::concatenate_eps(lhs, rhs, EPSILON, true)};
		const Nfa reference{expected(EPSILON)};
		CHECK(result.delta == reference.delta);
		CHECK(result.initial == reference.initial);
		CHECK(result.final == reference.final);
	}

	SECTION("custom epsilon above the operand symbols") {
		const Symbol custom_epsilon{42};
		const Nfa result{algorithms::concatenate_eps(lhs, rhs, custom_epsilon, true)};
		const Nfa reference{expected(custom_epsilon)};
		CHECK(result.delta == reference.delta);
		CHECK(result.initial == reference.initial);
		CHECK(result.final == reference.final);
	}

	SECTION("custom epsilon below other posts") {
		const Symbol custom_epsilon{3};
		const Nfa result{algorithms::concatenate_eps(lhs, rhs, custom_epsilon, true)};
		const Nfa reference{expected(custom_epsilon)};
		CHECK(result.delta == reference.delta);
		CHECK(result.initial == reference.initial);
		CHECK(result.final == reference.final);
	}
}

namespace {
/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t concat_draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A deterministic random NFA over {0, ..., num_of_symbols - 1} (plus an optional epsilon symbol), intended to
///  stand alone without depending on other files' generators.
Nfa random_concat_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const std::optional<Symbol> epsilon = std::nullopt
) {
	Nfa nfa{num_of_states};
	if (num_of_states == 0) { return nfa; }
	nfa.initial.insert(static_cast<State>(concat_draw(rng, num_of_states)));
	if (concat_draw(rng, 2) == 0) { nfa.initial.insert(static_cast<State>(concat_draw(rng, num_of_states))); }
	for (State state{0}; state < num_of_states; ++state) {
		if (concat_draw(rng, 3) == 0) { nfa.final.insert(state); }
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
			const size_t transitions{concat_draw(rng, 3)};
			for (size_t transition{0}; transition < transitions; ++transition) {
				nfa.delta.add(state, symbol, static_cast<State>(concat_draw(rng, num_of_states)));
			}
		}
		if (epsilon.has_value() && concat_draw(rng, 3) == 0) {
			nfa.delta.add(state, *epsilon, static_cast<State>(concat_draw(rng, num_of_states)));
		}
	}
	return nfa;
}
} // namespace

TEST_CASE("mata::nfa::concatenate(use_epsilon=false) matches the connector-and-remove reference", "[differential]") {
	// Reference: build with epsilons, then remove them globally. The new path should agree for both epsilon-free
	//  operands (direct merge) and operands carrying their own epsilon transitions (fallback).
	std::mt19937 rng{42'424'242};
	for (size_t iteration{0}; iteration < 96; ++iteration) {
		const bool with_epsilon{iteration % 3 == 2};
		const std::optional<Symbol> epsilon = with_epsilon ? std::optional<Symbol>{EPSILON} : std::nullopt;
		const size_t rhs_states{concat_draw(rng, 10) + 1};
		const size_t lhs_states{concat_draw(rng, 10) + 1};
		const Nfa lhs{random_concat_nfa(rng, lhs_states, 3, epsilon)};
		const Nfa rhs{random_concat_nfa(rng, rhs_states, 3, epsilon)};

		const Nfa actual{algorithms::concatenate_eps(lhs, rhs, EPSILON, false)};
		Nfa expected{algorithms::concatenate_eps(lhs, rhs, EPSILON, true)};
		expected.remove_epsilon(EPSILON);

		INFO("iteration=" << iteration << " with_epsilon=" << with_epsilon);
		CHECK(are_equivalent(actual, expected));
		if (!with_epsilon) {
			// For epsilon-free operands the direct path is exact: same delta, same finality, same trimming.
			CHECK(actual.delta == expected.delta);
			CHECK(actual.final == expected.final);
			CHECK(actual.initial == expected.initial);
			CHECK(actual.num_of_states() == expected.num_of_states());
		}
	}
}

TEST_CASE("mata::nfa::concatenate_eps(use_epsilon=false) handles empty languages and self-concatenation") {
	SECTION("empty lhs language") {
		Nfa lhs{};
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.delta.add(0, 'a', 1);
		Nfa rhs{};
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);
		CHECK(concatenate(lhs, rhs, false).num_of_states() == 0);
	}
	SECTION("empty rhs language") {
		Nfa lhs{};
		lhs.add_state();
		lhs.initial.insert(0);
		lhs.final.insert(0);
		Nfa rhs{};
		rhs.add_state(1);
		rhs.initial.insert(0);
		CHECK(concatenate(lhs, rhs, false).num_of_states() == 0);
	}
	SECTION("self-concatenation") {
		Nfa aut{};
		aut.add_state(1);
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 'a', 1);
		Nfa self{aut};
		const Nfa result{concatenate(aut, aut, false)};
		Nfa expected{algorithms::concatenate_eps(self, self, EPSILON, true)};
		expected.remove_epsilon(EPSILON);
		CHECK(result.delta == expected.delta);
		CHECK(result.final == expected.final);
		CHECK(result.num_of_states() == expected.num_of_states());
	}
	SECTION("rhs initial is also final makes lhs final in result") {
		Nfa lhs{};
		lhs.add_state(1);
		lhs.initial.insert(0);
		lhs.final.insert(1);
		lhs.delta.add(0, 'a', 1);
		Nfa rhs{};
		rhs.add_state();
		rhs.initial.insert(0);
		rhs.final.insert(0);
		rhs.delta.add(0, 'b', 0);
		const Nfa result{concatenate(lhs, rhs, false)};
		CHECK(result.final.contains(1)); // Original lhs final stays final iff some rhs initial is final.
		CHECK(result.delta.contains(1, 'b', 2));
	}
}

TEST_CASE("mata::nfa::concatenate_eps(use_epsilon=false) keeps operand epsilon transitions via fallback") {
	// Pre-existing epsilon transitions must still go through global removal exactly as before.
	Nfa lhs{};
	lhs.add_state(2);
	lhs.initial.insert(0);
	lhs.final.insert(2);
	lhs.delta.add(0, 'a', 1);
	lhs.delta.add(1, EPSILON, 2);
	Nfa rhs{};
	rhs.add_state(1);
	rhs.initial.insert(0);
	rhs.final.insert(1);
	rhs.delta.add(0, 'b', 1);

	const Nfa actual{algorithms::concatenate_eps(lhs, rhs, EPSILON, false)};
	Nfa expected{algorithms::concatenate_eps(lhs, rhs, EPSILON, true)};
	expected.remove_epsilon(EPSILON);

	CHECK(are_equivalent(actual, expected));
	CHECK(!actual.delta.get_used_symbols().contains(EPSILON));
	CHECK(actual.is_in_lang({'a', 'b'}));
}

TEST_CASE("mata::nfa::concatenate_eps(use_epsilon=false) trims trailing unused states like remove_epsilon") {
	// A lhs with unused trailing states and a transitionless rhs: remove_epsilon trims the direct result to the
	//  last used state, which the direct construction has to replicate.
	Nfa lhs{};
	lhs.add_state(3); // Four states, but only 0 -> 1 is used.
	lhs.initial.insert(0);
	lhs.final.insert(1);
	lhs.delta.add(0, 'a', 1);
	Nfa rhs{};
	rhs.add_state();
	rhs.initial.insert(0);
	rhs.final.insert(0);

	const Nfa result{concatenate(lhs, rhs, false)};
	Nfa expected{algorithms::concatenate_eps(lhs, rhs, EPSILON, true)};
	expected.remove_epsilon(EPSILON);

	CHECK(result.num_of_states() == expected.num_of_states());
	CHECK(result.delta == expected.delta);
	CHECK(result.final == expected.final);
	CHECK(result.final.contains(1));
	CHECK(result.is_in_lang({'a'}));
}

TEST_CASE("mata::nfa::concatenate_eps(use_epsilon=false) handles a 2000-state forward epsilon chain") {
	// Fallback-path stress: 2000 states chained by forward epsilon transitions. Correctness, not speed.
	Nfa lhs{2'000};
	lhs.initial.insert(0);
	lhs.final.insert(1'999);
	for (State state{0}; state + 1 < 2'000; ++state) { lhs.delta.add(state, EPSILON, state + 1); }
	Nfa rhs{};
	rhs.add_state();
	rhs.initial.insert(0);
	rhs.final.insert(0);

	const Nfa result{algorithms::concatenate_eps(lhs, rhs, EPSILON, false)};
	CHECK(result.num_of_states() > 0);
	CHECK(result.final.contains(0));
	CHECK(!result.delta.get_used_symbols().contains(EPSILON));
}
