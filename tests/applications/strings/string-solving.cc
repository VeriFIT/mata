/* tests-nfa-strings.cc -- Tests for string solving operations.
 */

#include <random>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"

using namespace mata::nfa;
using namespace mata::applications::strings;
using namespace mata::applications::strings::seg_nfa;
using namespace mata::utils;
using namespace mata::nfa::builder;

using Symbol = mata::Symbol;
using Word = mata::Word;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x)                                                                                             \
	(x).initial = {1, 3};                                                                                              \
	(x).final = {5};                                                                                                   \
	(x).delta.add(1, 'a', 3);                                                                                          \
	(x).delta.add(1, 'a', 10);                                                                                         \
	(x).delta.add(1, 'b', 7);                                                                                          \
	(x).delta.add(3, 'a', 7);                                                                                          \
	(x).delta.add(3, 'b', 9);                                                                                          \
	(x).delta.add(9, 'a', 9);                                                                                          \
	(x).delta.add(7, 'b', 1);                                                                                          \
	(x).delta.add(7, 'a', 3);                                                                                          \
	(x).delta.add(7, 'c', 3);                                                                                          \
	(x).delta.add(10, 'a', 7);                                                                                         \
	(x).delta.add(10, 'b', 7);                                                                                         \
	(x).delta.add(10, 'c', 7);                                                                                         \
	(x).delta.add(7, 'a', 5);                                                                                          \
	(x).delta.add(5, 'a', 5);                                                                                          \
	(x).delta.add(5, 'c', 9);

// Automaton B
#define FILL_WITH_AUT_B(x)                                                                                             \
	(x).initial = {4};                                                                                                 \
	(x).final = {2, 12};                                                                                               \
	(x).delta.add(4, 'c', 8);                                                                                          \
	(x).delta.add(4, 'a', 8);                                                                                          \
	(x).delta.add(8, 'b', 4);                                                                                          \
	(x).delta.add(4, 'a', 6);                                                                                          \
	(x).delta.add(4, 'b', 6);                                                                                          \
	(x).delta.add(6, 'a', 2);                                                                                          \
	(x).delta.add(2, 'b', 2);                                                                                          \
	(x).delta.add(2, 'a', 0);                                                                                          \
	(x).delta.add(0, 'a', 2);                                                                                          \
	(x).delta.add(2, 'c', 12);                                                                                         \
	(x).delta.add(12, 'a', 14);                                                                                        \
	(x).delta.add(14, 'b', 12);

// }}}

TEST_CASE("mata::nfa::get_shortest_words()") {
	Nfa aut('q' + 1);

	SECTION("Automaton B") {
		FILL_WITH_AUT_B(aut);
		Word word{};
		word.push_back('b');
		word.push_back('a');
		std::set<Word> expected{word};
		Word word2{};
		word2.push_back('a');
		word2.push_back('a');
		expected.insert(expected.begin(), word2);
		REQUIRE(get_shortest_words(aut) == expected);

		SECTION("Additional initial state with longer words") {
			aut.initial.insert(8);
			REQUIRE(get_shortest_words(aut) == expected);
		}

		SECTION("Change initial state") {
			aut.initial.clear();
			aut.initial.insert(8);

			word.clear();
			word.push_back('b');
			word.push_back('b');
			word.push_back('a');
			expected = std::set<Word>{word};
			word2.clear();
			word2.push_back('b');
			word2.push_back('a');
			word2.push_back('a');
			expected.insert(expected.begin(), word2);

			REQUIRE(get_shortest_words(aut) == expected);
		}
	}

	SECTION("Empty automaton") { REQUIRE(get_shortest_words(aut).empty()); }

	SECTION("One-state automaton accepting an empty language") {
		aut.initial.insert(0);
		REQUIRE(get_shortest_words(aut).empty());
		aut.final.insert(1);
		REQUIRE(get_shortest_words(aut).empty());
		aut.final.insert(0);
		REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{}});
	}

	SECTION("Automaton A") {
		FILL_WITH_AUT_A(aut);
		Word word{};
		word.push_back('b');
		word.push_back('a');
		std::set<Word> expected{word};
		Word word2{};
		word2.push_back('a');
		word2.push_back('a');
		expected.insert(expected.begin(), word2);
		REQUIRE(get_shortest_words(aut) == expected);
	}

	SECTION("Single transition automaton") {
		aut.initial = {1};
		aut.final = {2};
		aut.delta.add(1, 'a', 2);

		REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{'a'}});
	}

	SECTION("Single state automaton") {
		aut.initial = {1};
		aut.final = {1};
		aut.delta.add(1, 'a', 1);

		REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{}});
	}

	SECTION("Require FIFO queue") {
		aut.initial = {1};
		aut.final = {4};
		aut.delta.add(1, 'a', 5);
		aut.delta.add(5, 'c', 4);
		aut.delta.add(1, 'a', 2);
		aut.delta.add(2, 'b', 3);
		aut.delta.add(3, 'b', 4);

		Word word{};
		word.push_back('a');
		word.push_back('c');
		std::set<Word> expected{word};

		// LIFO queue would return as shortest words string "abb", which would be incorrect.
		REQUIRE(get_shortest_words(aut) == expected);
	}
}

