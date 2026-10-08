/* nfa-differential.cc -- Seeded randomized differential tests of the NFA algorithms.
 *
 * Every automaton is generated from raw std::mt19937 output (std::shuffle and the distributions
 *  are implementation-defined and would differ between libstdc++ and libc++), and every property
 *  is checked against a reference acceptor that uses no library algorithm.
 */

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <random>
#include <set>
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
using mata::nfa::ParameterMap;

namespace {

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A random NFA over the alphabet {0, ..., num_of_symbols - 1} with an explicit alphabet attached.
///  With @p epsilon set, the automaton also gets epsilon transitions, both arbitrary ones and
///  forward chains q -> q + 1, which the removal has to contract transitively.
Nfa random_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const std::shared_ptr<EnumAlphabet>& alphabet,
	const std::optional<Symbol> epsilon = std::nullopt
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
		if (epsilon.has_value()) {
			if (draw(rng, 4) == 0) { nfa.delta.add(state, *epsilon, draw(rng, num_of_states)); }
			if (state + 1 < num_of_states && draw(rng, 3) == 0) { nfa.delta.add(state, *epsilon, state + 1); }
		}
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
bool reference_accepts(const Nfa& nfa, const Word& word, const Symbol epsilon = EPSILON) {
	auto close = [&](std::set<mata::nfa::State>& states) {
		std::vector<mata::nfa::State> worklist{states.begin(), states.end()};
		while (!worklist.empty()) {
			const mata::nfa::State state{worklist.back()};
			worklist.pop_back();
			for (const SymbolPost& symbol_post : nfa.delta[state]) {
				if (symbol_post.symbol != epsilon) { continue; }
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
bool same_language_on(const Nfa& lhs, const Nfa& rhs, const std::vector<Word>& words, const Symbol epsilon = EPSILON) {
	for (const Word& word : words) {
		if (reference_accepts(lhs, word, epsilon) != reference_accepts(rhs, word, epsilon)) { return false; }
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

std::shared_ptr<EnumAlphabet> alphabet_of(const size_t num_of_symbols) {
	auto alphabet{std::make_shared<EnumAlphabet>()};
	for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
	return alphabet;
}

constexpr size_t MAX_WORD_LENGTH{5};

const ParameterMap SIMULATION{{"algorithm", "simulation"}, {"type", "after"}, {"direction", "forward"}};
const ParameterMap RESIDUAL_AFTER{{"algorithm", "residual"}, {"type", "after"}, {"direction", "forward"}};
const ParameterMap RESIDUAL_WITH{{"algorithm", "residual"}, {"type", "with"}, {"direction", "forward"}};
const ParameterMap BRZOZOWSKI{{"algorithm", "brzozowski"}};
const ParameterMap HOPCROFT{{"algorithm", "hopcroft"}};
const ParameterMap NAIVE{{"algorithm", "naive"}};
const ParameterMap ANTICHAINS{{"algorithm", "antichains"}};

/// Every property of one random automaton (and one random companion) that holds on the devel
///  implementation, checked on all words up to @p max_word_length.
void check_nfa_properties(
	const unsigned seed, const size_t num_of_states, const size_t num_of_symbols, const size_t max_word_length
) {
	std::mt19937 rng{seed};
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
	const Nfa nfa{random_nfa(rng, num_of_states, num_of_symbols, alphabet)};
	const std::vector<Word> words{all_words(num_of_symbols, max_word_length)};
	INFO(describe(seed, nfa));

	CHECK(same_language_on(determinize(nfa), nfa, words));
	CHECK(same_language_on(Nfa{nfa}.trim(), nfa, words));

	// Both reduction algorithms, and both ways the residual one can order its two phases, preserve
	//  the language. The residual algorithm rejects a state renaming, so none is asked for.
	CHECK(same_language_on(reduce(nfa, nullptr, SIMULATION), nfa, words));
	CHECK(same_language_on(reduce(nfa, nullptr, RESIDUAL_AFTER), nfa, words));
	CHECK(same_language_on(reduce(nfa, nullptr, RESIDUAL_WITH), nfa, words));

	// Brzozowski minimizes the automaton itself; Hopcroft needs a determinized and trimmed input
	//  (see #734), and then has to agree with Brzozowski both in language and in state count.
	const Nfa brzozowski{minimize(nfa, BRZOZOWSKI)};
	CHECK(same_language_on(brzozowski, nfa, words));
	Nfa deterministic{determinize(nfa)};
	deterministic.trim();
	const Nfa hopcroft{minimize(deterministic, HOPCROFT)};
	CHECK(same_language_on(hopcroft, nfa, words));
	if (!deterministic.final.empty()) { CHECK(hopcroft.num_of_states() == brzozowski.num_of_states()); }

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
	const bool naive_inclusion{is_included(nfa, other, alphabet.get(), NAIVE)};
	const bool antichain_inclusion{is_included(nfa, other, alphabet.get(), ANTICHAINS)};
	CHECK(naive_inclusion == antichain_inclusion);
	if (naive_inclusion) { CHECK(included_on(nfa, other, words)); }

	// The same for both equivalence algorithms.
	const bool naive_equivalence{are_equivalent(nfa, other, alphabet.get(), NAIVE)};
	const bool antichain_equivalence{are_equivalent(nfa, other, alphabet.get(), ANTICHAINS)};
	CHECK(naive_equivalence == antichain_equivalence);
	if (naive_equivalence) { CHECK(same_language_on(nfa, other, words)); }
}

/// Removing @p epsilon keeps the language, also after determinization.
void check_epsilon_removal(
	const unsigned seed,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const size_t max_word_length,
	const Symbol epsilon
) {
	std::mt19937 rng{seed};
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
	const Nfa nfa{random_nfa(rng, num_of_states, num_of_symbols, alphabet, epsilon)};
	const std::vector<Word> words{all_words(num_of_symbols, max_word_length)};
	INFO(describe(seed, nfa));

	const Nfa without_epsilon{remove_epsilon(nfa, epsilon)};
	CHECK(same_language_on(without_epsilon, nfa, words, epsilon));
	CHECK(same_language_on(determinize(without_epsilon), nfa, words, epsilon));
}

} // Anonymous namespace.

TEST_CASE("mata::nfa differential: the algorithms agree with the reference acceptor", "[differential]") {
	for (unsigned seed{0}; seed < 200; ++seed) {
		check_nfa_properties(seed, 2 + seed % 5, 1 + seed % 3, MAX_WORD_LENGTH);
	}
}

TEST_CASE("mata::nfa differential: epsilon removal keeps the language", "[differential]") {
	for (unsigned seed{0}; seed < 150; ++seed) {
		check_epsilon_removal(seed, 2 + seed % 5, 1 + seed % 2, MAX_WORD_LENGTH, EPSILON);
		// A non-default epsilon that is larger than every ordinary symbol: the removal must not
		//  assume the default one, and the ordinary symbols must survive it.
		check_epsilon_removal(seed, 2 + seed % 5, 1 + seed % 2, MAX_WORD_LENGTH, 100);
	}
}

// No randomized check of minimize(nfa, hopcroft) on a raw NFA (#734): minimize_hopcroft() guards its
//  determinism precondition with an assertion, so such a check aborts the Debug build instead of
//  failing, and [!shouldfail] cannot express it. The legal use of Hopcroft, on a determinized and
//  trimmed input, is checked above; the raw-NFA property belongs with the fix for #734.

// A hidden sweep over larger automata and longer words; the tagged-out default run stays fast.
TEST_CASE("mata::nfa differential: larger sweep", "[.differential-sweep]") {
	for (unsigned seed{0}; seed < 600; ++seed) {
		check_nfa_properties(seed, 2 + seed % 7, 1 + seed % 3, 6);
		check_epsilon_removal(seed, 2 + seed % 7, 1 + seed % 2, 6, EPSILON);
		check_epsilon_removal(seed, 2 + seed % 7, 1 + seed % 2, 6, 100);
	}
}
