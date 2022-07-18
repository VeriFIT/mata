#include "../3rdparty/catch.hpp"

#include <vata2/nfa.hh>
#include <vata2/re2parser.hh>
using namespace Vata2::Nfa;

TEST_CASE("Vata2::RE2Parser basic_parsing")
{ // {{{
    SECTION("Empty expression")
    {
        Vata2::Nfa::Nfa aut;
        Vata2::RE2Parser::create_nfa(&aut, "");
        REQUIRE(aut.finalstates.size() == aut.initialstates.size());
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{}));
    }

    SECTION("Basic test")
    {
        Vata2::Nfa::Nfa aut;
        Vata2::RE2Parser::create_nfa(&aut, "abcd");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }

    SECTION("Hex symbol encoding")
    {
        Vata2::Nfa::Nfa aut;
        Vata2::RE2Parser::create_nfa(&aut, "\\x7f");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{127}));
    }

    SECTION("Wild card")
    {
        Vata2::Nfa::Nfa aut;
        Vata2::RE2Parser::create_nfa(&aut, ".*");
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
        Vata2::Nfa::Nfa aut;
        Vata2::RE2Parser::create_nfa(&aut, "ab*cd*");
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
