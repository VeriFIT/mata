/* nfa-regex-writer.cc -- Tests for printing NFAs as regular expressions.
 */

#include <array>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using mata::OnTheFlyAlphabet;
using mata::Symbol;
using mata::Word;

namespace {

constexpr std::array MODES{RegexPrintMode::Fast, RegexPrintMode::Readable};

/// The automaton of @p regex as the parser makes it, without the reduction that may hide what the regex is.
Nfa parse(const std::string& regex, const Encoding encoding = Encoding::Latin1) {
	mata::parser::Re2Options options{};
	options.encoding = encoding;
	options.reduce = false;
	return builder::create_from_regex(regex, options);
}

/// A random automaton over @p num_of_symbols letters from 'a', some with an epsilon transition and a second
///  initial state.
Nfa random_nfa(std::mt19937& generator, const size_t max_states, const size_t num_of_symbols) {
	const size_t num_of_states{1 + generator() % max_states};
	const double density{
		std::min(static_cast<double>(num_of_states), 0.3 + static_cast<double>(generator() % 12) / 10)
	};
	const Nfa numbered{builder::create_random_nfa_tabakov_vardi(
		num_of_states, num_of_symbols, density, 0.2 + static_cast<double>(generator() % 5) / 10, generator()
	)};
	Nfa nfa{numbered.num_of_states(), numbered.initial, numbered.final};
	for (const Transition& transition : numbered.delta.transitions()) {
		nfa.delta.add(transition.source, 'a' + transition.symbol, transition.target);
	}
	if (generator() % 3 == 0 && nfa.num_of_states() > 1) {
		const auto random_state{[&] { return static_cast<State>(generator() % nfa.num_of_states()); }};
		nfa.delta.add(random_state(), EPSILON, random_state());
		nfa.initial.insert(random_state());
	}
	return nfa;
}

} // namespace

TEST_CASE("mata::nfa::Nfa::print_to_regex() prints a regex of the language of the automaton") {
	for (const RegexPrintMode mode : MODES) {
		SECTION("automata of regexes, mode " + std::to_string(static_cast<int>(mode))) {
			for (const std::string regex : {
					 "a",
					 "abc",
					 "a*",
					 "(ab)+",
					 "a|b|c",
					 "(a|b)*abb",
					 "abc|abd|aef",
					 "a?b+c*",
					 "(0|1(01*0)*1)*",
					 "hello|help|world",
					 "a{3,5}",
					 "(a{2,3}){2}b{4,}",
					 "(a|b)*a(a|b)(a|b)",
					 "x(ab|cd)*y|x(ab|cd)*z",
					 "(a*b*)*c|(a|bc)+",
					 "[a-z]+@[a-z]+\\.com",
					 "(a|ab)(c|bcd)(d*|e)",
					 "a{40}",
				 }) {
				const Nfa nfa{parse(regex)};
				INFO("regex: " << regex << ", printed: " << nfa.print_to_regex(mode));
				CHECK(are_equivalent(parse(nfa.print_to_regex(mode)), nfa));
			}
		}

		SECTION("random automata, mode " + std::to_string(static_cast<int>(mode))) {
			std::mt19937 generator{20'261'113};
			for (unsigned i{0}; i < 300; ++i) {
				// Small automata: the regex of a larger dense one is too long to decide its language.
				const Nfa nfa{random_nfa(generator, 5, 1 + i % 3)};
				INFO("automaton:\n" << nfa.print_to_mata() << "printed: " << nfa.print_to_regex(mode));
				CHECK(are_equivalent(parse(nfa.print_to_regex(mode)), nfa));
			}
		}
	}
}

