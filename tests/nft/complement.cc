/** @file
 * @brief Tests for @c mata::nft::complement().
 */

#include <algorithm>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nft/types.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"

using namespace mata::nft;
using namespace mata::utils;
using namespace mata;

TEST_CASE("mata::nft::complement()") {
    EnumAlphabet alphabet{ 'a', 'b', 'c' };
    Nft nft{ Nft::with_levels(3, 10) };
    nft.alphabet = &alphabet;
    Nft nft_complemented{ Nft::with_levels(3) };
    nft_complemented.alphabet = &alphabet;

    const auto CHECK_SHARED = [&] {
        CHECK(intersection(nft, nft_complemented).is_lang_empty());

        std::unordered_set<Transition> visited{};
        std::vector<std::tuple<State, Word>> stack{};
        for (State init_state : nft.initial) { stack.push_back({ init_state, {} }); }
        while (!stack.empty()) {
            const auto [source, word] = std::move(stack.back());
            stack.pop_back();
            for (const auto& [symbol, target] : nft.delta[source].moves()) {
                Transition transition{ source, symbol, target };
                if (visited.contains(transition)) { continue; }
                visited.insert(transition);
                Word new_word{ word };
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
        for (const auto& word : words_in_nft) {
            CHECK(not words_in_nft_complemented.contains(word));
        }
        for (const auto& word : words_in_nft_complemented) {
            CHECK(not words_in_nft.contains(word));
        }
    };

    SECTION("empty automaton, empty alphabet") {
        alphabet.clear();
        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }

    SECTION("make_complete with levels.num_of_levels == 1") {
        nft.levels.num_of_levels = 1;
        nft.initial = { 0 };
        nft.final = { 2 };
        nft.delta.add(0, 'a', 1);
        nft.delta.add(1, 'b', 2);

        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }

    SECTION("make_complete with levels.num_of_levels == 2") {
        nft.levels.num_of_levels = 2;
        nft.initial = { 0 };
        nft.final = { 4 };
        nft.levels.set({ 0, 1, 0, 1, 0 });
        nft.delta.add(0, 'a', 1);
        nft.delta.add(1, 'b', 2);
        nft.delta.add(2, 'a', 3);
        nft.delta.add(3, 'b', 4);

        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }

    SECTION("make_complete with levels.num_of_levels == 3") {
        nft.initial = { 0 };
        nft.final = { 4 };
        nft.levels.set({ 0, 1, 2, 0, 1, 0 });
        nft.delta.add(0, 'a', 1);
        nft.delta.add(1, 'b', 2);
        nft.delta.add(2, 'a', 3);
        nft.delta.add(3, 'b', 4);
        nft.delta.add(4, 'a', 5);

        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }

    SECTION("self loops and jumps") {
        nft.initial = { 0 };
        nft.final = { 4 };
        nft.levels.set({ 0, 1, 2, 0, 1, 0 });
        nft.delta.add(0, 'a', 1);
        nft.delta.add(0, 'b', 0);
        nft.delta.add(0, 'a', 2);
        nft.delta.add(0, 'b', 3);
        nft.delta.add(1, 'b', 2);
        nft.delta.add(2, 'a', 3);
        nft.delta.add(3, 'b', 4);
        nft.delta.add(4, 'a', 5);

        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }

    SECTION("with sinks") {
        nft.initial = { 0 };
        nft.final = { 4 };
        nft.levels = { 0, 1, 2, 0, 1, 0, 0, 1, 2 };
        nft.delta.add(0, 'a', 1);
        nft.delta.add(0, 'b', 0);
        nft.delta.add(0, 'a', 2);
        nft.delta.add(0, 'b', 3);
        nft.delta.add(1, 'b', 2);
        nft.delta.add(1, 'c', 8);
        nft.delta.add(2, 'a', 3);
        nft.delta.add(3, 'b', 4);
        nft.delta.add(4, 'a', 5);

        nft_complemented = complement(nft, alphabet);
        CHECK_SHARED();
    }
}
