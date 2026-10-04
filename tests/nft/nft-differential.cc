/* nft-differential.cc -- Seeded randomized differential tests of the NFT algorithms.
 *
 * The automata come from raw std::mt19937 output, so a seed gives the same transducer under every
 *  standard library, and failing inputs are printed in full.
 */

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <set>
#include <string>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/nft.hh"

using namespace mata::nft;
using mata::EnumAlphabet;
using mata::Symbol;
using mata::Word;

namespace {

size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A random level-respecting NFT over {0, ..., num_of_symbols - 1}: every transition goes from a
///  state on level l to a state on level (l + 1) mod num_of_levels.
Nft random_nft(
	std::mt19937& rng, const size_t num_of_states_per_level, const size_t num_of_symbols, const size_t num_of_levels
) {
	Nft nft{Nft::with_levels(num_of_levels)};
	std::vector<std::vector<State>> states_by_level(num_of_levels);
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			states_by_level[level].push_back(nft.add_state_with_level(static_cast<Level>(level)));
		}
	}
	nft.initial.insert(states_by_level[0][draw(rng, num_of_states_per_level)]);
	for (const State state : states_by_level[0]) {
		if (draw(rng, 3) == 0) { nft.final.insert(state); }
	}
	for (size_t level{0}; level < num_of_levels; ++level) {
		const size_t next_level{(level + 1) % num_of_levels};
		for (const State source : states_by_level[level]) {
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				for (size_t transition{0}, transitions{draw(rng, 2)}; transition < transitions; ++transition) {
					nft.delta.add(source, symbol, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
				}
			}
		}
	}
	return nft;
}

/// Every word over {0, ..., num_of_symbols - 1} whose length is at most @p max_length and divisible
///  by @p num_of_levels, so that it can be read by a level-respecting NFT.
std::vector<Word> all_level_words(const size_t num_of_symbols, const size_t max_length, const size_t num_of_levels) {
	std::vector<Word> words{Word{}};
	std::vector<Word> result{Word{}};
	for (size_t length{1}; length <= max_length; ++length) {
		std::vector<Word> next{};
		for (const Word& word : words) {
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				Word extended{word};
				extended.push_back(symbol);
				next.push_back(std::move(extended));
			}
		}
		words = std::move(next);
		if (length % num_of_levels == 0) { result.insert(result.end(), words.begin(), words.end()); }
	}
	return result;
}

std::string describe(const unsigned seed, const Nft& nft) {
	std::string description{"seed " + std::to_string(seed) + "\nlevels:"};
	for (const Level level : nft.levels) { description += " " + std::to_string(level); }
	description += "\ninitial:";
	for (const State state : nft.initial) { description += " " + std::to_string(state); }
	description += "\nfinal:";
	for (const State state : nft.final) { description += " " + std::to_string(state); }
	description += "\ntransitions:\n";
	for (const Transition& transition : nft.delta.transitions()) {
		description += std::to_string(transition.source) + " " + std::to_string(transition.symbol) + " " +
					   std::to_string(transition.target) + "\n";
	}
	return description;
}

} // Anonymous namespace.

TEST_CASE("mata::nft differential: complement accepts exactly the rejected words", "[differential]") {
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t num_of_levels{2};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));

		const Nft complemented{complement(nft, *alphabet)};
		for (const Word& word : all_level_words(num_of_symbols, 4, num_of_levels)) {
			// Exactly one of the transducer and its complement accepts each word of the alphabet.
			const bool accepted{nft.is_in_lang(word)};
			const bool accepted_by_complement{complemented.is_in_lang(word)};
			CHECK(accepted != accepted_by_complement);
		}
	}
}

TEST_CASE("mata::nft differential: the inclusion algorithms agree", "[differential]") {
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		std::mt19937 other_rng{seed + 500};
		const size_t num_of_symbols{1 + seed % 2};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nft lhs{random_nft(rng, 1 + seed % 3, num_of_symbols, 2)};
		const Nft rhs{random_nft(other_rng, 1 + seed % 3, num_of_symbols, 2)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 500, rhs));

		const bool naive{is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "naive"}})};
		const bool antichains{
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "antichains"}})
		};
		CHECK(naive == antichains);

		if (naive) {
			// Every bounded word of the included transducer is accepted by the including one.
			for (const Word& word : all_level_words(num_of_symbols, 4, 2)) {
				if (lhs.is_in_lang(word)) { CHECK(rhs.is_in_lang(word)); }
			}
		}
	}
}

TEST_CASE("mata::nft differential: the composition modes agree on jump-free transducers", "[differential]") {
	for (unsigned seed{0}; seed < 60; ++seed) {
		std::mt19937 lhs_rng{seed};
		std::mt19937 rhs_rng{seed + 700};
		const size_t num_of_symbols{1 + seed % 2};
		const Nft lhs{random_nft(lhs_rng, 1 + seed % 2, num_of_symbols, 2)};
		const Nft rhs{random_nft(rhs_rng, 1 + seed % 2, num_of_symbols, 2)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 700, rhs));

		const Nft general{compose(lhs, rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::General)};
		const Nft fast{compose(lhs, rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::FastNoJump)};
		CHECK(general.levels.num_of_levels == fast.levels.num_of_levels);
		// The epsilon placement may differ between the modes, so the languages are compared word by
		//  word up to a bound instead of with are_equivalent().
		for (const Word& word : all_level_words(num_of_symbols, 4, 2)) {
			CHECK(general.is_in_lang(word) == fast.is_in_lang(word));
		}
	}
}
