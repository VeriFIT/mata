// TODO: Some header.

#include <sstream>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"

using namespace mata;
using namespace mata::nfa::algorithms;
using namespace mata::nfa;
using namespace mata::utils;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

template <class T> void unused(const T&) {}

TEST_CASE("mata::IntAlphabet") {
	auto alphabet1 = IntAlphabet();
	auto alphabet2 = IntAlphabet();
	CHECK(alphabet1.is_equal(alphabet2));

	auto& alphabet3 = alphabet2;
	CHECK(alphabet3.is_equal(&alphabet1));
	const auto& alphabet4 = alphabet2;
	CHECK(alphabet4.is_equal(alphabet1));

	OnTheFlyAlphabet different_alphabet{};
	OnTheFlyAlphabet different_alphabet2{};
	CHECK(!alphabet1.is_equal(different_alphabet));
	CHECK(!different_alphabet.is_equal(different_alphabet2));
	CHECK(different_alphabet.is_equal(&different_alphabet));
}

TEST_CASE("mata::OnTheFlyAlphabet::add_symbols_from()") {
	OnTheFlyAlphabet alphabet{{"a", 4}, {"b", 2}, {"c", 10}};
	auto symbols{alphabet.get_alphabet_symbols()};
	mata::utils::OrdVector<Symbol> expected{4, 2, 10};
	CHECK(symbols == expected);
	CHECK(alphabet.get_next_value() == 11);
	CHECK(alphabet.get_symbol_map() == OnTheFlyAlphabet::StringToSymbolMap{{"a", 4}, {"b", 2}, {"c", 10}});

	alphabet.add_new_symbol("e", 7);
	CHECK_THROWS(alphabet.add_new_symbol("a", 0));

	symbols = alphabet.get_alphabet_symbols();
	expected = mata::utils::OrdVector<Symbol>{7, 4, 2, 10};
	CHECK(symbols == expected);
	CHECK(alphabet.get_next_value() == 11);
	CHECK(alphabet.get_symbol_map() == OnTheFlyAlphabet::StringToSymbolMap{{"a", 4}, {"b", 2}, {"c", 10}, {"e", 7}});

	OnTheFlyAlphabet alphabet2{alphabet};
	alphabet2.add_new_symbol("f", 42);
	CHECK(alphabet.get_alphabet_symbols() != alphabet2.get_alphabet_symbols());
	CHECK(alphabet.translate_symb("f") != 42);
	CHECK(alphabet2.translate_symb("f") == 42);
	size_t num_of_symbols{alphabet.get_alphabet_symbols().size()};
	alphabet.erase("e");
	alphabet.erase("f");
	CHECK(alphabet.get_alphabet_symbols().size() + 2 == num_of_symbols);

	alphabet2 = alphabet;
	alphabet2.add_new_symbol("f", 42);
	CHECK(alphabet.get_alphabet_symbols() != alphabet2.get_alphabet_symbols());
	CHECK(alphabet.translate_symb("f") != 42);
	alphabet.erase("f");
	CHECK(alphabet2.translate_symb("f") == 42);

	OnTheFlyAlphabet alphabet_copy{alphabet};
	alphabet2 = OnTheFlyAlphabet{std::move(alphabet)};
	CHECK(alphabet2.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
	alphabet = alphabet2;
	alphabet2 = std::move(alphabet);
	CHECK(alphabet2.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
	alphabet = alphabet2;

	alphabet.clear();
	CHECK(alphabet.get_number_of_symbols() == 0);
}

TEST_CASE("mata::OnTheFlyAlphabet::reverse_translate_symbol()") {
	SECTION("follows insertions, erasures and clear") {
		OnTheFlyAlphabet alphabet{{"a", 4}, {"b", 2}};
		CHECK(alphabet.reverse_translate_symbol(4) == "a");
		CHECK(alphabet.reverse_translate_symbol(alphabet.translate_symb("c")) == "c");
		CHECK(alphabet.erase(Symbol{4}) == 1);
		CHECK_THROWS(alphabet.reverse_translate_symbol(4));
		CHECK(alphabet.get_symbol_map() == OnTheFlyAlphabet::StringToSymbolMap{{"b", 2}, {"c", 5}});
		alphabet.erase(alphabet.get_symbol_map().find("b"));
		CHECK_THROWS(alphabet.reverse_translate_symbol(2));
		alphabet.erase(alphabet.get_symbol_map().begin(), alphabet.get_symbol_map().end());
		CHECK(alphabet.get_alphabet_symbols().empty());
		alphabet.add_new_symbol("d", 2);
		CHECK(alphabet.reverse_translate_symbol(2) == "d");
		alphabet.clear();
		CHECK_THROWS(alphabet.reverse_translate_symbol(2));
	}

	SECTION("copies and moves own their index") {
		OnTheFlyAlphabet copy{};
		OnTheFlyAlphabet assigned{{"z", 0}};
		{
			OnTheFlyAlphabet original{{"a", 0}, {"b", 1}};
			copy = OnTheFlyAlphabet{original};
			assigned = original;
			original.erase("a");
			original.add_new_symbol("x", 0);
		}
		CHECK(copy.reverse_translate_symbol(0) == "a");
		CHECK(assigned.reverse_translate_symbol(0) == "a");
		CHECK(assigned.reverse_translate_symbol(1) == "b");
		const OnTheFlyAlphabet moved{std::move(copy)};
		CHECK(moved.reverse_translate_symbol(1) == "b");
	}

	SECTION("names sharing a symbol") {
		OnTheFlyAlphabet alphabet{{"a", 3}, {"b", 3}, {"c", 3}};
		CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{3});
		CHECK(alphabet.reverse_translate_symbol(3) == "a");
		const OnTheFlyAlphabet copy{alphabet};
		CHECK(copy.reverse_translate_symbol(3) == "a");
		alphabet.erase("a");
		const std::string promoted{alphabet.reverse_translate_symbol(3)};
		const bool promoted_another_name{promoted == "b" || promoted == "c"};
		CHECK(promoted_another_name);
		CHECK(alphabet.erase(Symbol{3}) == 1);
		CHECK(alphabet.get_symbol_map().size() == 1);
		CHECK(alphabet.reverse_translate_symbol(3) != promoted);
		alphabet.erase(Symbol{3});
		CHECK_THROWS(alphabet.reverse_translate_symbol(3));
		CHECK(alphabet.get_alphabet_symbols().empty());
	}

	SECTION("symbols far above the number of names") {
		OnTheFlyAlphabet alphabet{};
		alphabet.add_new_symbol("x", 1'000);
		alphabet.add_new_symbol("far", 2'000);
		OrdVector<Symbol> expected{};
		for (Symbol symbol{0}; symbol < 500; ++symbol) {
			alphabet.add_new_symbol("s" + std::to_string(symbol), symbol);
			expected.push_back(symbol);
		}
		// Enough names now for the index to cover 1000 densely: "x" keeps being reported.
		alphabet.add_new_symbol("y", 1'000);
		CHECK(alphabet.reverse_translate_symbol(1'000) == "x");
		CHECK(alphabet.reverse_translate_symbol(2'000) == "far");
		expected.push_back(1'000);
		expected.push_back(2'000);
		CHECK(alphabet.get_alphabet_symbols() == expected);
		alphabet.erase("x");
		CHECK(alphabet.reverse_translate_symbol(1'000) == "y");
		alphabet.erase(Symbol{1'000});
		CHECK_THROWS(alphabet.reverse_translate_symbol(1'000));
		expected.erase(1'000);
		CHECK(alphabet.get_alphabet_symbols() == expected);
	}
}

TEST_CASE("mata::OnTheFlyAlphabet reserved special-symbol names (#794)") {
	OnTheFlyAlphabet alphabet{};
	// The reserved names translate straight to the special symbols and are never registered, so a
	//  printed automaton parses back with the same transition symbols.
	CHECK(alphabet.translate_symb(EPSILON_NAME) == nfa::EPSILON);
	CHECK(alphabet.translate_symb(DONT_CARE_NAME) == nft::DONT_CARE);
	CHECK(alphabet.get_number_of_symbols() == 0);
	CHECK(
		alphabet.translate_word({std::string{EPSILON_NAME}, std::string{DONT_CARE_NAME}}) ==
		Word{nfa::EPSILON, nft::DONT_CARE}
	);

	// The reserved names cannot be registered as ordinary symbols.
	CHECK_THROWS(alphabet.add_new_symbol(std::string{EPSILON_NAME}));
	CHECK_THROWS(alphabet.add_new_symbol(std::string{DONT_CARE_NAME}));
	CHECK_THROWS(alphabet.add_new_symbol(std::string{EPSILON_NAME}, 13));
	CHECK_THROWS(alphabet.try_add_new_symbol(std::string{DONT_CARE_NAME}, 13));
	CHECK_THROWS(alphabet.add_symbols_from({std::string{EPSILON_NAME}}));
	CHECK_THROWS(OnTheFlyAlphabet{{std::string{EPSILON_NAME}, 13}});
	CHECK(alphabet.get_number_of_symbols() == 0);
}

TEST_CASE("mata::OnTheFlyAlphabet next symbol value (#759)") {
	SECTION("the symbol map constructor updates the next symbol value") {
		OnTheFlyAlphabet alphabet{OnTheFlyAlphabet::StringToSymbolMap{{"a", 0}, {"b", 1}}};
		CHECK(alphabet.get_next_value() == 2);
		CHECK(alphabet.translate_symb("a") == 0);
		CHECK(alphabet.translate_symb("b") == 1);
		CHECK(alphabet.translate_symb("c") == 2);
		CHECK(alphabet.get_number_of_symbols() == 3);
	}

	SECTION("create_alphabet() does not turn EPSILON into a letter") {
		Nfa aut{2};
		aut.initial.insert(0);
		aut.final.insert(1);
		aut.delta.add(0, 5, 1);
		aut.delta.add(0, nfa::EPSILON, 1);
		OnTheFlyAlphabet alphabet{create_alphabet(aut)};
		CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{5});
		CHECK(alphabet.get_number_of_symbols() == 1);
		CHECK(alphabet.translate_symb("new") == 6);
	}

	SECTION("Nft::fill_alphabet() does not turn DONT_CARE into a letter") {
		nft::Nft transducer{nft::Nft::with_levels(2, 3)};
		transducer.initial.insert(0);
		transducer.final.insert(2);
		transducer.delta.add(0, 5, 1);
		transducer.delta.add(1, nft::DONT_CARE, 2);
		OnTheFlyAlphabet alphabet{};
		transducer.fill_alphabet(alphabet);
		CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{5});
		CHECK(alphabet.translate_symb("new") == 6);
	}

	SECTION("get_number_of_symbols() counts the symbols, not the counter") {
		OnTheFlyAlphabet alphabet{100};
		alphabet.add_new_symbol("x");
		alphabet.add_new_symbol("y");
		CHECK(alphabet.get_number_of_symbols() == 2);
		CHECK(alphabet.get_next_value() == 102);
		CHECK(alphabet.erase("x") == 1);
		CHECK(alphabet.get_number_of_symbols() == 1);
	}

	SECTION("an exhausted symbol space is reported instead of handing out EPSILON") {
		OnTheFlyAlphabet alphabet{};
		alphabet.add_new_symbol("almost_epsilon", nfa::EPSILON - 1);
		CHECK(alphabet.get_next_value() == nfa::EPSILON);
		CHECK_THROWS_AS(alphabet.translate_symb("new"), std::runtime_error);
		CHECK_THROWS_AS(alphabet.add_new_symbol("new"), std::runtime_error);
		CHECK(alphabet.translate_symb("almost_epsilon") == nfa::EPSILON - 1);
		CHECK(alphabet.get_number_of_symbols() == 1);
	}

	SECTION("an exhausted symbol space is reported instead of handing out DONT_CARE or EPSILON") {
		// DONT_CARE (EPSILON - 1) and EPSILON are reserved, so an ordinary name must not get either value.
		for (Symbol top : {Symbol{nft::DONT_CARE}, Symbol{nfa::EPSILON}}) {
			OnTheFlyAlphabet alphabet{};
			alphabet.add_new_symbol("top", top);
			if (top == nft::DONT_CARE) {
				// The counter reaches DONT_CARE, so no ordinary name may be allocated any more.
				CHECK_THROWS_AS(alphabet.translate_symb("new"), std::runtime_error);
				CHECK_THROWS_AS(alphabet.add_new_symbol("new"), std::runtime_error);
				CHECK(alphabet.translate_symb("top") == top);
				CHECK(alphabet.get_number_of_symbols() == 1);
			} else {
				// An explicitly bound EPSILON never advances the counter; the low values stay available.
				CHECK(alphabet.translate_symb("new") == 0);
			}
		}
	}

	SECTION("erasing one of two names of a symbol does not hand its value out again") {
		for (bool by_symbol : {false, true}) {
			OnTheFlyAlphabet alphabet{};
			alphabet.add_new_symbol("a", 5);
			alphabet.add_new_symbol("b", 5);
			CHECK(alphabet.get_next_value() == 6);
			if (by_symbol) {
				CHECK(alphabet.erase(Symbol{5}) == 1);
			} else {
				CHECK(alphabet.erase("a") == 1);
			}
			// The value 5 still belongs to the other name, so it must not be reused for a new letter.
			CHECK(alphabet.get_next_value() == 6);
			if (by_symbol) {
				CHECK(alphabet.erase(Symbol{5}) == 1);
			} else {
				CHECK(alphabet.erase("b") == 1);
			}
			// The last name of the top value is gone, so its value is handed out again.
			CHECK(alphabet.translate_symb("new") == 5);
		}
	}

	SECTION("an explicitly added EPSILON does not wrap the counter") {
		OnTheFlyAlphabet alphabet{};
		alphabet.add_new_symbol("a", 3);
		alphabet.add_new_symbol("epsilon", nfa::EPSILON);
		CHECK(alphabet.get_next_value() == 4);
		CHECK(alphabet.translate_symb("new") == 4);
	}

	SECTION("erasing EPSILON from a fresh alphabet does not wrap the counter") {
		OnTheFlyAlphabet alphabet{};
		alphabet.add_new_symbol("epsilon", nfa::EPSILON);
		CHECK(alphabet.erase("epsilon") == 1);
		CHECK(alphabet.get_next_value() == 0);
		alphabet.add_new_symbol("epsilon", nfa::EPSILON);
		CHECK(alphabet.erase(nfa::EPSILON) == 1);
		CHECK(alphabet.get_next_value() == 0);
	}
}

TEST_CASE("mata::EnumAlphabet") {
	EnumAlphabet alphabet{};
	EnumAlphabet alphabet2{1, 2, 3, 4, 5};

	CHECK(alphabet.get_alphabet_symbols().empty());
	CHECK(alphabet.get_number_of_symbols() == 0);
	CHECK(alphabet.get_next_value() == 0);
	CHECK(alphabet.is_equal(alphabet));
	CHECK(!alphabet.is_equal(alphabet2));
	CHECK(alphabet.get_complement({}).empty());

	CHECK_NOTHROW(alphabet.add_new_symbol(1));
	CHECK_NOTHROW(alphabet.add_new_symbol(1));

	CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{1});
	CHECK(alphabet.get_number_of_symbols() == 1);
	CHECK(alphabet.get_next_value() == 2);
	CHECK(alphabet.get_complement({}) == OrdVector<Symbol>{1});

	CHECK_NOTHROW(alphabet.add_new_symbol(2));
	CHECK_NOTHROW(alphabet.add_new_symbol(3));

	CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{1, 2, 3});
	CHECK(alphabet.get_number_of_symbols() == 3);
	CHECK(alphabet.get_next_value() == 4);
	CHECK(alphabet.get_complement({2}) == OrdVector<Symbol>{1, 3});

	CHECK_NOTHROW(alphabet.add_symbols_from(alphabet2));
	CHECK(alphabet.get_alphabet_symbols() == alphabet2.get_alphabet_symbols());

	CHECK_THROWS(alphabet.translate_symb("3414"));
	CHECK(alphabet.translate_symb("1") == 1);
	CHECK_THROWS(alphabet.translate_symb("3414not a number"));
	CHECK_THROWS(alphabet.translate_symb("not a number"));

	EnumAlphabet alphabet3{alphabet};
	alphabet3.add_new_symbol(42);
	CHECK(alphabet.get_alphabet_symbols() != alphabet3.get_alphabet_symbols());
	CHECK(alphabet3.get_number_of_symbols() == alphabet.get_number_of_symbols() + 1);
	CHECK_THROWS(alphabet.translate_symb("42"));
	CHECK(alphabet3.translate_symb("42") == 42);

	alphabet3 = alphabet;
	alphabet3.add_new_symbol(42);
	CHECK(alphabet.get_alphabet_symbols() != alphabet3.get_alphabet_symbols());
	CHECK(alphabet3.get_number_of_symbols() == alphabet.get_number_of_symbols() + 1);
	CHECK_THROWS(alphabet.translate_symb("42"));
	CHECK(alphabet3.translate_symb("42") == 42);

	EnumAlphabet alphabet_copy{alphabet};
	alphabet3 = EnumAlphabet{std::move(alphabet)};
	CHECK(alphabet3.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
	alphabet = alphabet3;
	alphabet3 = std::move(alphabet);
	CHECK(alphabet3.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
	alphabet = std::move(alphabet3);

	alphabet.clear();
	CHECK(alphabet.get_number_of_symbols() == 0);
}

TEST_CASE("UTF-8 encoding") {
	SECTION("between 0x00 and 0x7F") {
		Word word{0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7F};
		CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
	}

	SECTION("between 0x80 and 0x7FF") {
		Word word{0x80, 0x1'00, 0x2'00, 0x3'00, 0x4'00, 0x5'00, 0x6'00, 0x7'00, 0x7'FF};
		CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
	}

	SECTION("between 0x800 and 0xFFFF") {
		Word word{0x8'00, 0x10'00, 0x20'00, 0x30'00, 0x40'00, 0x50'00, 0x60'00, 0x70'00, 0xFF'FF};
		CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
	}

	SECTION("between 0x10000 and 0x10FFFF") {
		Word word{0x1'00'00, 0x2'00'01, 0x3'00'02, 0x4'00'03, 0x5'00'04, 0x6'00'05, 0x7'00'06, 0x8'00'07, 0x10'FF'FF};
		CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
	}

	SECTION("big mix") {
		Word word{0x00,		 0x10,		0x20,	   0x30,	  0x40,		 0x50,		0x60,	   0x70,	  0x7F,
				  0x80,		 0x1'00,	0x2'00,	   0x3'00,	  0x4'00,	 0x5'00,	0x6'00,	   0x7'00,	  0x7'FF,
				  0x8'00,	 0x10'00,	0x20'00,   0x30'00,	  0x40'00,	 0x50'00,	0x60'00,   0x70'00,	  0xFF'FF,
				  0x1'00'00, 0x2'00'01, 0x3'00'02, 0x4'00'03, 0x5'00'04, 0x6'00'05, 0x7'00'06, 0x8'00'07, 0x10'FF'FF};
		CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
	}
}

TEST_CASE("mata::Alphabet streaming - #760") {
	SECTION("OnTheFlyAlphabet prints its name to symbol pairs") {
		OnTheFlyAlphabet alphabet{{"a", 0}, {"b", 1}};
		std::ostringstream out;
		out << alphabet;
		CHECK(out.str() == "{a->0, b->1}");
	}

	SECTION("OnTheFlyAlphabet prints names sharing a value independently of the insertion order") {
		const OnTheFlyAlphabet forward{{"a", 0}, {"b", 0}, {"c", 0}, {"d", 1}};
		const OnTheFlyAlphabet backward{{"d", 1}, {"c", 0}, {"b", 0}, {"a", 0}};
		std::ostringstream out_forward, out_backward;
		out_forward << forward;
		out_backward << backward;
		CHECK(out_forward.str() == "{a->0, b->0, c->0, d->1}");
		CHECK(out_backward.str() == "{a->0, b->0, c->0, d->1}");
	}

	SECTION("EnumAlphabet prints its symbols") {
		EnumAlphabet alphabet{0, 1, 5};
		std::ostringstream out;
		out << alphabet;
		CHECK(out.str() == "{0, 1, 5}");
	}

	SECTION("IntAlphabet prints a tag instead of recursing forever") {
		IntAlphabet alphabet{};
		std::ostringstream out;
		out << alphabet;
		CHECK(out.str() == "IntAlphabet");
	}
}