TEST_CASE("mata::nfa::Nfa::print_to_regex() of the empty language and of the empty word") {
	for (const RegexPrintMode mode : MODES) {
		SECTION("empty language, mode " + std::to_string(static_cast<int>(mode))) {
			Nfa no_states{};
			Nfa no_final_state{2, {0}, {}};
			no_final_state.delta.add(0, 'a', 1);
			Nfa unreachable_final_state{3, {0}, {2}};
			unreachable_final_state.delta.add(0, 'a', 1);
			unreachable_final_state.delta.add(2, 'a', 2);
			for (const Nfa& nfa : {no_states, no_final_state, unreachable_final_state}) {
				CHECK(nfa.print_to_regex(mode) == "[^\\s\\S]");
				CHECK(parse(nfa.print_to_regex(mode)).is_lang_empty());
			}
		}

		SECTION("empty word, mode " + std::to_string(static_cast<int>(mode))) {
			const Nfa empty_word{builder::create_empty_string_nfa()};
			CHECK(empty_word.print_to_regex(mode) == "()");
			CHECK(are_equivalent(parse(empty_word.print_to_regex(mode)), empty_word));

			// A final state that is initial as well, next to other words.
			Nfa with_empty_word{2, {0}, {0, 1}};
			with_empty_word.delta.add(0, 'a', 1);
			CHECK(are_equivalent(parse(with_empty_word.print_to_regex(mode)), with_empty_word));
		}

		SECTION("epsilon transitions are the empty word, mode " + std::to_string(static_cast<int>(mode))) {
			Nfa nfa{4, {0}, {3}};
			nfa.delta.add(0, 'a', 1);
			nfa.delta.add(1, EPSILON, 2);
			nfa.delta.add(2, EPSILON, 1);
			nfa.delta.add(1, 'b', 1);
			nfa.delta.add(2, 'c', 3);
			nfa.delta.add(0, EPSILON, 3);
			CHECK(are_equivalent(parse(nfa.print_to_regex(mode)), remove_epsilon(nfa)));
		}
	}
}

