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
///  state on level l to a state on level (l + 1) mod num_of_levels. Each (state, symbol) gets zero,
///  one or two targets, so a word can reach several states at once whenever the level holds more
///  than one state. @p with_dont_care and @p with_epsilon add DONT_CARE and EPSILON transitions,
///  which the algorithms have to treat like the ordinary ones.
Nft random_nft(
	std::mt19937& rng,
	const size_t num_of_states_per_level,
	const size_t num_of_symbols,
	const size_t num_of_levels,
	const bool with_dont_care = false,
	const bool with_epsilon = false
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
				for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
					nft.delta.add(source, symbol, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
				}
			}
			if (with_dont_care && draw(rng, 4) == 0) {
				nft.delta.add(source, DONT_CARE, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
			}
			if (with_epsilon && draw(rng, 5) == 0) {
				nft.delta.add(source, EPSILON, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
			}
		}
	}
	return nft;
}

std::shared_ptr<EnumAlphabet> alphabet_of(const size_t num_of_symbols) {
	auto alphabet{std::make_shared<EnumAlphabet>()};
	for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
	return alphabet;
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
	size_t nondeterministic_inputs{0};
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t num_of_levels{2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));
		if (!nft.is_deterministic()) { ++nondeterministic_inputs; }

		const Nft complemented{complement(nft, *alphabet)};
		for (const Word& word : all_level_words(num_of_symbols, 4, num_of_levels)) {
			// Exactly one of the transducer and its complement accepts each word of the alphabet.
			const bool accepted{nft.is_in_lang(word)};
			const bool accepted_by_complement{complemented.is_in_lang(word)};
			CHECK(accepted != accepted_by_complement);
		}
	}
	// Complementing goes through determinization, which is trivial on a deterministic input, so the
	//  corpus has to contain transducers where a word reaches several states at once.
	CHECK(nondeterministic_inputs > 0);
}

TEST_CASE("mata::nft differential: the inclusion algorithms agree", "[differential]") {
	size_t nondeterministic_inputs{0};
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		std::mt19937 other_rng{seed + 500};
		const size_t num_of_symbols{1 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft lhs{random_nft(rng, 1 + seed % 3, num_of_symbols, 2)};
		const Nft rhs{random_nft(other_rng, 1 + seed % 3, num_of_symbols, 2)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 500, rhs));
		if (!lhs.is_deterministic() || !rhs.is_deterministic()) { ++nondeterministic_inputs; }

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
	// The two algorithms can only differ on a transducer where a word reaches several states.
	CHECK(nondeterministic_inputs > 0);
}

// Only DONT_CARE is drawn into the operands below: with an EPSILON transition in an operand,
//  compose(..., FastNoJump) aborts on the assertion at src/nft/composition.cc:848 while General
//  returns an empty transducer, so the comparison cannot run until that is fixed.
TEST_CASE("mata::nft differential: the composition modes agree on jump-free transducers", "[differential]") {
	size_t nondeterministic_inputs{0};
	for (unsigned seed{0}; seed < 60; ++seed) {
		for (const bool with_dont_care : {false, true}) {
			std::mt19937 lhs_rng{seed};
			std::mt19937 rhs_rng{seed + 700};
			const size_t num_of_symbols{1 + seed % 2};
			const Nft lhs{random_nft(lhs_rng, 1 + seed % 2, num_of_symbols, 2, with_dont_care)};
			const Nft rhs{random_nft(rhs_rng, 1 + seed % 2, num_of_symbols, 2, with_dont_care)};
			INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 700, rhs));
			if (!lhs.is_deterministic() || !rhs.is_deterministic()) { ++nondeterministic_inputs; }

			const Nft general{compose(lhs, rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::General)};
			const Nft fast{compose(lhs, rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::FastNoJump)};
			CHECK(general.levels.num_of_levels == fast.levels.num_of_levels);
			// The epsilon placement may differ between the modes, so the languages are compared word
			//  by word up to a bound instead of with are_equivalent().
			for (const Word& word : all_level_words(num_of_symbols, 4, 2)) {
				CHECK(general.is_in_lang(word) == fast.is_in_lang(word));
			}
		}
	}
	// The two modes build the product differently only when a word reaches several states.
	CHECK(nondeterministic_inputs > 0);
}

