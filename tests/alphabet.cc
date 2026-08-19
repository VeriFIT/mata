// TODO: Some header.

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

using namespace mata;
using namespace mata::nfa::algorithms;
using namespace mata::nfa;
using namespace mata::utils;
using namespace mata::parser;
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
    OnTheFlyAlphabet alphabet{ { "a", 4 }, { "b", 2 }, { "c", 10 } };
    auto symbols{ alphabet.get_alphabet_symbols() };
    mata::utils::OrdVector<Symbol> expected{ 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == OnTheFlyAlphabet::StringToSymbolMap{ { "a", 4 }, { "b", 2 }, { "c", 10 } });

    alphabet.add_new_symbol("e", 7);
    CHECK_THROWS(alphabet.add_new_symbol("a", 0));

    symbols = alphabet.get_alphabet_symbols();
    expected = mata::utils::OrdVector<Symbol>{ 7, 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == OnTheFlyAlphabet::StringToSymbolMap {
        { "a", 4 }, { "b", 2 }, { "c", 10 }, { "e", 7 }
    });

    OnTheFlyAlphabet alphabet2{ alphabet };
    alphabet2.add_new_symbol("f", 42);
    CHECK(alphabet.get_alphabet_symbols() != alphabet2.get_alphabet_symbols());
    CHECK(alphabet.translate_symb("f") != 42);
    CHECK(alphabet2.translate_symb("f") == 42);
    size_t num_of_symbols{ alphabet.get_alphabet_symbols().size() };
    alphabet.erase("e");
    alphabet.erase("f");
    CHECK(alphabet.get_alphabet_symbols().size() + 2 == num_of_symbols);

    alphabet2 = alphabet;
    alphabet2.add_new_symbol("f", 42);
    CHECK(alphabet.get_alphabet_symbols() != alphabet2.get_alphabet_symbols());
    CHECK(alphabet.translate_symb("f") != 42);
    alphabet.erase("f");
    CHECK(alphabet2.translate_symb("f") == 42);

    OnTheFlyAlphabet alphabet_copy{ alphabet };
    alphabet2 = OnTheFlyAlphabet{ std::move(alphabet) };
    CHECK(alphabet2.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
    alphabet = alphabet2;
    alphabet2 = std::move(alphabet);
    CHECK(alphabet2.get_alphabet_symbols() == alphabet_copy.get_alphabet_symbols());
    alphabet = alphabet2;

    alphabet.clear();
    CHECK(alphabet.get_number_of_symbols() == 0);
}

TEST_CASE("mata::EnumAlphabet") {
    EnumAlphabet alphabet{};
    EnumAlphabet alphabet2{ 1, 2, 3, 4, 5 };

    CHECK(alphabet.get_alphabet_symbols().empty());
    CHECK(alphabet.get_number_of_symbols() == 0);
    CHECK(alphabet.get_next_value() == 0);
    CHECK(alphabet.is_equal(alphabet));
    CHECK(!alphabet.is_equal(alphabet2));
    CHECK(alphabet.get_complement({}).empty());

    CHECK_NOTHROW(alphabet.add_new_symbol(1));
    CHECK_NOTHROW(alphabet.add_new_symbol(1));

    CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{ 1 });
    CHECK(alphabet.get_number_of_symbols() == 1);
    CHECK(alphabet.get_next_value() == 2);
    CHECK(alphabet.get_complement({}) == OrdVector<Symbol>{ 1 });

    CHECK_NOTHROW(alphabet.add_new_symbol(2));
    CHECK_NOTHROW(alphabet.add_new_symbol(3));

    CHECK(alphabet.get_alphabet_symbols() == OrdVector<Symbol>{ 1, 2, 3 });
    CHECK(alphabet.get_number_of_symbols() == 3);
    CHECK(alphabet.get_next_value() == 4);
    CHECK(alphabet.get_complement({ 2 }) == OrdVector<Symbol>{ 1, 3 });

    CHECK_NOTHROW(alphabet.add_symbols_from(alphabet2));
    CHECK(alphabet.get_alphabet_symbols() == alphabet2.get_alphabet_symbols());

    CHECK_THROWS(alphabet.translate_symb("3414"));
    CHECK(alphabet.translate_symb("1") == 1);
    CHECK_THROWS(alphabet.translate_symb("3414not a number"));
    CHECK_THROWS(alphabet.translate_symb("not a number"));

    EnumAlphabet alphabet3{ alphabet };
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

    EnumAlphabet alphabet_copy{ alphabet };
    alphabet3 = EnumAlphabet{ std::move(alphabet) };
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
        Word word{ 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7F };
        CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
    }

    SECTION("between 0x80 and 0x7FF") {
        Word word{ 0x80, 0x1'00, 0x2'00, 0x3'00, 0x4'00, 0x5'00, 0x6'00, 0x7'00, 0x7'FF };
        CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
    }

    SECTION("between 0x800 and 0xFFFF") {
        Word word{ 0x8'00, 0x10'00, 0x20'00, 0x30'00, 0x40'00, 0x50'00, 0x60'00, 0x70'00, 0xFF'FF };
        CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
    }

    SECTION("between 0x10000 and 0x10FFFF") {
        Word word{ 0x1'00'00, 0x2'00'01, 0x3'00'02, 0x4'00'03, 0x5'00'04, 0x6'00'05, 0x7'00'06, 0x8'00'07, 0x10'FF'FF };
        CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
    }

    SECTION("big mix") {
        Word word{ 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7F,
                   0x80, 0x1'00, 0x2'00, 0x3'00, 0x4'00, 0x5'00, 0x6'00, 0x7'00, 0x7'FF,
                   0x8'00, 0x10'00, 0x20'00, 0x30'00, 0x40'00, 0x50'00, 0x60'00, 0x70'00, 0xFF'FF,
                   0x1'00'00, 0x2'00'01, 0x3'00'02, 0x4'00'03, 0x5'00'04, 0x6'00'05, 0x7'00'06, 0x8'00'07, 0x10'FF'FF };
        CHECK(decode_word_utf8(encode_word_utf8(word)) == word);
    }
}