TEST_CASE("mata::nfa::Nfa::print_to_regex() writes every symbol so that it reads back as itself") {
	for (const RegexPrintMode mode : MODES) {
		SECTION("bytes, mode " + std::to_string(static_cast<int>(mode))) {
			// Each byte alone, in a class with its neighbours, and at both ends of a range in a class.
			Nfa nfa{5, {0}, {4}};
			for (Symbol symbol{0}; symbol <= 0xFF; ++symbol) {
				nfa.delta.add(0, symbol, 1);
				if (symbol % 3 != 0) { nfa.delta.add(1, symbol, 2); }
				if (symbol % 7 < 4) { nfa.delta.add(2, symbol, 3); }
			}
			const Word reserved{'\\', '.', '[', ']', '(', ')', '{', '}', '*',
								'+',  '?', '|', '^', '$', '-', ' ', '<', '>'};
			for (const Symbol symbol : reserved) {
				// One state per symbol, so that the symbol is written outside of a class.
				const State state{nfa.add_state()};
				nfa.delta.add(3, symbol, state);
				nfa.delta.add(state, symbol, 4);
			}
			nfa.delta.add(3, '^', 4);
			nfa.delta.add(3, '-', 4);
			nfa.delta.add(3, ']', 4);
			INFO("printed: " << nfa.print_to_regex(mode));
			CHECK(are_equivalent(parse(nfa.print_to_regex(mode)), nfa));
		}

		SECTION("code points above 0xFF, mode " + std::to_string(static_cast<int>(mode))) {
			Nfa nfa{3, {0}, {2}};
			for (const Symbol symbol : {Symbol{'a'}, Symbol{0xE9}, Symbol{0x1'00}, Symbol{0x1'7F}, Symbol{0x20'AC}}) {
				nfa.delta.add(0, symbol, 1);
			}
			nfa.delta.add(1, 0x1'F6'00, 2);
			nfa.delta.add(1, 0x10'FF'FF, 2);
			nfa.delta.add(1, 0x7'FF, 1);
			INFO("printed: " << nfa.print_to_regex(mode));
			// The parser makes an automaton over the bytes of the UTF-8 encoding.
			CHECK(are_equivalent(parse(nfa.print_to_regex(mode), Encoding::Utf8).decode_utf8(), nfa));
		}

		SECTION("symbols that RE2 cannot represent, mode " + std::to_string(static_cast<int>(mode))) {
			Nfa nfa{2, {0}, {1}};
			for (const Symbol symbol : {Symbol{0x11'00'00}, Symbol{0xD8'00}, Symbol{0xDF'FF}}) {
				nfa.delta.add(0, symbol, 1);
				CHECK_THROWS_AS(nfa.print_to_regex(mode), std::invalid_argument);
			}
			// Unless the symbol is not a part of the language.
			nfa.final = {0};
			CHECK(nfa.print_to_regex(mode) == "()");
		}
	}
}

TEST_CASE("mata::nfa::Nfa::print_to_regex() with an alphabet") {
	Nfa nfa{3, {0}, {2}};
	nfa.delta.add(0, 0, 1);
	nfa.delta.add(1, 1, 1);
	nfa.delta.add(1, 2, 2);
	const Nfa expected{parse("ab*\\*")};

	for (const RegexPrintMode mode : MODES) {
		SECTION("symbols are written as their names, mode " + std::to_string(static_cast<int>(mode))) {
			OnTheFlyAlphabet alphabet{{"a", 0}, {"b", 1}, {"*", 2}};
			CHECK(are_equivalent(parse(nfa.print_to_regex(mode, &alphabet)), expected));
		}

		SECTION("the alphabet of the automaton is used by default, mode " + std::to_string(static_cast<int>(mode))) {
			Nfa named{nfa};
			named.alphabet = std::make_shared<OnTheFlyAlphabet>(OnTheFlyAlphabet{{"a", 0}, {"b", 1}, {"*", 2}});
			CHECK(are_equivalent(parse(named.print_to_regex(mode)), expected));
			// The passed alphabet takes precedence.
			OnTheFlyAlphabet other{{"x", 0}, {"y", 1}, {"z", 2}};
			CHECK(are_equivalent(parse(named.print_to_regex(mode, &other)), parse("xy*z")));
		}

		SECTION("a name of one character in UTF-8, mode " + std::to_string(static_cast<int>(mode))) {
			OnTheFlyAlphabet alphabet{{"\xC5\xBF", 0}, {"b", 1}, {"\xE2\x82\xAC", 2}};
			Nfa code_points{3, {0}, {2}};
			code_points.delta.add(0, 0x1'7F, 1);
			code_points.delta.add(1, 'b', 1);
			code_points.delta.add(1, 0x20'AC, 2);
			CHECK(
				are_equivalent(parse(nfa.print_to_regex(mode, &alphabet), Encoding::Utf8).decode_utf8(), code_points)
			);
		}

		SECTION("a name that is not one character, mode " + std::to_string(static_cast<int>(mode))) {
			for (const std::string name :
				 {"ab", "", "10", "\xC5", "\xC5\xBF\xC5\xBF", "\xC0\xAF", "\xE0\x80\xAF", "\xED\xA0\x80"}) {
				OnTheFlyAlphabet alphabet{{"a", 0}, {name, 1}, {"c", 2}};
				CHECK_THROWS_AS(nfa.print_to_regex(mode, &alphabet), std::invalid_argument);
			}
		}

		SECTION("a symbol without a name, mode " + std::to_string(static_cast<int>(mode))) {
			OnTheFlyAlphabet alphabet{{"a", 0}, {"b", 1}};
			CHECK_THROWS(nfa.print_to_regex(mode, &alphabet));
		}
	}
}

TEST_CASE("mata::nfa::Nfa::print_to_regex() in the readable mode is never longer than in the fast mode") {
	std::mt19937 generator{7};
	for (unsigned i{0}; i < 300; ++i) {
		const Nfa nfa{random_nfa(generator, 8, 1 + i % 4)};
		INFO("automaton:\n" << nfa.print_to_mata());
		CHECK(nfa.print_to_regex(RegexPrintMode::Readable).size() <= nfa.print_to_regex(RegexPrintMode::Fast).size());
	}
}

TEST_CASE("mata::nfa::Nfa::print_to_regex() to a stream prints the whole regex") {
	// A fast-mode regex longer than the blocks it is passed on to the stream in.
	Nfa nfa{2, {0}, {1}};
	for (Symbol symbol{0x80}; symbol < 0xD8'00; symbol += 2) { nfa.delta.add(0, symbol, 1); }
	const std::string regex{nfa.print_to_regex(RegexPrintMode::Fast)};
	REQUIRE(regex.size() > 200'000);
	for (const RegexPrintMode mode : MODES) {
		std::ostringstream stream{};
		nfa.print_to_regex(stream, mode);
		CHECK(stream.str() == nfa.print_to_regex(mode));
	}
}