TEST_CASE("mata::nfa::get_shortest_words() for profiling", "[.profiling][shortest_words]") {
	Nfa aut('q' + 1);
	FILL_WITH_AUT_B(aut);
	aut.initial.clear();
	aut.initial.insert(8);
	Word word{};
	word.push_back('b');
	word.push_back('b');
	word.push_back('a');
	std::set<Word> expected{word};
	Word word2{};
	word2.push_back('b');
	word2.push_back('a');
	word2.push_back('a');
	expected.insert(expected.begin(), word2);

	for (size_t n{}; n < 100'000; ++n) { get_shortest_words(aut); }
}

namespace {
/// The longest words by the lazy variant, in the order yielded, or @c std::nullopt for an infinite language.
std::optional<std::vector<Word>> lazy_longest_words(const Nfa& nfa) {
#if MATA_HAS_GENERATOR_SUPPORT
	std::optional<std::generator<Word>> words{get_longest_words_lazy(nfa)};
	if (!words.has_value()) { return std::nullopt; }
	std::vector<Word> result{};
	for (Word&& word : *words) { result.push_back(std::move(word)); }
	return result;
#else
	const std::optional<std::set<Word>> words{get_longest_words(nfa)};
	if (!words.has_value()) { return std::nullopt; }
	return std::vector<Word>(words->begin(), words->end());
#endif
}

/// Checks that both variants find @p expected (@c std::nullopt for an infinite language), the lazy one in
///  lexicographic order.
void check_longest_words(const Nfa& nfa, const std::optional<std::set<Word>>& expected) {
	CHECK(get_longest_words(nfa) == expected);
	const std::optional<std::vector<Word>> lazy{lazy_longest_words(nfa)};
	REQUIRE(lazy.has_value() == expected.has_value());
	if (expected.has_value()) { CHECK(*lazy == std::vector<Word>(expected->begin(), expected->end())); }
}

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }
} // namespace