TEST_CASE("mata::nft differential: composing with an empty operand keeps the level count", "[differential]") {
	for (unsigned seed{0}; seed < 20; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, 2)};
		const Nft empty{Nft::with_levels(2)};
		INFO(describe(seed, nft));

		// An empty operand empties the language but must not lose the levels of the result.
		for (const auto& [lhs, rhs] :
			 {std::pair{std::cref(nft), std::cref(empty)}, std::pair{std::cref(empty), std::cref(nft)},
			  std::pair{std::cref(empty), std::cref(empty)}}) {
			const Nft general{
				compose(lhs.get(), rhs.get(), {1}, {0}, true, JumpMode::NoJump, CompositionMode::General)
			};
			const Nft fast{
				compose(lhs.get(), rhs.get(), {1}, {0}, true, JumpMode::NoJump, CompositionMode::FastNoJump)
			};
			CHECK(general.levels.num_of_levels == 2);
			CHECK(fast.levels.num_of_levels == 2);
			CHECK(general.is_lang_empty());
			CHECK(fast.is_lang_empty());
		}
	}
}

// is_universal() carries the NFA antichain algorithm over unchanged: it demands a final state after
//  every single symbol, while a transducer with n levels can only be final after a multiple of n
//  symbols. On genuine multi-level transducers it therefore answers false even for the complete one
//  below, which accepts every word of the alphabet, and it contradicts the complement being empty.
//  ("naive" throws "is_universal naive algorithm is not implemented for NFTs", so the complement is
//  the only independent reference.) Expected to fail until is_universal() respects the levels.
TEST_CASE(
	"mata::nft differential: universality agrees with the complement on multi-level transducers",
	"[differential][!shouldfail]"
) {
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		// Genuine multi-level transducers: two and three levels, each holding its own states.
		const size_t num_of_levels{2 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));

		const bool universal{nft.is_universal(*alphabet, {{"algorithm", "antichains"}})};
		CHECK(universal == complement(nft, *alphabet).is_lang_empty());

		// A universal transducer accepts every word over the alphabet, so a rejected bounded word
		//  refutes universality.
		for (const Word& word : all_level_words(num_of_symbols, 2 * num_of_levels, num_of_levels)) {
			if (!nft.is_in_lang(word)) {
				CHECK(!universal);
				break;
			}
		}
	}

	// A complete two-level transducer whose level-0 state is final accepts every word of the
	//  alphabet; the bounded check confirms it, is_universal() does not.
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	Nft universal{Nft::with_levels(2, 2)};
	universal.levels[1] = 1;
	universal.initial.insert(0);
	universal.final.insert(0);
	for (Symbol symbol{0}; symbol < 2; ++symbol) {
		universal.delta.add(0, symbol, 1);
		universal.delta.add(1, symbol, 0);
	}
	for (const Word& word : all_level_words(2, 4, 2)) { CHECK(universal.is_in_lang(word)); }
	CHECK(complement(universal, *alphabet).is_lang_empty());
	CHECK(universal.is_universal(*alphabet, {{"algorithm", "antichains"}}));
}

