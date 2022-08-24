#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/re2parser.hh>
using namespace Mata::Nfa;

TEST_CASE("Mata::RE2Parser basic_parsing")
{ // {{{
    SECTION("Empty expression")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "");
        REQUIRE(aut.finalstates.size() == aut.initialstates.size());
        REQUIRE(aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{}));
    }

    SECTION("Basic test")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "abcd");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }

    SECTION("Hex symbol encoding")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "\\x7f");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{127}));
    }

    SECTION("Wild card")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, ".*");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{'w','h','a','t','e','v','e','r'}));
        REQUIRE(is_in_lang(aut, Word{127}));
        REQUIRE(is_in_lang(aut, Word{0x7f}));
        REQUIRE(is_in_lang(aut, Word{}));
        EnumAlphabet alph = { };
        REQUIRE(is_universal(aut,alph));
    }

    SECTION("Iteration test")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "ab*cd*");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','b','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }
} // }}}

TEST_CASE("Mata::RE2Parser error")
{ // {{{
    SECTION("Complex regex that fails")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "((aa)*)*(b)*");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{'a','a','b'}));
        REQUIRE(!is_in_lang(aut, Word{'a','b'}));
    }

    SECTION("Regexes from issue #48")
    {
        Mata::Nfa::Nfa aut1;
        Mata::Nfa::Nfa aut2;
        Mata::RE2Parser::create_nfa(&aut1, "[qQrR]*");
        Mata::RE2Parser::create_nfa(&aut2, "[qr]*");
        REQUIRE(!aut1.trans_empty());
        REQUIRE(!is_lang_empty(aut1));
        REQUIRE(!aut2.trans_empty());
        REQUIRE(!is_lang_empty(aut2));
        REQUIRE(is_in_lang(aut1, Word{'Q','R','q','r'}));
        REQUIRE(is_in_lang(aut2, Word{'q','r','q','r'}));
        REQUIRE(!is_in_lang(aut2, Word{'q','R','q'}));
    }
} // }}}