TEST_CASE("mata::applications::strings::get_longest_words()") {
	Nfa aut('q' + 1);

	SECTION("Empty automaton") { check_longest_words(aut, std::set<Word>{}); }

	SECTION("No final state reachable") {
		aut.initial = {0};
		aut.final = {2};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'a', 1);
		check_longest_words(aut, std::set<Word>{});
	}

	SECTION("Only the empty word") {
		aut.initial = {0};
		aut.final = {0};
		check_longest_words(aut, std::set<Word>{Word{}});
	}

	SECTION("Self-loop on a useful state") {
		aut.initial = {1};
		aut.final = {1};
		aut.delta.add(1, 'a', 1);
		check_longest_words(aut, std::nullopt);
	}

	SECTION("Cycles through useful states") {
		FILL_WITH_AUT_A(aut);
		check_longest_words(aut, std::nullopt);
		aut.clear();
		FILL_WITH_AUT_B(aut);
		check_longest_words(aut, std::nullopt);
	}

	SECTION("Cycles through useless states only") {
		aut.initial = {0};
		aut.final = {2};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'b', 2);
		// A non-terminating cycle reachable from the initial state.
		aut.delta.add(0, 'c', 3);
		aut.delta.add(3, 'c', 3);
		// An unreachable cycle leading to the final state.
		aut.delta.add(4, 'a', 5);
		aut.delta.add(5, 'a', 4);
		aut.delta.add(5, 'a', 2);
		check_longest_words(aut, std::set<Word>{Word{'a', 'b'}});
	}

	SECTION("Shorter words are dropped and words spelled by several paths come once") {
		aut.initial = {0, 10};
		aut.final = {3, 1, 12};
		// 0 accepts "a", "ab", "abc", "bbc" ("abc" twice).
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'b', 2);
		aut.delta.add(2, 'c', 3);
		aut.delta.add(0, 'a', 4);
		aut.delta.add(4, 'b', 5);
		aut.delta.add(5, 'c', 3);
		aut.delta.add(0, 'b', 6);
		aut.delta.add(6, 'b', 2);
		aut.delta.add(1, 'b', 3);
		// 10 accepts only "aa", shorter than the longest words of 0.
		aut.delta.add(10, 'a', 11);
		aut.delta.add(11, 'a', 12);
		check_longest_words(aut, std::set<Word>{Word{'a', 'b', 'c'}, Word{'b', 'b', 'c'}});

		SECTION("Another initial state with longest words of the same length") {
			aut.delta.add(12, 'a', 13);
			aut.final.insert(13);
			check_longest_words(aut, std::set<Word>{Word{'a', 'a', 'a'}, Word{'a', 'b', 'c'}, Word{'b', 'b', 'c'}});
		}
	}

	SECTION("Longest path through a final state") {
		aut.initial = {0};
		aut.final = {1, 3};
		aut.delta.add(0, 'a', 1);
		aut.delta.add(1, 'b', 2);
		aut.delta.add(2, 'c', 3);
		aut.delta.add(0, 'b', 3);
		check_longest_words(aut, std::set<Word>{Word{'a', 'b', 'c'}});
	}
}

TEST_CASE("mata::applications::strings::get_longest_words() agrees with the words up to the number of states") {
	for (unsigned seed{0}; seed < 500; ++seed) {
		INFO("seed " << seed);
		std::mt19937 rng{seed};
		const size_t num_of_states{1 + draw(rng, 10)};
		const size_t num_of_symbols{1 + draw(rng, 3)};
		// Short forward transitions keep many languages finite and their words long; a backward one may close a cycle.
		const size_t backward_odds{4 + draw(rng, 60)};
		Nfa nfa{num_of_states};
		for (size_t initial{0}, initials{1 + draw(rng, 2)}; initial < initials; ++initial) {
			nfa.initial.insert(static_cast<State>(draw(rng, (num_of_states + 1) / 2)));
		}
		for (State state{0}; state < num_of_states; ++state) {
			if (draw(rng, 3) == 0) { nfa.final.insert(state); }
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
					const bool backward{draw(rng, backward_odds) == 0};
					if (!backward && state + 1 == num_of_states) { continue; }
					const size_t target{
						backward ? draw(rng, state + 1)
								 : state + 1 + draw(rng, std::min<size_t>(3, num_of_states - state - 1))
					};
					nfa.delta.add(state, symbol, static_cast<State>(target));
				}
			}
		}
		INFO(nfa.print_to_mata());

		Nfa trimmed{nfa};
		trimmed.trim();
		if (!trimmed.is_acyclic()) {
			check_longest_words(nfa, std::nullopt);
			continue;
		}
		// An acyclic trimmed automaton accepts only words shorter than its number of states.
		const std::set<Word> words{nfa.get_words(num_of_states)};
		size_t longest_length{0};
		for (const Word& word : words) { longest_length = std::max(longest_length, word.size()); }
		std::set<Word> expected{};
		for (const Word& word : words) {
			if (word.size() == longest_length) { expected.insert(word); }
		}
		check_longest_words(nfa, expected);
	}
}

