/* nfa-differential.cc -- Seeded randomized differential tests of the NFA algorithms.
 *
 * Every automaton is generated from raw std::mt19937 output (std::shuffle and the distributions
 *  are implementation-defined and would differ between libstdc++ and libc++), and every property
 *  is checked against a reference acceptor that uses no library algorithm.
 */

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using mata::EnumAlphabet;
using mata::Symbol;
using mata::Word;

namespace {

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A random NFA over the alphabet {0, ..., num_of_symbols - 1} with an explicit alphabet attached.
Nfa random_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const std::shared_ptr<EnumAlphabet>& alphabet,
	const bool with_epsilon = false
) {
	Nfa nfa{num_of_states};
	nfa.alphabet = alphabet;
	nfa.initial.insert(draw(rng, num_of_states));
	for (mata::nfa::State state{0}; state < num_of_states; ++state) {
		if (draw(rng, 3) == 0) { nfa.final.insert(state); }
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
			// Each (state, symbol) gets zero, one or two targets.
			for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
				nfa.delta.add(state, symbol, draw(rng, num_of_states));
			}
		}
		if (with_epsilon && draw(rng, 4) == 0) { nfa.delta.add(state, EPSILON, draw(rng, num_of_states)); }
	}
	return nfa;
}

/// Every word over {0, ..., num_of_symbols - 1} of length at most @p max_length.
std::vector<Word> all_words(const size_t num_of_symbols, const size_t max_length) {
	std::vector<Word> words{Word{}};
	size_t level_begin{0};
	for (size_t length{0}; length < max_length; ++length) {
		const size_t level_end{words.size()};
		for (size_t index{level_begin}; index < level_end; ++index) {
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				Word word{words[index]};
				word.push_back(symbol);
				words.push_back(std::move(word));
			}
		}
		level_begin = level_end;
	}
	return words;
}

/// Reference acceptance: a subset simulation over the delta, with an epsilon closure, using no
///  library algorithm beyond reading the transitions.
bool reference_accepts(const Nfa& nfa, const Word& word) {
	auto close = [&](std::set<mata::nfa::State>& states) {
		std::vector<mata::nfa::State> worklist{states.begin(), states.end()};
		while (!worklist.empty()) {
			const mata::nfa::State state{worklist.back()};
			worklist.pop_back();
			for (const SymbolPost& symbol_post : nfa.delta[state]) {
				if (symbol_post.symbol != EPSILON) { continue; }
				for (const mata::nfa::State target : symbol_post.targets) {
					if (states.insert(target).second) { worklist.push_back(target); }
				}
			}
		}
	};

	std::set<mata::nfa::State> current{nfa.initial.begin(), nfa.initial.end()};
	close(current);
	for (const Symbol symbol : word) {
		std::set<mata::nfa::State> next{};
		for (const mata::nfa::State state : current) {
			for (const SymbolPost& symbol_post : nfa.delta[state]) {
				if (symbol_post.symbol != symbol) { continue; }
				next.insert(symbol_post.targets.begin(), symbol_post.targets.end());
			}
		}
		close(next);
		current = std::move(next);
		if (current.empty()) { return false; }
	}
	for (const mata::nfa::State state : current) {
		if (nfa.final.contains(state)) { return true; }
	}
	return false;
}

/// Do the two automata accept the same words among @p words, as judged by the reference acceptor?
bool same_language_on(const Nfa& lhs, const Nfa& rhs, const std::vector<Word>& words) {
	for (const Word& word : words) {
		if (reference_accepts(lhs, word) != reference_accepts(rhs, word)) { return false; }
	}
	return true;
}

/// Is the language of @p lhs included in the language of @p rhs among @p words?
bool included_on(const Nfa& lhs, const Nfa& rhs, const std::vector<Word>& words) {
	for (const Word& word : words) {
		if (reference_accepts(lhs, word) && !reference_accepts(rhs, word)) { return false; }
	}
	return true;
}
/// Dump the automaton without the alphabet: print_to_mata() rejects symbols outside the attached
///  alphabet, and the generated automata use EPSILON.
std::string describe(const unsigned seed, const Nfa& nfa) {
	std::string description{"seed " + std::to_string(seed) + "\ninitial:"};
	for (const mata::nfa::State state : nfa.initial) { description += " " + std::to_string(state); }
	description += "\nfinal:";
	for (const mata::nfa::State state : nfa.final) { description += " " + std::to_string(state); }
	description += "\ntransitions:\n";
	for (const Transition& transition : nfa.delta.transitions()) {
		description += std::to_string(transition.source) + " " + std::to_string(transition.symbol) + " " +
					   std::to_string(transition.target) + "\n";
	}
	return description;
}

constexpr size_t MAX_WORD_LENGTH{5};

} // Anonymous namespace.

TEST_CASE("mata::nfa differential: the algorithms agree with the reference acceptor", "[differential]") {
	for (unsigned seed{0}; seed < 200; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_states{2 + seed % 5};
		const size_t num_of_symbols{1 + seed % 3};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nfa nfa{random_nfa(rng, num_of_states, num_of_symbols, alphabet)};
		const std::vector<Word> words{all_words(num_of_symbols, MAX_WORD_LENGTH)};
		INFO(describe(seed, nfa));

		CHECK(same_language_on(determinize(nfa), nfa, words));
		CHECK(same_language_on(reduce(nfa), nfa, words));
		CHECK(same_language_on(Nfa{nfa}.trim(), nfa, words));
		CHECK(same_language_on(minimize(nfa), nfa, words));

		// The three revert variants build the same transition relation, and reverting twice gives
		//  the original delta back.
		const Nfa reverted{revert(nfa)};
		CHECK(reverted.delta == fragile_revert(nfa).delta);
		CHECK(reverted.delta == somewhat_simple_revert(nfa).delta);
		CHECK(revert(reverted).delta == nfa.delta);

		// Intersection accepts exactly the words both operands accept.
		std::mt19937 other_rng{seed + 1'000};
		const Nfa other{random_nfa(other_rng, num_of_states, num_of_symbols, alphabet)};
		const Nfa product{intersection(nfa, other)};
		for (const Word& word : words) {
			const bool in_both{reference_accepts(nfa, word) && reference_accepts(other, word)};
			CHECK(reference_accepts(product, word) == in_both);
		}

		// Both inclusion algorithms agree with each other and, when they report inclusion, with the
		//  bounded check.
		const bool naive{is_included(nfa, other, alphabet.get(), {{"algorithm", "naive"}})};
		const bool antichains{is_included(nfa, other, alphabet.get(), {{"algorithm", "antichains"}})};
		CHECK(naive == antichains);
		if (naive) { CHECK(included_on(nfa, other, words)); }

		// Equivalence agrees with the bounded language comparison when it reports equivalence.
		if (are_equivalent(nfa, other, alphabet.get())) { CHECK(same_language_on(nfa, other, words)); }
	}
}

TEST_CASE("mata::nfa differential: epsilon removal keeps the language", "[differential]") {
	for (unsigned seed{0}; seed < 150; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_states{2 + seed % 5};
		const size_t num_of_symbols{1 + seed % 2};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nfa nfa{random_nfa(rng, num_of_states, num_of_symbols, alphabet, true)};
		const std::vector<Word> words{all_words(num_of_symbols, MAX_WORD_LENGTH)};
		INFO(describe(seed, nfa));

		const Nfa without_epsilon{remove_epsilon(nfa)};
		CHECK(same_language_on(without_epsilon, nfa, words));
		CHECK(same_language_on(determinize(without_epsilon), nfa, words));
	}
}
