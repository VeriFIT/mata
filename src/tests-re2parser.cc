#include "../3rdparty/catch.hpp"

#include <vata2/nfa.hh>
#include <vata2/re2parser.hh>
using namespace Vata2::Nfa;

TEST_CASE("Vata2::RE2Parser basic_parsing")
{ // {{{
    auto parser = Vata2::RegexParser();

    SECTION("Empty expression")
    {
        Vata2::Nfa::Nfa aut = parser.createNFA("");
        REQUIRE(aut.finalstates.size() == aut.initialstates.size());
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{}));
    }

    SECTION("Iteration test")
    {
        Vata2::Nfa::Nfa aut = parser.createNFA("abcd");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }

    SECTION("Iteration test")
    {
        Vata2::Nfa::Nfa aut = parser.createNFA("ab*cd*");
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
