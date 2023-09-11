#include <catch2/catch.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/parser/re2parser.hh"
using namespace mata::nfa;

using Symbol = mata::Symbol;
using Word = std::vector<Symbol>;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

// Some example regexes were taken from RegExr under GPL v3: https://github.com/gskinner/regexr.

TEST_CASE("mata::Parser basic_parsing") {
    Nfa aut;

    SECTION("Empty expression") {
        mata::parser::create_nfa(&aut, "");
        REQUIRE(aut.final.size() == aut.initial.size());
        REQUIRE(aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(aut.is_in_lang(Word{}));
    }

    SECTION("Basic test")
    {
        mata::parser::create_nfa(&aut, "abcd");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.empty_language());
        REQUIRE(!aut.is_in_lang(Word{'a','b','c'}));
        REQUIRE(aut.is_in_lang(Word{'a','b','c','d'}));
        REQUIRE(!aut.is_in_lang(Word{'a','b','c','d','d'}));
        REQUIRE(!aut.is_in_lang(Word{'a','d','c'}));
    }

    SECTION("Hex symbol encoding")
    {
        mata::parser::create_nfa(&aut, "\\x7f");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.empty_language());
        REQUIRE(aut.is_in_lang(Word{127}));
    }

    SECTION("Wild cardinality")
    {
        mata::parser::create_nfa(&aut, ".*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.empty_language());
        REQUIRE(aut.is_in_lang(Word{'w','h','a','t','e','v','e','r'}));
        REQUIRE(aut.is_in_lang(Word{127}));
        REQUIRE(aut.is_in_lang(Word{0x7f}));
        REQUIRE(aut.is_in_lang(Word{}));
        OnTheFlyAlphabet alph{};
        REQUIRE(aut.is_universal(alph));
    }

    SECTION("Special character") {
        mata::parser::create_nfa(&aut, "\\t");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{'\t'}));
        CHECK(!aut.is_in_lang(Word{'t'}));
        CHECK(!aut.is_in_lang(Word{}));
    }

    SECTION("Whitespace") {
        mata::parser::create_nfa(&aut, "a\\sb");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{'a', '\t', 'b'}));
        CHECK(!aut.is_in_lang(Word{}));
    }

    SECTION("Iteration test")
    {
        mata::parser::create_nfa(&aut, "ab*cd*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.empty_language());
        REQUIRE(aut.is_in_lang(Word{'a','b','c'}));
        REQUIRE(aut.is_in_lang(Word{'a','b','c','d'}));
        REQUIRE(aut.is_in_lang(Word{'a','c','d'}));
        REQUIRE(aut.is_in_lang(Word{'a','b','b','c','d'}));
        REQUIRE(aut.is_in_lang(Word{'a','b','c','d','d'}));
        REQUIRE(!aut.is_in_lang(Word{'a','d','c'}));
    }

    SECTION("Additional parenthesis") {
        Nfa expected{2};
        expected.initial.insert(0);
        expected.final.insert(1);
        expected.delta.add(0, 'a', 0);
        expected.delta.add(0, 'b', 1);

        SECTION("No parenthesis") {
            mata::parser::create_nfa(&aut, "a*b");
        }

        SECTION("Around example parenthesis") {
            mata::parser::create_nfa(&aut, "(a*b)");
        }

        SECTION("Around variable 'a' parenthesis") {
            mata::parser::create_nfa(&aut, "(a)*b");
        }

        SECTION("Around variable 'b' parenthesis") {
            mata::parser::create_nfa(&aut, "a*(b)");
        }

        SECTION("Parenthesis after iteration") {
            mata::parser::create_nfa(&aut, "((a)*)b");
        }

        SECTION("Double parenthesis around 'b'") {
            mata::parser::create_nfa(&aut, "(a*(b))");
        }

        SECTION("Double parenthesis around 'a'") {
            mata::parser::create_nfa(&aut, "((a)*b)");
        }

        SECTION("Many parenthesis") {
            mata::parser::create_nfa(&aut, "(((a)*)b)");
        }

        SECTION("Double parenthesis") {
            mata::parser::create_nfa(&aut, "((a))*((b))");
        }

        SECTION("Double parenthesis after iteration") {
            mata::parser::create_nfa(&aut, "((((a))*))((b))");
        }

        SECTION("Many parenthesis with double parenthesis") {
            mata::parser::create_nfa(&aut, "(((((a))*))((b)))");
        }

        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'a','b'}));
        CHECK(aut.is_in_lang(Word{'a','a','b'}));
        CHECK(!aut.is_in_lang(Word{'b','a'}));
        CHECK(are_equivalent(aut, expected));
    }

    SECTION("Complex regex") {
        mata::parser::create_nfa(&aut, "(a+)|(e)(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'e', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e', 'b'}));
    }

    SECTION("Complex regex with additional plus") {
        mata::parser::create_nfa(&aut, "(a+)|(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'e', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus") {
        mata::parser::create_nfa(&aut, "(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'e', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2") {
        mata::parser::create_nfa(&aut, "(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.5") {
        mata::parser::create_nfa(&aut, "(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.63") {
        mata::parser::create_nfa(&aut, "w*b+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.75") {
        mata::parser::create_nfa(&aut, "w(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w', 'w', 'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.85") {
        mata::parser::create_nfa(&aut, "w*(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'w'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 3") {
        mata::parser::create_nfa(&aut, "(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Complex regex 2") {
        mata::parser::create_nfa(&aut, "(a+)|(e)(w*)(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'e', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w'}));
        CHECK(aut.is_in_lang(Word{'e', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Complex regex 2 with additional plus") {
        mata::parser::create_nfa(&aut, "(a+)|(e)(w*)+(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'e', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w'}));
        CHECK(aut.is_in_lang(Word{'e', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("a+b+") {
        mata::parser::create_nfa(&aut, "a+b+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a', 'b', 'a'}));
    }

    SECTION("a+b+a*") {
        mata::parser::create_nfa(&aut, "a+b+a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("a+(b+)a*") {
        mata::parser::create_nfa(&aut, "a+(b+)a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("(a+(b+)a*)") {
        mata::parser::create_nfa(&aut, "(a+(b+)a*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("(a+b*a*)") {
        mata::parser::create_nfa(&aut, "(a+b*a*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("a+a+") {
        mata::parser::create_nfa(&aut, "a+a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)a+") {
        mata::parser::create_nfa(&aut, "(a+)a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("a(a+)") {
        mata::parser::create_nfa(&aut, "a(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)b") {
        mata::parser::create_nfa(&aut, "(a+)b");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b'}));
    }

    SECTION("b(a+)") {
        mata::parser::create_nfa(&aut, "b(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'a', 'a'}));
    }

    SECTION("b|(a+)") {
        mata::parser::create_nfa(&aut, "b|(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a+") {
        mata::parser::create_nfa(&aut, "b|a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a") {
        mata::parser::create_nfa(&aut, "b|a");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a*") {
        mata::parser::create_nfa(&aut, "b|a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("bba+") {
        mata::parser::create_nfa(&aut, "bba+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a', 'a', 'a'}));
    }

    SECTION("b*ba+") {
        mata::parser::create_nfa(&aut, "b*ba+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'a', 'a', 'a'}));
    }

    SECTION("b*ca+") {
        mata::parser::create_nfa(&aut, "b*ca+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'c', 'a'}));
        CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'c', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b', 'c', 'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'c', 'a', 'a', 'a'}));
    }

    SECTION("[abcd]") {
        mata::parser::create_nfa(&aut, "[abcd]");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
    }

    SECTION("[abcd]*") {
        mata::parser::create_nfa(&aut, "[abcd]*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'c'}));
        CHECK(aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c'}));
    }

    SECTION("[abcd]*e*") {
        mata::parser::create_nfa(&aut, "[abcd]*e*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'c'}));
        CHECK(aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c'}));

        CHECK(aut.is_in_lang(Word{'a', 'e'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e', 'e'}));
    }

    SECTION("[abcd]*e+") {
        mata::parser::create_nfa(&aut, "[abcd]*e+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'c'}));
        CHECK(!aut.is_in_lang(Word{'d'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'c', 'c'}));
        CHECK(!aut.is_in_lang(Word{'d', 'd'}));
        CHECK(!aut.is_in_lang(Word{'a', 'd'}));
        CHECK(!aut.is_in_lang(Word{'a', 'd', 'c'}));

        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'e'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e', 'e'}));
    }

    SECTION("[abcd]*.*") {
        mata::parser::create_nfa(&aut, "[abcd]*.*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'c'}));
        CHECK(aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c'}));

        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'e'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e', 'e'}));

        CHECK(aut.is_in_lang(Word{'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'g'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'g', 'g'}));
    }

    SECTION("[abcd]*.+") {
        mata::parser::create_nfa(&aut, "[abcd]*.+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'c'}));
        CHECK(aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c'}));

        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'e'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'e', 'e'}));

        CHECK(aut.is_in_lang(Word{'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'g'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'd', 'c', 'g', 'g'}));
    }

    SECTION("[a-c]+") {
        mata::parser::create_nfa(&aut, "[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(!aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'c', 'c'}));
        CHECK(!aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
    }

    SECTION("d[a-c]+") {
        mata::parser::create_nfa(&aut, "d[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'c'}));
        CHECK(!aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'d', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'d', 'c', 'c'}));
        CHECK(!aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'b', 'c'}));
    }

    SECTION("d*[a-c]+") {
        mata::parser::create_nfa(&aut, "d*[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(!aut.is_in_lang(Word{'d'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'d', 'b', 'b'}));
        CHECK(aut.is_in_lang(Word{'d', 'c', 'c'}));
        CHECK(!aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'d', 'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'d', 'd', 'a', 'b', 'c'}));
    }

    SECTION("[^a-c]") {
        mata::parser::create_nfa(&aut, "[^a-c]");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(!aut.is_in_lang(Word{'d', 'd'}));
        CHECK(aut.is_in_lang(Word{'e'}));
        CHECK(!aut.is_in_lang(Word{'e', 'e'}));
    }

    SECTION("(ha)+") {
        mata::parser::create_nfa(&aut, "(ha)+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'h'}));
        CHECK(aut.is_in_lang(Word{'h', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'h'}));
        CHECK(aut.is_in_lang(Word{'h', 'a', 'h', 'a'}));
        CHECK(!aut.is_in_lang(Word{'h', 'a', 'h'}));
        CHECK(!aut.is_in_lang(Word{'h', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'h', 'a', 'h', 'a', 'h', 'a'}));
    }

    SECTION("(ha)*") {
        mata::parser::create_nfa(&aut, "(ha)*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'h'}));
        CHECK(aut.is_in_lang(Word{'h', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'h'}));
        CHECK(aut.is_in_lang(Word{'h', 'a', 'h', 'a'}));
        CHECK(!aut.is_in_lang(Word{'h', 'a', 'h'}));
        CHECK(!aut.is_in_lang(Word{'h', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'h', 'a', 'h', 'a', 'h', 'a'}));
    }

    SECTION("b\\w{2,3}") {
        mata::parser::create_nfa(&aut, "b\\w{2,3}");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'e'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'e', 'r'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b\\w+?") {
        mata::parser::create_nfa(&aut, "b\\w+?");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'e'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b(a|e|i)d") {
        mata::parser::create_nfa(&aut, "b(a|e|i)d");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'd'}));
        CHECK(!aut.is_in_lang(Word{'b', 'u', 'd'}));
        CHECK(!aut.is_in_lang(Word{'b', 'o', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'i', 'd'}));
    }

    SECTION("[ab](c|d)") {
        mata::parser::create_nfa(&aut, "[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("[ab](c|d)") {
        mata::parser::create_nfa(&aut, "[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("[ab]+(c|d)") {
        mata::parser::create_nfa(&aut, "[ab]+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'a', 'c'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("([ab])+(c|d)") {
        mata::parser::create_nfa(&aut, "([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'a', 'c'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("(([ab])+)(c|d)") {
        mata::parser::create_nfa(&aut, "(([ab])+)(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'a', 'c'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("g|((([ab])+)(c|d))") {
        mata::parser::create_nfa(&aut, "(g|(([ab])+))(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'a', 'c'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
        CHECK(aut.is_in_lang(Word{'g', 'c'}));
        CHECK(aut.is_in_lang(Word{'g', 'd'}));
    }

    SECTION("g|([ab])+(c|d)") {
        mata::parser::create_nfa(&aut, "g|([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.empty_language());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'g'}));
        CHECK(aut.is_in_lang(Word{'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'd'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'c'}));
        CHECK(aut.is_in_lang(Word{'a', 'b', 'a', 'c'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'c', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'e'}));
        CHECK(!aut.is_in_lang(Word{'a', 'c', 'd'}));
    }

    SECTION("Star iteration") {
        Nfa expected{2};
        expected.initial.insert(0);
        expected.final.insert({0, 1});
        expected.delta.add(0, 'c', 0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'a', 1);

        SECTION("(((c)*)((a)*))") {
            mata::parser::create_nfa(&aut, "(((c)*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(aut.is_in_lang(Word{}));
            CHECK(aut.is_in_lang(Word{'c'}));
            CHECK(aut.is_in_lang(Word{'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c'}));
            CHECK(aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("((c*)((a)*))") {
            mata::parser::create_nfa(&aut, "((c*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(aut.is_in_lang(Word{}));
            CHECK(aut.is_in_lang(Word{'c'}));
            CHECK(aut.is_in_lang(Word{'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c'}));
            CHECK(aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(c*(a*))") {
            mata::parser::create_nfa(&aut, "(c*(a*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(aut.is_in_lang(Word{}));
            CHECK(aut.is_in_lang(Word{'c'}));
            CHECK(aut.is_in_lang(Word{'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c'}));
            CHECK(aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(c*a*)") {
            mata::parser::create_nfa(&aut, "(c*a*)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(aut.is_in_lang(Word{}));
            CHECK(aut.is_in_lang(Word{'c'}));
            CHECK(aut.is_in_lang(Word{'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c'}));
            CHECK(aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("c*a*") {
            mata::parser::create_nfa(&aut, "c*a*");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(aut.is_in_lang(Word{}));
            CHECK(aut.is_in_lang(Word{'c'}));
            CHECK(aut.is_in_lang(Word{'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c'}));
            CHECK(aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(((c)+)((a)+))") {
            mata::parser::create_nfa(&aut, "(((c)+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("((c+)((a)+))") {
            mata::parser::create_nfa(&aut, "((c+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("((c+)(a+))") {
            mata::parser::create_nfa(&aut, "((c+)(a+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("(c+)(a+)") {
            mata::parser::create_nfa(&aut, "(c+)(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("c+(a+)") {
            mata::parser::create_nfa(&aut, "c+(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("(c+)a+") {
            mata::parser::create_nfa(&aut, "(c+)a+");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("c+a+") {
            mata::parser::create_nfa(&aut, "c+a+");
            CHECK(!aut.delta.empty());
            CHECK(!aut.empty_language());
            CHECK(!aut.is_in_lang(Word{}));
            CHECK(!aut.is_in_lang(Word{'c'}));
            CHECK(!aut.is_in_lang(Word{'a'}));
            CHECK(!aut.is_in_lang(Word{'c', 'c'}));
            CHECK(!aut.is_in_lang(Word{'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a'}));
            CHECK(!aut.is_in_lang(Word{'a', 'c'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'a', 'a'}));
            CHECK(aut.is_in_lang(Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.insert(0);
            expected_plus_iteration.final.insert(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }
    }
} // }}}

TEST_CASE("mata::Parser error")
{ // {{{
    SECTION("Complex regex that fails")
    {
        mata::nfa::Nfa aut;
        mata::parser::create_nfa(&aut, "((aa)*)*(b)*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.empty_language());
        REQUIRE(aut.is_in_lang(Word{'a','a','b'}));
        REQUIRE(!aut.is_in_lang(Word{'a','b'}));
    }

    SECTION("Regexes from issue #48")
    {
        mata::nfa::Nfa aut1;
        mata::nfa::Nfa aut2;
        mata::parser::create_nfa(&aut1, "[qQrR]*");
        mata::parser::create_nfa(&aut2, "[qr]*");
        REQUIRE(!aut1.delta.empty());
        REQUIRE(!aut1.empty_language());
        REQUIRE(!aut2.delta.empty());
        REQUIRE(!aut2.empty_language());
        REQUIRE(aut1.is_in_lang(Word{'Q','R','q','r'}));
        REQUIRE(aut2.is_in_lang(Word{'q','r','q','r'}));
        REQUIRE(!aut2.is_in_lang(Word{'q','R','q'}));
    }

    SECTION("Regex from issue #139") {
        Nfa x;
        mata::parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)");
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));

        x.clear();
        mata::parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)", false, 306, false);
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
    }

    SECTION("Another failing regex") {
        Nfa x;
        mata::parser::create_nfa(&x, "(cd(abcde)+)|(a(aaa)+|ccc+)");
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'c', 'c' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'c' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'c', 'c', 'c', 'c', 'c' }, {} }));

        x.clear();
        mata::parser::create_nfa(&x, "(cd(abcde)+)|(a(aaa)+|ccc+)", false, 306, false);
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'c', 'c' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'c', 'c' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'c', 'c', 'c', 'c', 'c' }, {} }));
    }
} // }}}

TEST_CASE("mata::Parser bug epsilon")
{ // {{{
    SECTION("failing regex")
    {
        Nfa x;
        mata::parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)");
        CHECK(x.is_in_lang(Run{Word{'a', 'a', 'a', 'a'}, {}}));
    }
} // }}}