TEST_CASE("mata::applications::strings::get_lengths()") {
	SECTION("basic") {
		Nfa x = create_from_regex("(abcde)*");
		x.trim();
		CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0, 5}}));
	}

	SECTION("basic2") {
		Nfa x = create_from_regex("a+");
		x.trim();
		CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{1, 1}}));
	}

	SECTION("basic3") {
		Nfa x = create_from_regex("a*");
		x.trim();
		CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0, 1}}));
	}

	SECTION("empty") {
		Nfa x = create_from_regex("");
		x.trim();
		CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0, 0}}));
	}

	SECTION("finite") {
		Nfa x = create_from_regex("abcd");
		x.trim();
		CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{4, 0}}));
	}

	SECTION("advanced 1") {
		Nfa x = create_from_regex("(cd(abcde)*)|(a(aaa)*)");
		CHECK(
			get_word_lengths(x) ==
			std::set<std::pair<int, int>>({{1, 0}, {2, 15}, {4, 15}, {7, 15}, {10, 15}, {12, 15}, {13, 15}, {16, 15}})
		);
	}

	SECTION("advanced 2") {
		Nfa x = create_from_regex("a(aaaa|aaaaaaa)*");
		CHECK(
			get_word_lengths(x) == std::set<std::pair<int, int>>(
									   {{1, 0},
										{5, 0},
										{8, 0},
										{9, 0},
										{12, 0},
										{13, 0},
										{15, 0},
										{16, 0},
										{17, 0},
										{19, 0},
										{20, 0},
										{21, 0},
										{22, 0},
										{23, 0},
										{24, 0},
										{25, 0},
										{26, 1}}
								   )
		);
	}
}

TEST_CASE("mata::applications::strings::is_lang_eps()") {
	SECTION("basic") {
		Nfa x = create_from_regex("(abcde)*");
		CHECK(!is_lang_eps(x));
	}

	SECTION("basic 2") {
		Nfa x = create_from_regex("");
		CHECK(is_lang_eps(x));
	}

	SECTION("basic 3") {
		Nfa x;
		CHECK(!is_lang_eps(x));
	}
}

TEST_CASE("mata::nfa::create_single_word_nfa()") {
	SECTION("From numbers") {
		SECTION("Simple word") {
			std::vector<mata::Symbol> word{10, 20, 30, 40, 50, 60};
			auto nfa{builder::create_single_word_nfa(word)};
			CHECK(nfa.is_in_lang(word));
			CHECK(nfa.final.size() == 1);
			CHECK(nfa.initial.size() == 1);
			CHECK(
				mata::applications::strings::get_word_lengths(nfa) ==
				std::set<std::pair<int, int>>{std::make_pair(6, 0)}
			);
		}

		SECTION("Empty string") {
			std::vector<mata::Symbol> word{};
			auto nfa{builder::create_single_word_nfa(word)};
			CHECK(nfa.is_in_lang(word));
			CHECK(mata::applications::strings::is_lang_eps(nfa));
			CHECK(nfa.final.size() == 1);
			CHECK(nfa.initial.size() == 1);
			CHECK(
				mata::applications::strings::get_word_lengths(nfa) ==
				std::set<std::pair<int, int>>{std::make_pair(0, 0)}
			);
		}
	}

	SECTION("From symbol names") {
		SECTION("Simple word") {
			std::vector<std::string> word{"zero", "one", "two", "three", "four", "five"};
			auto nfa{builder::create_single_word_nfa(word)};
			CHECK(nfa.is_in_lang(nfa.alphabet->translate_word(word)));
			CHECK(nfa.final.size() == 1);
			CHECK(nfa.initial.size() == 1);
			CHECK(
				mata::applications::strings::get_word_lengths(nfa) ==
				std::set<std::pair<int, int>>{std::make_pair(6, 0)}
			);
		}

		SECTION("Empty string") {
			std::vector<mata::Symbol> word{};
			auto nfa{builder::create_single_word_nfa(word)};
			CHECK(nfa.is_in_lang(word));
			CHECK(mata::applications::strings::is_lang_eps(nfa));
			CHECK(nfa.final.size() == 1);
			CHECK(nfa.initial.size() == 1);
			CHECK(
				mata::applications::strings::get_word_lengths(nfa) ==
				std::set<std::pair<int, int>>{std::make_pair(0, 0)}
			);
		}

		SECTION("Simple word with alphabet") {
			std::vector<std::string> word{"zero", "one", "two", "three", "four", "five"};
			mata::OnTheFlyAlphabet alphabet{};
			for (mata::Symbol symbol{0}; symbol < word.size(); ++symbol) {
				alphabet.add_new_symbol(word[symbol], symbol);
			}
			auto nfa{builder::create_single_word_nfa(word)};
			CHECK(nfa.is_in_lang(nfa.alphabet->translate_word(word)));
			CHECK(nfa.final.size() == 1);
			CHECK(nfa.initial.size() == 1);
			CHECK(
				mata::applications::strings::get_word_lengths(nfa) ==
				std::set<std::pair<int, int>>{std::make_pair(6, 0)}
			);
		}
	}
}

