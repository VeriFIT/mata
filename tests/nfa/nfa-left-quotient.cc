/* nfa-left-quotient.cc -- Tests for the left quotient of NFA languages.
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using mata::utils::OrdVector;

namespace {

/// Reference: q is initial in the quotient iff some word of @p prefixes leads from an initial state of @p nfa to q.
Nfa reference_left_quotient(const Nfa& nfa, const Nfa& prefixes) {
	Nfa result{nfa};
	result.initial.clear();
	for (State state{0}; state < nfa.num_of_states(); ++state) {
		Nfa nfa_to_state{nfa};
		nfa_to_state.final = {state};
		if (!is_intersection_empty(nfa_to_state, prefixes)) { result.initial.insert(state); }
	}
	return result;
}

} // namespace

TEST_CASE("mata::nfa::left_quotient()") {
	SECTION("derivative by a single word") {
		const Nfa nfa{builder::create_from_regex("abc*|bd|ab")};
		CHECK(are_equivalent(
			left_quotient(nfa, builder::create_single_word_nfa(mata::Word{'a'})), builder::create_from_regex("bc*")
		));
		CHECK(are_equivalent(
			left_quotient(nfa, builder::create_single_word_nfa(mata::Word{'a', 'b'})), builder::create_from_regex("c*")
		));
		CHECK(are_equivalent(
			left_quotient(nfa, builder::create_single_word_nfa(mata::Word{'b'})), builder::create_from_regex("d")
		));
		CHECK(left_quotient(nfa, builder::create_single_word_nfa(mata::Word{'c'})).is_lang_empty());
	}

	SECTION("quotient by a language") {
		const Nfa nfa{builder::create_from_regex("a*bc|a*d")};
		CHECK(are_equivalent(left_quotient(nfa, builder::create_from_regex("a*")), nfa));
		CHECK(are_equivalent(left_quotient(nfa, builder::create_from_regex("a*b")), builder::create_from_regex("c")));
		CHECK(
			are_equivalent(left_quotient(nfa, builder::create_from_regex("a+(b|d)")), builder::create_from_regex("c|"))
		);
	}

	SECTION("prefixes with the empty word keep the language") {
		const Nfa nfa{builder::create_from_regex("ab*")};
		CHECK(are_equivalent(left_quotient(nfa, builder::create_from_regex("|b")), nfa));
	}

	SECTION("prefixes or nfa with an empty language give an empty language") {
		const Nfa nfa{builder::create_from_regex("ab*")};
		Nfa prefixes{builder::create_single_word_nfa(mata::Word{'a'})};
		prefixes.final.clear();
		CHECK(left_quotient(nfa, prefixes).is_lang_empty());
		CHECK(left_quotient(nfa, Nfa{}).is_lang_empty());
		CHECK(left_quotient(Nfa{}, builder::create_from_regex("a*")).is_lang_empty());
	}

	SECTION("prefix words longer than every word of nfa") {
		const Nfa nfa{builder::create_from_regex("ab")};
		CHECK(left_quotient(nfa, builder::create_from_regex("abb")).is_lang_empty());
		CHECK(are_equivalent(left_quotient(nfa, builder::create_from_regex("ab+")), builder::create_from_regex("")));
	}

	SECTION("epsilon transitions in both automata") {
		// nfa: a eps b, prefixes: eps a.
		Nfa nfa{3, {0}, {3}};
		nfa.delta.add(0, 'a', 1);
		nfa.delta.add(1, EPSILON, 2);
		nfa.delta.add(2, 'b', 3);
		Nfa prefixes{2, {0}, {2}};
		prefixes.delta.add(0, EPSILON, 1);
		prefixes.delta.add(1, 'a', 2);
		Nfa result{left_quotient(nfa, prefixes)};
		CHECK(result.is_in_lang(mata::Word{'b'}, true));
		CHECK(!result.is_in_lang(mata::Word{'a', 'b'}, true));
		CHECK(!result.is_in_lang(mata::Word{}, true));
	}

	SECTION("custom first epsilon") {
		Nfa nfa{3, {0}, {2}};
		nfa.delta.add(0, 100, 1);
		nfa.delta.add(1, 'a', 2);
		const Nfa prefixes{builder::create_single_word_nfa(mata::Word{'a'})};
		// With the default epsilon, 100 is an ordinary symbol that no word of prefixes starts with.
		CHECK(left_quotient(nfa, prefixes).is_lang_empty());
		// With first_epsilon = 100, the 100-move is one-sided and the quotient accepts the empty word.
		const Nfa result{left_quotient(nfa, prefixes, 100)};
		CHECK(result.initial.contains(2));
		CHECK(result.final.contains(2));
	}

	SECTION("consuming overload with prefixes aliasing nfa") {
		Nfa nfa{builder::create_from_regex("ab|abab")};
		const Nfa expected{builder::create_from_regex("|ab")};
		CHECK(are_equivalent(left_quotient(std::move(nfa), nfa), expected));
	}

	SECTION("random automata match the reference") {
		for (unsigned seed{0}; seed < 200; ++seed) {
			Nfa nfa{builder::create_random_nfa_tabakov_vardi(12, 3, 1.0, 0.3, seed)};
			Nfa prefixes{builder::create_random_nfa_tabakov_vardi(5, 3, 0.8, 0.3, seed + 1'000)};
			if (seed % 2 == 1) {
				nfa.delta.add(seed % 12, EPSILON, (seed * 7) % 12);
				prefixes.delta.add(seed % 5, EPSILON, (seed * 3) % 5);
			}
			const Nfa expected{reference_left_quotient(nfa, prefixes)};
			CAPTURE(seed);
			CHECK(OrdVector<State>{left_quotient(nfa, prefixes).initial} == OrdVector<State>{expected.initial});
			CHECK(OrdVector<State>{left_quotient(Nfa{nfa}, prefixes).initial} == OrdVector<State>{expected.initial});
		}
	}

	SECTION("automata with too many pairs of states for a bit per pair") {
		// An isolated state with a large number pads each automaton: the language and the quotient stay the same, but
		//  the pairs of states no longer fit into the memory budget of the bit matrix of visited pairs.
		for (unsigned seed{0}; seed < 20; ++seed) {
			Nfa nfa{builder::create_random_nfa_tabakov_vardi(12, 3, 1.0, 0.3, seed)};
			Nfa prefixes{builder::create_random_nfa_tabakov_vardi(5, 3, 0.8, 0.3, seed + 1'000)};
			const OrdVector<State> expected{left_quotient(nfa, prefixes).initial};
			nfa.final.insert(99'999);
			prefixes.final.insert(19'999);
			CAPTURE(seed);
			CHECK(OrdVector<State>{left_quotient(nfa, prefixes).initial} == expected);
		}
	}
}