// complement() builds a one-state automaton with no transitions when the input has no initial state,
//  so the complement of the empty language accepts only the empty word instead of every word. An
//  input with an initial state but no transitions is complemented correctly, so the gap is the
//  missing initial state. Expected to fail until complement() handles it.
TEST_CASE(
	"mata::nft differential: complement of a transducer without initial states is universal",
	"[differential][!shouldfail]"
) {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	const Nft empty{Nft::with_levels(2)};
	const Nft complemented{complement(empty, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(!empty.is_in_lang(word));
		CHECK(complemented.is_in_lang(word));
	}
}

// complement() does not expand jump transitions: on the transducer below it rejects the word (0, 1),
//  which the input rejects as well, while the two-state-per-level transducer of the same language is
//  complemented correctly. Expected to fail until complement() handles jumps.
TEST_CASE("mata::nft differential: complement of a transducer with a jump transition", "[differential][!shouldfail]") {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	// Both states stay on level 0, so 0 -0-> 1 is one transition over both levels, read as the pair
	//  (0, 0) under RepeatSymbol.
	Nft jumping{Nft::with_levels(2, 2)};
	jumping.initial.insert(0);
	jumping.final.insert(1);
	jumping.delta.add(0, 0, 1);
	const Nft complemented{complement(jumping, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(jumping.is_in_lang(word) != complemented.is_in_lang(word));
	}
}

// complement() ignores DONT_CARE: it treats the transition as one more concrete symbol instead of
//  the set of all of them, so the complement of the transducer below accepts the words the input
//  accepts, while the same language written with concrete symbols is complemented correctly.
//  Expected to fail until complement() expands DONT_CARE.
TEST_CASE("mata::nft differential: complement of a transducer with DONT_CARE", "[differential][!shouldfail]") {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	// 0 -DONT_CARE-> 1 -0-> 0 with 0 final: the transducer accepts (x, 0) for every symbol x.
	Nft dont_care{Nft::with_levels(2, 2)};
	dont_care.levels[1] = 1;
	dont_care.initial.insert(0);
	dont_care.final.insert(0);
	dont_care.delta.add(0, DONT_CARE, 1);
	dont_care.delta.add(1, 0, 0);
	const Nft complemented{complement(dont_care, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(dont_care.is_in_lang(word) != complemented.is_in_lang(word));
	}
}

// A hidden sweep over larger transducers and longer words; the default run stays fast.
TEST_CASE("mata::nft differential: larger sweep", "[.differential-sweep]") {
	for (unsigned seed{0}; seed < 300; ++seed) {
		const size_t num_of_symbols{1 + seed % 3};
		const size_t num_of_levels{2 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		std::mt19937 lhs_rng{seed};
		std::mt19937 rhs_rng{seed + 900};
		// No DONT_CARE in the operands of complement(): it does not expand the symbol, which the
		//  [!shouldfail] test case above pins down.
		const Nft lhs{random_nft(lhs_rng, 1 + seed % 4, num_of_symbols, num_of_levels)};
		const Nft rhs{random_nft(rhs_rng, 1 + seed % 4, num_of_symbols, num_of_levels)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 900, rhs));

		const Nft complemented{complement(lhs, *alphabet)};
		for (const Word& word : all_level_words(num_of_symbols, 3 * num_of_levels, num_of_levels)) {
			CHECK(lhs.is_in_lang(word) != complemented.is_in_lang(word));
		}
		CHECK(
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "naive"}}) ==
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "antichains"}})
		);
		// No universality here: it disagrees with the complement on every multi-level transducer,
		//  which the [!shouldfail] test case above pins down.

		// The composition modes, on two-level operands that do carry DONT_CARE.
		std::mt19937 composed_lhs_rng{seed + 1'300};
		std::mt19937 composed_rhs_rng{seed + 1'700};
		const Nft composed_lhs{random_nft(composed_lhs_rng, 1 + seed % 3, num_of_symbols, 2, true)};
		const Nft composed_rhs{random_nft(composed_rhs_rng, 1 + seed % 3, num_of_symbols, 2, true)};
		const Nft general{
			compose(composed_lhs, composed_rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::General)
		};
		const Nft fast{
			compose(composed_lhs, composed_rhs, {1}, {0}, true, JumpMode::NoJump, CompositionMode::FastNoJump)
		};
		CHECK(general.levels.num_of_levels == fast.levels.num_of_levels);
		for (const Word& word : all_level_words(num_of_symbols, 6, 2)) {
			CHECK(general.is_in_lang(word) == fast.is_in_lang(word));
		}
	}
}
