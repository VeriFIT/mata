// TODO: Some header.

#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace Mata;
using namespace Mata::Nfa::Algorithms;
using namespace Mata::Nfa;
using namespace Mata::Util;
using namespace Mata::Parser;
using IntAlphabet = Mata::IntAlphabet;
using OnTheFlyAlphabet = Mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

template<class T> void unused(const T &) {}

TEST_CASE("Mata::IntAlphabet") {
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

TEST_CASE("Mata::OnTheFlyAlphabet::add_symbols_from()") {
    OnTheFlyAlphabet alphabet{};
    StringToSymbolMap symbol_map{ { "a", 4 }, { "b", 2 }, { "c", 10 } };
    alphabet.add_symbols_from(symbol_map);

    auto symbols{alphabet.get_alphabet_symbols() };
    Mata::Util::OrdVector<Symbol> expected{ 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == symbol_map);

    symbol_map["a"] = 6;
    symbol_map["e"] = 7;
    alphabet.add_symbols_from(symbol_map);

    symbols = alphabet.get_alphabet_symbols();
    expected = Mata::Util::OrdVector<Symbol>{ 7, 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == StringToSymbolMap{
        { "a", 4 }, { "b", 2 }, { "c", 10 }, { "e", 7 }
    });
}

TEST_CASE("Mata::EnumAlphabet") {
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
}