TEST_CASE("mata::applications::strings::get_accepted_symbols()") {
	Nfa x;
	std::set<mata::Symbol> symbols;

	SECTION("basic") {
		x = create_from_regex("a|bc");
		symbols = {'a'};
		CHECK(get_accepted_symbols(x) == symbols);
	}

	SECTION("basic 2") {
		x = create_from_regex("");
		CHECK(get_accepted_symbols(x).empty());
	}

	SECTION("basic 3") { CHECK(get_accepted_symbols(x).empty()); }

	SECTION("advanced 1") {
		x = create_from_regex("a*|c+|(db)*");
		symbols = {'a', 'c'};
		CHECK(get_accepted_symbols(x) == symbols);
	}

	SECTION("advanced 2") {
		(x).delta.add(0, 'a', 1);
		(x).delta.add(0, 'b', 1);
		(x).delta.add(2, 'c', 3);
		(x).delta.add(2, 'd', 4);
		(x).delta.add(4, 'e', 2);
		(x).delta.add(2, 'f', 2);
		(x).delta.add(5, 'g', 1);
		(x).initial = {0, 2, 4};
		(x).final = {1, 3, 2};
		symbols = {'a', 'b', 'c', 'e', 'f'};
		CHECK(get_accepted_symbols(x) == symbols);
	}
}

