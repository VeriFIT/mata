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
#include <utility>
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

/// A partial deterministic automaton over the alphabet {0, ..., num_of_symbols - 1}: every (state, symbol) has at
///  most one target. A union of @c k such automata determinizes into at most the product of (|component| + 1)
///  macrostates, which keeps the naive reference algorithms affordable, while the reachable macrostates still
///  vary in cardinality between 0 and @c k.
Nfa random_partial_dfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const std::shared_ptr<EnumAlphabet>& alphabet
) {
	Nfa nfa{num_of_states};
	nfa.alphabet = alphabet;
	nfa.initial.insert(draw(rng, num_of_states));
	for (mata::nfa::State state{0}; state < num_of_states; ++state) {
		if (draw(rng, 2) == 0) { nfa.final.insert(state); }
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
			if (draw(rng, 5) != 0) { nfa.delta.add(state, symbol, draw(rng, num_of_states)); }
		}
	}
	return nfa;
}

/// A dense random NFA: half of the states final and @c 1.75 * num_of_states transitions per symbol, placed
///  uniformly at random. This is the shape that keeps the antichains of the inclusion check deep.
Nfa random_dense_nfa(
	std::mt19937& rng,
	const size_t num_of_states,
	const size_t num_of_symbols,
	const std::shared_ptr<EnumAlphabet>& alphabet
) {
	Nfa nfa{num_of_states};
	nfa.alphabet = alphabet;
	nfa.initial.insert(draw(rng, num_of_states));
	for (mata::nfa::State state{0}; state < num_of_states; ++state) {
		if (draw(rng, 2) == 0) { nfa.final.insert(state); }
	}
	const size_t num_of_transitions{7 * num_of_states / 4};
	for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
		for (size_t transition{0}; transition < num_of_transitions; ++transition) {
			nfa.delta.add(draw(rng, num_of_states), symbol, draw(rng, num_of_states));
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
const ParameterMap CLASSICAL{{"algorithm", "classical"}};
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

	// Exactly one of the automaton and its complement accepts each word: for both algorithms, and for
	//  the deterministic shortcuts of the classical one (on a copy, and in place on an rvalue).
	for (const Nfa& complemented :
		 {complement(nfa, *alphabet, CLASSICAL), complement(nfa, *alphabet, BRZOZOWSKI),
		  complement(deterministic, *alphabet, CLASSICAL),
		  complement(Nfa{deterministic}, alphabet->get_alphabet_symbols(), CLASSICAL)}) {
		for (const Word& word : words) { CHECK(reference_accepts(complemented, word) != reference_accepts(nfa, word)); }
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
//  trimmed input, is checked above; the raw-NFA property belongs with the fix for #734 (#901).

// A hidden sweep over larger automata and longer words; the tagged-out default run stays fast.
TEST_CASE("mata::nfa differential: larger sweep", "[.differential-sweep]") {
	for (unsigned seed{0}; seed < 600; ++seed) {
		check_nfa_properties(seed, 2 + seed % 7, 1 + seed % 3, 6);
		check_epsilon_removal(seed, 2 + seed % 7, 1 + seed % 2, 6, EPSILON);
		check_epsilon_removal(seed, 2 + seed % 7, 1 + seed % 2, 6, 100);
	}
}

TEST_CASE(
	"mata::nfa differential: the antichain filters stay sound on macrostates wider than 64 states", "[differential]"
) {
	// The antichain algorithms filter their subsumption tests with a 64-bit signature of the macrostate, so states
	//  that are congruent modulo 64 share a bit and two different macrostates can carry the same signature. The
	//  bigger automaton here is a union of three partial DFAs: that bounds determinization (and with it the naive
	//  reference algorithms) by the product of the components, pushes the state indices past 64, and still makes
	//  the reachable macrostates vary in cardinality, which is what reaches the strict-subset buckets of the
	//  processed antichain.
	constexpr size_t COMPONENT_STATES{23};
	constexpr size_t COMPONENTS{3};
	for (unsigned seed{0}; seed < 20; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }

		Nfa bigger{random_partial_dfa(rng, COMPONENT_STATES, num_of_symbols, alphabet)};
		for (size_t component{1}; component < COMPONENTS; ++component) {
			bigger =
				union_nondet(std::move(bigger), random_partial_dfa(rng, COMPONENT_STATES, num_of_symbols, alphabet));
		}
		bigger.alphabet = alphabet;
		REQUIRE(bigger.num_of_states() == COMPONENTS * COMPONENT_STATES);
		REQUIRE(bigger.num_of_states() > 64);
		const Nfa smaller{random_nfa(rng, 2 + seed % 4, num_of_symbols, alphabet)};
		INFO(describe(seed, bigger));
		INFO(describe(seed, smaller));

		// Macrostates of the bigger automaton drive the subsumption tests, so both directions are checked: the
		//  one where the antichain runs over the wide automaton, and the one where the naive reference does.
		CHECK(
			is_included(smaller, bigger, alphabet.get(), {{"algorithm", "naive"}}) ==
			is_included(smaller, bigger, alphabet.get(), {{"algorithm", "antichains"}})
		);
		CHECK(
			is_included(bigger, smaller, alphabet.get(), {{"algorithm", "naive"}}) ==
			is_included(bigger, smaller, alphabet.get(), {{"algorithm", "antichains"}})
		);

		CHECK(
			bigger.is_universal(*alphabet, {{"algorithm", "naive"}}) ==
			bigger.is_universal(*alphabet, {{"algorithm", "antichains"}})
		);

		// A pair whose inclusion holds, so the antichain runs to its fixpoint instead of stopping at the first
		//  counterexample: an intersection with @c bigger accepts only words that @c bigger accepts.
		const Nfa included{intersection(smaller, bigger)};
		CHECK(is_included(included, bigger, alphabet.get(), {{"algorithm", "antichains"}}));
		CHECK(
			is_included(bigger, included, alphabet.get(), {{"algorithm", "naive"}}) ==
			is_included(bigger, included, alphabet.get(), {{"algorithm", "antichains"}})
		);

		// A reported counterexample is a word of the smaller language that the bigger one rejects.
		Run cex{};
		if (!is_included(smaller, bigger, &cex, alphabet.get(), {{"algorithm", "antichains"}})) {
			CHECK(reference_accepts(smaller, cex.word));
			CHECK(!reference_accepts(bigger, cex.word));
		}
	}
}

TEST_CASE("mata::nfa differential: equal antichain signatures never prove a subset", "[differential]") {
	// The states 0, 64 and 65 put {0} and {64} on the same signature, and the signature of {0} inside that of
	//  {64, 65}, yet neither set is a subset of the other. Initial state 1 branches into a live and a dead one of
	//  these sets; the dead one is the only way to a counterexample, so a filter that takes a signature match for a
	//  subset drops it and reports success. Reaching the dead set first or second puts the collision into the
	//  subsumption test of a new macrostate or into the pruning of the stored ones, respectively.
	using SetPair = std::pair<std::vector<mata::nfa::State>, std::vector<mata::nfa::State>>;
	const std::vector<SetPair> live_and_dead{{{0}, {64}}, {{64}, {0}}, {{0}, {64, 65}}, {{64, 65}, {0}}};
	constexpr size_t NUM_OF_STATES{66};
	for (const auto& [live, dead] : live_and_dead) {
		for (const bool dead_first : {false, true}) {
			const Symbol live_symbol{dead_first ? Symbol{1} : Symbol{0}};
			const Symbol dead_symbol{dead_first ? Symbol{0} : Symbol{1}};
			INFO("live " << live.size() << " state(s) from " << live.front() << ", dead_first " << dead_first);

			// Inclusion: smaller accepts {0 2, 1 2}; bigger reads 2 from the live set only. Symbol 3 takes both sets
			//  to the final state 2, so the distance bound does not short-cut the dead branch.
			auto inclusion_alphabet{std::make_shared<EnumAlphabet>()};
			for (Symbol symbol{0}; symbol < 4; ++symbol) { inclusion_alphabet->add_new_symbol(std::to_string(symbol)); }
			Nfa smaller{3};
			smaller.alphabet = inclusion_alphabet;
			smaller.initial.insert(0);
			smaller.final.insert(2);
			smaller.delta.add(0, 0, 1);
			smaller.delta.add(0, 1, 1);
			smaller.delta.add(1, 2, 2);
			Nfa bigger{NUM_OF_STATES};
			bigger.alphabet = inclusion_alphabet;
			bigger.initial.insert(1);
			bigger.final.insert(2);
			for (const mata::nfa::State state : live) {
				bigger.delta.add(1, live_symbol, state);
				bigger.delta.add(state, 2, 2);
				bigger.delta.add(state, 3, 2);
			}
			for (const mata::nfa::State state : dead) {
				bigger.delta.add(1, dead_symbol, state);
				bigger.delta.add(state, 3, 2);
			}
			REQUIRE(!is_included(smaller, bigger, inclusion_alphabet.get(), NAIVE));
			Run inclusion_cex{};
			CHECK(!is_included(smaller, bigger, &inclusion_cex, inclusion_alphabet.get(), ANTICHAINS));
			CHECK(inclusion_cex.word == Word{dead_symbol, 2});

			// Universality over {0, 1}: every set is final, the live one loops on both symbols, the dead one on 0
			//  only, so the dead set followed by 1 is the only rejected word.
			auto universality_alphabet{std::make_shared<EnumAlphabet>()};
			for (Symbol symbol{0}; symbol < 2; ++symbol) {
				universality_alphabet->add_new_symbol(std::to_string(symbol));
			}
			Nfa aut{NUM_OF_STATES};
			aut.alphabet = universality_alphabet;
			aut.initial.insert(1);
			aut.final.insert(1);
			for (const mata::nfa::State state : live) {
				aut.final.insert(state);
				aut.delta.add(1, live_symbol, state);
				aut.delta.add(state, 0, state);
				aut.delta.add(state, 1, state);
			}
			for (const mata::nfa::State state : dead) {
				aut.final.insert(state);
				aut.delta.add(1, dead_symbol, state);
				aut.delta.add(state, 0, state);
			}
			REQUIRE(!aut.is_universal(*universality_alphabet, NAIVE));
			Run universality_cex{};
			CHECK(!aut.is_universal(*universality_alphabet, &universality_cex, ANTICHAINS));
			CHECK(!reference_accepts(aut, universality_cex.word));
		}
	}
}

TEST_CASE("mata::nfa differential: antichain inclusion on dense automata with more than 64 states", "[differential]") {
	// Dense automata keep the processed antichains deep, which is where the cardinality buckets and the signature
	//  filter of the subsumption test actually decide something. Complementing them is out of reach, so the
	//  answers are pinned against a construction that fixes one of them and against the words of a bounded length.
	constexpr size_t NUM_OF_SYMBOLS{2};
	for (unsigned seed{0}; seed < 8; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_states{65 + seed % 6};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < NUM_OF_SYMBOLS; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nfa lhs{random_dense_nfa(rng, num_of_states, NUM_OF_SYMBOLS, alphabet)};
		const Nfa rhs{random_dense_nfa(rng, num_of_states, NUM_OF_SYMBOLS, alphabet)};
		const Nfa united{union_nondet(lhs, rhs)};
		const std::vector<Word> words{all_words(NUM_OF_SYMBOLS, MAX_WORD_LENGTH)};
		INFO(describe(seed, lhs));
		INFO(describe(seed, rhs));

		// L(lhs) is included in L(lhs) ∪ L(rhs) by construction, so this check runs the antichain to its fixpoint.
		CHECK(is_included(lhs, united, alphabet.get(), {{"algorithm", "antichains"}}));

		// The converse normally fails. Either answer has to agree with the words: a reported inclusion admits no
		//  witness among them, a reported counterexample is one.
		Run cex{};
		if (is_included(united, lhs, &cex, alphabet.get(), {{"algorithm", "antichains"}})) {
			CHECK(included_on(united, lhs, words));
		} else {
			CHECK(reference_accepts(united, cex.word));
			CHECK(!reference_accepts(lhs, cex.word));
		}
	}
}

TEST_CASE("mata::nfa differential: the revert variants agree with epsilon transitions", "[differential]") {
	for (unsigned seed{0}; seed < 200; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_states{2 + seed % 5};
		const size_t num_of_symbols{1 + seed % 3};
		auto alphabet{std::make_shared<EnumAlphabet>()};
		for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
		const Nfa nfa{random_nfa(rng, num_of_states, num_of_symbols, alphabet, EPSILON)};
		INFO(describe(seed, nfa));

		// Epsilon is the largest symbol, so the symbol-indexed fragile_revert has to keep it out of its
		//  arrays while the counting sort of revert() does not see any difference.
		const Nfa reverted{revert(nfa)};
		CHECK(reverted.delta == simple_revert(nfa).delta);
		CHECK(reverted.delta == fragile_revert(nfa).delta);
		CHECK(reverted.delta == somewhat_simple_revert(nfa).delta);
		CHECK(reverted.initial == simple_revert(nfa).initial);
		CHECK(reverted.final == simple_revert(nfa).final);
		CHECK(revert(reverted).delta == nfa.delta);
	}
}

TEST_CASE("mata::nfa differential: revert with symbols near EPSILON", "[differential]") {
	// fragile_revert is excluded on purpose: it allocates two vectors per symbol value up to the largest
	//  one (EPSILON-1 here), which is exactly the flaw #751 removes from revert().
	for (unsigned seed{0}; seed < 200; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_states{2 + seed % 4};
		Nfa nfa{num_of_states};
		nfa.initial.insert(draw(rng, num_of_states));
		for (mata::nfa::State state{0}; state < num_of_states; ++state) {
			if (draw(rng, 3) == 0) { nfa.final.insert(state); }
			// Symbols from {0, 1} and right below and at EPSILON, so both ordinary and special symbols mix
			//  in one state post.
			for (const Symbol symbol : {Symbol{0}, Symbol{1}, EPSILON - 2, EPSILON - 1, EPSILON}) {
				for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
					nfa.delta.add(state, symbol, draw(rng, num_of_states));
				}
			}
		}
		INFO(describe(seed, nfa));

		const Nfa reverted{revert(nfa)};
		CHECK(reverted.delta == simple_revert(nfa).delta);
		CHECK(reverted.delta == somewhat_simple_revert(nfa).delta);
		CHECK(reverted.initial == simple_revert(nfa).initial);
		CHECK(reverted.final == simple_revert(nfa).final);
		CHECK(revert(reverted).delta == nfa.delta);
	}
}
