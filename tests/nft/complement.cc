/** @file
 * @brief Tests for @c mata::nft::complement().
 */

#include <algorithm>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/types.hh"

using namespace mata::nft;
using namespace mata::utils;
using namespace mata;

TEST_CASE("mata::nft::complement()") {
	auto alphabet = std::make_shared<EnumAlphabet>(EnumAlphabet{'a', 'b', 'c'});
	Nft nft{Nft::with_levels(3, 10)};
	Nft nft_complemented{Nft::with_levels(3)};

	const auto CHECK_SHARED = [&] {
		CHECK(intersection(nft, nft_complemented).is_lang_empty());

		std::unordered_set<Transition> visited{};
		std::vector<std::tuple<State, Word>> stack{};
		for (State init_state : nft.initial) { stack.push_back({init_state, {}}); }
		while (!stack.empty()) {
			const auto [source, word] = std::move(stack.back());
			stack.pop_back();
			for (const auto& [symbol, target] : nft.delta[source].moves()) {
				Transition transition{source, symbol, target};
				if (visited.contains(transition)) { continue; }
				visited.insert(transition);
				Word new_word{word};
				new_word.push_back(symbol);
				stack.emplace_back(target, new_word);

				if (nft.levels[target] == 0 && nft.final.contains(target)) {
					CHECK(nft.is_in_lang(new_word));
					CHECK(not nft_complemented.is_in_lang(new_word));
				}
			}
		}

		auto words_in_nft = nft.get_words(nft.levels.num_of_levels * 2);
		auto words_in_nft_complemented = nft_complemented.get_words(nft_complemented.levels.num_of_levels * 2);
		for (const auto& word : words_in_nft) { CHECK(not words_in_nft_complemented.contains(word)); }
		for (const auto& word : words_in_nft_complemented) { CHECK(not words_in_nft.contains(word)); }
	};

	SECTION("empty automaton, empty alphabet") {
		alphabet->clear();
		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("make_complete with levels.num_of_levels == 1") {
		nft.levels.num_of_levels = 1;
		nft.initial = {0};
		nft.final = {2};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);

		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("make_complete with levels.num_of_levels == 2") {
		nft.levels.num_of_levels = 2;
		nft.initial = {0};
		nft.final = {4};
		nft.levels.set({0, 1, 0, 1, 0});
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);
		nft.delta.add(2, 'a', 3);
		nft.delta.add(3, 'b', 4);

		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("make_complete with levels.num_of_levels == 3") {
		nft.initial = {0};
		nft.final = {4};
		nft.levels.set({0, 1, 2, 0, 1, 0});
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);
		nft.delta.add(2, 'a', 3);
		nft.delta.add(3, 'b', 4);
		nft.delta.add(4, 'a', 5);

		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("self loops and jumps") {
		nft.initial = {0};
		nft.final = {4};
		nft.levels.set({0, 1, 2, 0, 1, 0});
		nft.delta.add(0, 'a', 1);
		nft.delta.add(0, 'b', 0);
		nft.delta.add(0, 'a', 2);
		nft.delta.add(0, 'b', 3);
		nft.delta.add(1, 'b', 2);
		nft.delta.add(2, 'a', 3);
		nft.delta.add(3, 'b', 4);
		nft.delta.add(4, 'a', 5);

		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("with sinks") {
		nft.initial = {0};
		nft.final = {4};
		nft.levels = {0, 1, 2, 0, 1, 0, 0, 1, 2};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(0, 'b', 0);
		nft.delta.add(0, 'a', 2);
		nft.delta.add(0, 'b', 3);
		nft.delta.add(1, 'b', 2);
		nft.delta.add(1, 'c', 8);
		nft.delta.add(2, 'a', 3);
		nft.delta.add(3, 'b', 4);
		nft.delta.add(4, 'a', 5);

		nft_complemented = complement(nft, *alphabet);
		CHECK_SHARED();
	}

	SECTION("word rejected only because of a pending jump inherited in determinize() - issue #736") {
		// Relation { (qa, qa), (qa, qc), (pb, pc) }: `2 -a-> 6` jumps over level 1, so the targets of `2 -a-> { 5, 6 }`
		// lie on two levels. The state built for { 5 } parks the jump; reusing it for `4 -b-> { 5 }` made the
		// determinized automaton accept (pb, pa), and the complement reject it.
		Nft relation{Nft::with_levels(Levels{2, {0, 1, 0, 1, 0, 1, 0, 0}}, 8, {0}, {6, 7})};
		relation.delta.add(0, 'q', 1);
		relation.delta.add(1, 'q', 2);
		relation.delta.add(0, 'p', 3);
		relation.delta.add(3, 'p', 4);
		relation.delta.add(2, 'a', 5);
		relation.delta.add(2, 'a', 6);
		relation.delta.add(4, 'b', 5);
		relation.delta.add(5, 'c', 7);

		const std::vector<Word> pb_pa{Word{'p', 'b'}, Word{'p', 'a'}};
		REQUIRE_FALSE(relation.is_in_lang_by_levels(pb_pa));
		const Nft complemented{complement(relation, EnumAlphabet{'a', 'b', 'c', 'p', 'q'})};
		CHECK(complemented.is_in_lang_by_levels(pb_pa));
		CHECK_FALSE(complemented.is_in_lang_by_levels({Word{'p', 'b'}, Word{'p', 'c'}}));
	}

	SECTION("two symbols with the same mixed target set in the complement - issue #736") {
		// `0 -a-> { 1, 2 }` and `0 -b-> { 1, 2 }` reach one mixed set: { 1 } is resolved now and the jump to 2 pending.
		//  Each pending jump must stay with its symbol; keying the mixed state by the set alone would keep one of them
		//  and the complement would accept (aa) or (bb) by mistake.
		Nft nft{Nft::with_levels(Levels{2, {0, 1, 0}}, 3, {0}, {2})};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(0, 'a', 2);
		nft.delta.add(0, 'b', 1);
		nft.delta.add(0, 'b', 2);
		const Nft complemented{complement(nft, EnumAlphabet{'a', 'b'})};
		CHECK_FALSE(complemented.is_in_lang_by_levels({Word{'a'}, Word{'a'}}));
		CHECK_FALSE(complemented.is_in_lang_by_levels({Word{'b'}, Word{'b'}}));
		CHECK(complemented.is_in_lang_by_levels({Word{'a'}, Word{'b'}}));
		CHECK(complemented.is_in_lang_by_levels({Word{'b'}, Word{'a'}}));
	}
}