TEST_CASE("mata::applications::strings::get_words_of_lengths()") {
	using mata::nft::Levels;
	using mata::nft::Nft;

	auto has_lengths = [](const std::vector<Word>& words, const std::vector<unsigned>& lengths) {
		if (words.size() != lengths.size()) { return false; }
		for (size_t i{0}; i < lengths.size(); ++i) {
			if (words[i].size() != lengths[i]) { return false; }
		}
		return true;
	};

	SECTION("epsilon cycle") {
		// 0 -eps-> 1 -eps-> 0 is an epsilon cycle; 0 -a-> 2 -b-> 3 accepts the pair (a, b).
		Nft nft{Nft::with_levels(Levels{2, {0, 1, 1, 0}}, 4, {0}, {3})};
		nft.delta.add(0, mata::nft::EPSILON, 1);
		nft.delta.add(1, mata::nft::EPSILON, 0);
		nft.delta.add(0, 'a', 2);
		nft.delta.add(2, 'b', 3);

		const std::optional<std::vector<Word>> words{get_words_of_lengths(nft, {1, 1})};
		REQUIRE(words.has_value());
		CHECK(*words == std::vector<Word>{Word{'a'}, Word{'b'}});
		CHECK(nft.is_in_lang_by_levels(*words));
		// No pair of lengths (2, 2) exists; the epsilon cycle must not keep the search going.
		CHECK(!get_words_of_lengths(nft, {2, 2}).has_value());
	}

	SECTION("every accepted pair has equal lengths") {
		// Three level-0 states and three level-1 states, fully connected by 'a' in both directions, plus the only
		// path to the final state: 0 -b-> 3 -b-> 6.
		constexpr mata::nft::State k{3};
		Nft nft{Nft::with_levels(Levels{2, {0, 0, 0, 1, 1, 1, 0}}, 2 * k + 1, {0}, {2 * k})};
		for (mata::nft::State i{0}; i < k; ++i) {
			for (mata::nft::State j{0}; j < k; ++j) {
				nft.delta.add(i, 'a', k + j);
				nft.delta.add(k + j, 'a', i);
			}
		}
		nft.delta.add(0, 'b', k);
		nft.delta.add(k, 'b', 2 * k);

		// Unequal lengths have no solution. The path tree is exponential in the lengths, the configuration space is
		// not, so this must stay fast.
		CHECK(!get_words_of_lengths(nft, {20, 21}).has_value());
		const std::vector<unsigned> lengths{20, 20};
		const std::optional<std::vector<Word>> words{get_words_of_lengths(nft, lengths)};
		REQUIRE(words.has_value());
		CHECK(has_lengths(*words, lengths));
		CHECK(nft.is_in_lang_by_levels(*words));
	}

	SECTION("empty words") {
		Nft nft{Nft::with_levels(Levels{2, {0, 1, 0}}, 3, {0}, {0, 2})};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);
		CHECK(get_words_of_lengths(nft, {0, 0}) == std::vector<Word>{Word{}, Word{}});
		// A final initial state is not a solution for non-zero lengths.
		CHECK(get_words_of_lengths(nft, {1, 1}) == std::vector<Word>{Word{'a'}, Word{'b'}});
		CHECK(!get_words_of_lengths(nft, {1, 0}).has_value());
	}

	SECTION("several initial states and three levels") {
		Nft nft{Nft::with_levels(Levels{3, {0, 1, 2, 0, 0, 1, 2, 0}}, 8, {0, 4}, {7})};
		// 0 -a-> 1 -b-> 2 -c-> 3 is a dead end, 4 -x-> 5 -y-> 6 -z-> 7 accepts (x, y, z).
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);
		nft.delta.add(2, 'c', 3);
		nft.delta.add(4, 'x', 5);
		nft.delta.add(5, 'y', 6);
		nft.delta.add(6, 'z', 7);
		CHECK(get_words_of_lengths(nft, {1, 1, 1}) == std::vector<Word>{Word{'x'}, Word{'y'}, Word{'z'}});
		CHECK(!get_words_of_lengths(nft, {2, 2, 2}).has_value());
	}

	SECTION("no solution without states to search") {
		const Nft nft{Nft::with_levels(Levels{2, {0, 1}}, 2, {0}, {})};
		CHECK(!get_words_of_lengths(nft, {0, 0}).has_value());
	}

	SECTION("rejected inputs") {
		Nft nft{Nft::with_levels(Levels{2, {0, 1, 0}}, 3, {0}, {2})};
		nft.delta.add(0, 'a', 1);
		nft.delta.add(1, 'b', 2);
		CHECK_THROWS_AS(get_words_of_lengths(nft, {1}), std::invalid_argument);
		CHECK_THROWS_AS(get_words_of_lengths(nft, {1, 1, 1}), std::invalid_argument);
		// 0 and 2 are both on level 0, so this transition jumps over level 1.
		nft.delta.add(0, 'c', 2);
		CHECK_THROWS_AS(get_words_of_lengths(nft, {1, 1}), std::invalid_argument);
	}

	SECTION("lengths that cannot be spanned are rejected, not wrapped") {
		Nft nft{Nft::with_levels(Levels{2, {0, 1}}, 2, {0}, {1})};
		nft.delta.add(0, 'a', 1);
		// Where size_t is as wide as unsigned, the first length alone already makes 'length + 1' wrap to zero; on a
		// 64-bit build it takes both of them to overflow the product.
		CHECK_THROWS_AS(
			get_words_of_lengths(nft, {std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max()}),
			std::invalid_argument
		);
	}
}
