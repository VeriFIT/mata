#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/parser/re2parser.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;

using Symbol = mata::Symbol;
using Word = std::vector<Symbol>;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

// Some example regexes were taken from RegExr under GPL v3: https://github.com/gskinner/regexr.

TEST_CASE("mata::Parser basic_parsing") {
    Nfa aut;

    SECTION("Empty expression") {
        aut = mata::parser::create_nfa("");
        REQUIRE(aut.final.size() == aut.initial.size());
        REQUIRE(aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(aut.is_in_lang(Word{}));
    }

    SECTION("Basic test")
    {
        aut = mata::parser::create_nfa("abcd");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(!aut.is_in_lang(Word{'a','b','c'}));
        REQUIRE(aut.is_in_lang(Word{'a','b','c','d'}));
        REQUIRE(!aut.is_in_lang(Word{'a','b','c','d','d'}));
        REQUIRE(!aut.is_in_lang(Word{'a','d','c'}));
    }

    SECTION("Hex symbol encoding")
    {
        aut = mata::parser::create_nfa("\\x7f");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(aut.is_in_lang(Word{127}));
    }

    SECTION("Wild cardinality")
    {
        aut = mata::parser::create_nfa(".*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(aut.is_in_lang(Word{'w','h','a','t','e','v','e','r'}));
        REQUIRE(aut.is_in_lang(Word{127}));
        REQUIRE(aut.is_in_lang(Word{0x7f}));
        REQUIRE(aut.is_in_lang(Word{}));
        OnTheFlyAlphabet alph{};
        REQUIRE(aut.is_universal(alph));
    }

    SECTION("Special character") {
        aut = mata::parser::create_nfa("\\t");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(aut.is_in_lang(Word{'\t'}));
        CHECK(!aut.is_in_lang(Word{'t'}));
        CHECK(!aut.is_in_lang(Word{}));
    }

    SECTION("Whitespace") {
        aut = mata::parser::create_nfa("a\\sb");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(aut.is_in_lang(Word{'a', '\t', 'b'}));
        CHECK(!aut.is_in_lang(Word{}));
    }

    SECTION("Iteration test")
    {
        aut = mata::parser::create_nfa("ab*cd*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("a*b");
        }

        SECTION("Around example parenthesis") {
            aut = mata::parser::create_nfa("(a*b)");
        }

        SECTION("Around variable 'a' parenthesis") {
            aut = mata::parser::create_nfa("(a)*b");
        }

        SECTION("Around variable 'b' parenthesis") {
            aut = mata::parser::create_nfa("a*(b)");
        }

        SECTION("Parenthesis after iteration") {
            aut = mata::parser::create_nfa("((a)*)b");
        }

        SECTION("Double parenthesis around 'b'") {
            aut = mata::parser::create_nfa("(a*(b))");
        }

        SECTION("Double parenthesis around 'a'") {
            aut = mata::parser::create_nfa("((a)*b)");
        }

        SECTION("Many parenthesis") {
            aut = mata::parser::create_nfa("(((a)*)b)");
        }

        SECTION("Double parenthesis") {
            aut = mata::parser::create_nfa("((a))*((b))");
        }

        SECTION("Double parenthesis after iteration") {
            aut = mata::parser::create_nfa("((((a))*))((b))");
        }

        SECTION("Many parenthesis with double parenthesis") {
            aut = mata::parser::create_nfa("(((((a))*))((b)))");
        }

        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'a','b'}));
        CHECK(aut.is_in_lang(Word{'a','a','b'}));
        CHECK(!aut.is_in_lang(Word{'b','a'}));
        CHECK(are_equivalent(aut, expected));
    }

    SECTION("Complex regex") {
        aut = mata::parser::create_nfa("(a+)|(e)(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(a+)|(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("w*b+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("w(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("w*(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'e'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'b'}));
        CHECK(!aut.is_in_lang(Word{'a', 'w', 'b'}));
    }

    SECTION("Complex regex 2") {
        aut = mata::parser::create_nfa("(a+)|(e)(w*)(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(a+)|(e)(w*)+(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("a+b+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("a+b+a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("a+(b+)a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(a+(b+)a*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(a+b*a*)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("a+a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)a+") {
        aut = mata::parser::create_nfa("(a+)a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("a(a+)") {
        aut = mata::parser::create_nfa("a(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)b") {
        aut = mata::parser::create_nfa("(a+)b");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'b'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a', 'b'}));
    }

    SECTION("b(a+)") {
        aut = mata::parser::create_nfa("b(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'a', 'a'}));
    }

    SECTION("b|(a+)") {
        aut = mata::parser::create_nfa("b|(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a+") {
        aut = mata::parser::create_nfa("b|a+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a") {
        aut = mata::parser::create_nfa("b|a");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a'}));
        CHECK(!aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("b|a*") {
        aut = mata::parser::create_nfa("b|a*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a'}));
        CHECK(!aut.is_in_lang(Word{'b', 'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a'}));
        CHECK(aut.is_in_lang(Word{'a', 'a', 'a'}));
    }

    SECTION("bba+") {
        aut = mata::parser::create_nfa("bba+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("b*ba+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("b*ca+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[abcd]");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'a'}));
        CHECK(aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'c'}));
        CHECK(aut.is_in_lang(Word{'d'}));
        CHECK(!aut.is_in_lang(Word{'b', 'b'}));
    }

    SECTION("[abcd]*") {
        aut = mata::parser::create_nfa("[abcd]*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[abcd]*e*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[abcd]*e+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[abcd]*.*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[abcd]*.+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("d[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("d*[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[^a-c]");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(ha)+");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(ha)*");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("b\\w{2,3}");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'e'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'e', 'r'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b\\w+?") {
        aut = mata::parser::create_nfa("b\\w+?");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(!aut.is_in_lang(Word{'b'}));
        CHECK(aut.is_in_lang(Word{'b', 'e'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r'}));
        CHECK(!aut.is_in_lang(Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b(a|e|i)d") {
        aut = mata::parser::create_nfa("b(a|e|i)d");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
        CHECK(!aut.is_in_lang(Word{}));
        CHECK(aut.is_in_lang(Word{'b', 'a', 'd'}));
        CHECK(!aut.is_in_lang(Word{'b', 'u', 'd'}));
        CHECK(!aut.is_in_lang(Word{'b', 'o', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'e', 'd'}));
        CHECK(aut.is_in_lang(Word{'b', 'i', 'd'}));
    }

    SECTION("[ab](c|d)") {
        aut = mata::parser::create_nfa("[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("[ab]+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(([ab])+)(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("(g|(([ab])+))(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
        aut = mata::parser::create_nfa("g|([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(((c)*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("((c*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(c*(a*))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(c*a*)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("c*a*");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(((c)+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("((c+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("((c+)(a+))");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(c+)(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("c+(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("(c+)a+");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
            aut = mata::parser::create_nfa("c+a+");
            CHECK(!aut.delta.empty());
            CHECK(!aut.is_lang_empty());
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
        mata::nfa::Nfa aut = mata::parser::create_nfa("((aa)*)*(b)*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!aut.is_lang_empty());
        REQUIRE(aut.is_in_lang(Word{'a','a','b'}));
        REQUIRE(!aut.is_in_lang(Word{'a','b'}));
    }

    SECTION("Regexes from issue #48")
    {
        mata::nfa::Nfa aut1 = mata::parser::create_nfa("[qQrR]*");
        mata::nfa::Nfa aut2 = mata::parser::create_nfa("[qr]*");
        REQUIRE(!aut1.delta.empty());
        REQUIRE(!aut1.is_lang_empty());
        REQUIRE(!aut2.delta.empty());
        REQUIRE(!aut2.is_lang_empty());
        REQUIRE(aut1.is_in_lang(Word{'Q','R','q','r'}));
        REQUIRE(aut2.is_in_lang(Word{'q','r','q','r'}));
        REQUIRE(!aut2.is_in_lang(Word{'q','R','q'}));
    }

    SECTION("Regex from issue #139") {
        Nfa x = mata::parser::create_nfa("(cd(abcde)*)|(a(aaa)*)");
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));

        x = mata::parser::create_nfa("(cd(abcde)*)|(a(aaa)*)", false, 306, false);
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'c', 'd' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!x.is_in_lang(Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
    }

    SECTION("Regex from issue #456") {
        Nfa x = mata::parser::create_nfa("[\\x00-\\x5a\\x5c-\\x7F]");

        Nfa y;
        State initial_s = 0;
        State final_s = 1;
        y.initial.insert(initial_s);
        y.final.insert(final_s);
        for (Symbol c = 0; c <= 0x7F; c++) {
            if (c == 0x5B) {
                continue;
            }
            y.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(x, y));
    }

    // TODO uncomment these two after the issues are fixed

    // SECTION("Regex from issue #457") {
    //     Nfa x{2, {0}, {1}};
    //     x.delta.add(0,'a',1);
    //     x.delta.add(0,'b',1);
    //     Nfa y = mata::parser::create_nfa("(a|b)");
    //     CHECK(are_equivalent(x, y));
    //     y = mata::parser::create_nfa("(a$|b)");
    //     CHECK(are_equivalent(x, y));
    //     y = mata::parser::create_nfa("a|b");
    //     CHECK(are_equivalent(x, y));
    //     y = mata::parser::create_nfa("a$|b");
    //     CHECK(are_equivalent(x, y));
    // }

    // SECTION("Regexes from issue #437") {
    //     std::vector<std::string> regexes = {
    //         "^.*[sS][yY][sS][tT][eE][mM][pP][aA][tT][hH]\\=([hH][tT]{2}[pP][sS]?)|([fF][tT][pP])",
    //         R"REGEX(^[^\f\n\r\t\v]{65}|[^\f\n\r\t\v]+[\f\n\r\t\v]+[^\f\n\r\t\v]{65}|[^\f\n\r\t\v]+[\f\n\r\t\v]+[^\f\n\r\t\v]+[\f\n\r\t\v]+[^\f\n\r\t\v]{65})REGEX",
    //         R"REGEX(^get (X.downloadX[ -~]*|X.supernode[ -~]|X.status[ -~]|X.network[ -~]*|X.files|X.hash\=[0-9a-f]*X[ -~]*) httpX1.1|user-agent: kazaa|x-kazaa(-username|-network|-ip|-supernodeip|-xferid|-xferuid|tag)|^give [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]?[0-9]?[0-9]?)REGEX",
    //         R"REGEX(^(\*[\x01\x02].*\x03\x0b|\*\x01.?.?.?.?\x01)|flapon|toc_signon.*0x)REGEX",
    //         R"REGEX(^(\x11\x20\x01...?\x11|\xfe\xfd.?.?.?.?.?.?(\x14\x01\x06|\xff\xff\xff))|[\]\x01].?battlefield2)REGEX",
    //         R"REGEX(^(\x13bittorrent protocol|azver\x01$|get Xscrape\?info_hash\=get Xannounce\?info_hash\=|get XclientXbitcometX|GET Xdata\?fid\=)|d1:ad2:id20:|\x08'7P\)[RP])REGEX",
    //         R"REGEX(^[a-z][a-z0-9\-_]+|login: [\x09-\x0d -~]* name: [\x09-\x0d -~]* Directory:)REGEX",
    //         R"REGEX(^get X.*icy-metadata:1|icy [1-5][0-9][0-9] [\x09-\x0d -~]*(content-type:audio|icy-))REGEX",
    //         R"REGEX(^([()]|get)(...?.?.?(reg|get|query)|.+User-Agent: (MozillaX4\.0 \(compatible; (MSIE 6\.0; Windows NT 5\.1;? ?\)|MSIE 5\.00; Windows 98\))))|Keep-Alive\x0d\x0a\x0d\x0a[26])REGEX",
    //         R"REGEX(^[\f\n\r\t\v]*Accept-Language[\f\n\r\t\v]*|3a|[\f\n\r\t\v]*([^\r\n]*?\x2c){20})REGEX"
    //     };

    //     for (std::string regex : regexes) {
    //         Nfa x = mata::parser::create_nfa(regex);
    //         Nfa y = mata::parser::create_nfa(regex.substr(1));
    //         CHECK(!x.is_lang_empty());
    //         CHECK(are_equivalent(x, y));
    //     }
    // }

    SECTION("Another failing regex") {
        Nfa x = mata::parser::create_nfa("(cd(abcde)+)|(a(aaa)+|ccc+)");
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

        x = mata::parser::create_nfa("(cd(abcde)+)|(a(aaa)+|ccc+)", false, 306, false);
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
        Nfa x = mata::parser::create_nfa("(cd(abcde)*)|(a(aaa)*)");
        CHECK(x.is_in_lang(Run{Word{'a', 'a', 'a', 'a'}, {}}));
    }
} // }}}

TEST_CASE("mata::Parser Latin1 encoding")
{ // {{{
    SECTION("below 0x80")
    {
        Nfa x = mata::parser::create_nfa("\\x{01}\\x{10}\\x{20}\\x{30}\\x{40}\\x{50}\\x{60}\\x{70}\\x{7f}");
        CHECK(x.is_in_lang(Run{Word{0x01, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7f}, {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0x01, 1);
        y.delta.add(1, 0x10, 2);
        y.delta.add(2, 0x20, 3);
        y.delta.add(3, 0x30, 4);
        y.delta.add(4, 0x40, 5);
        y.delta.add(5, 0x50, 6);
        y.delta.add(6, 0x60, 7);
        y.delta.add(7, 0x70, 8);
        y.delta.add(8, 0x7f, 9);
        y.final.insert(9);
        CHECK(are_equivalent(x, y));
    }

    SECTION("above 0x80")
    {
        Nfa x = mata::parser::create_nfa("\\x{80}\\x{90}\\x{a0}\\x{b0}\\x{c0}\\x{d0}\\x{e0}\\x{f0}\\x{ff}");
        CHECK(x.is_in_lang(Run{Word{0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xff}, {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0x80, 1);
        y.delta.add(1, 0x90, 2);
        y.delta.add(2, 0xa0, 3);
        y.delta.add(3, 0xb0, 4);
        y.delta.add(4, 0xc0, 5);
        y.delta.add(5, 0xd0, 6);
        y.delta.add(6, 0xe0, 7);
        y.delta.add(7, 0xf0, 8);
        y.delta.add(8, 0xff, 9);
        y.final.insert(9);
        CHECK(are_equivalent(x, y));
    }
} // }}}

TEST_CASE("mata::Parser UTF-8 encoding")
{ // {{{
    SECTION("below 0x80")
    {
        Nfa x = mata::parser::create_nfa("\\x{01}\\x{10}\\x{20}\\x{30}\\x{40}\\x{50}\\x{60}\\x{70}\\x{7f}", false, 306, true, Encoding::UTF8);
        CHECK(x.is_in_lang(Run{Word{0x01, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7f}, {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0x01, 1);
        y.delta.add(1, 0x10, 2);
        y.delta.add(2, 0x20, 3);
        y.delta.add(3, 0x30, 4);
        y.delta.add(4, 0x40, 5);
        y.delta.add(5, 0x50, 6);
        y.delta.add(6, 0x60, 7);
        y.delta.add(7, 0x70, 8);
        y.delta.add(8, 0x7f, 9);
        y.final.insert(9);
        CHECK(are_equivalent(x, y));
    }

    SECTION("between 0x80 and 0x800")
    {
        Nfa x = mata::parser::create_nfa("\\x{80}\\x{90}\\x{a0}\\x{b0}\\x{c0}\\x{d0}\\x{600}\\x{700}\\x{7ff}", false, 306, true, Encoding::UTF8);
        CHECK(x.is_in_lang(Run{mata::encode_word_utf8(Word{0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0x600, 0x700, 0x7ff}), {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0xc2, 1);
        y.delta.add(1, 0x80, 2);
        y.delta.add(2, 0xc2, 3);
        y.delta.add(3, 0x90, 4);
        y.delta.add(4, 0xc2, 5);
        y.delta.add(5, 0xa0, 6);
        y.delta.add(6, 0xc2, 7);
        y.delta.add(7, 0xb0, 8);
        y.delta.add(8, 0xc3, 9);
        y.delta.add(9, 0x80, 10);
        y.delta.add(10, 0xc3, 11);
        y.delta.add(11, 0x90, 12);
        y.delta.add(12, 0xd8, 13);
        y.delta.add(13, 0x80, 14);
        y.delta.add(14, 0xdc, 15);
        y.delta.add(15, 0x80, 16);
        y.delta.add(16, 0xdf, 17);
        y.delta.add(17, 0xbf, 18);
        y.final.insert(18);
        CHECK(are_equivalent(x, y));
    }

    SECTION("between 0x800 and 0x7FFF")
    {
        Nfa x = mata::parser::create_nfa("\\x{800}\\x{900}\\x{a00}\\x{b00}\\x{c00}\\x{d00}\\x{6000}\\x{7000}\\x{7fff}", false, 306, true, Encoding::UTF8);
        CHECK(x.is_in_lang(Run{mata::encode_word_utf8(Word{0x800, 0x900, 0xa00, 0xb00, 0xc00, 0xd00, 0x6000, 0x7000, 0x7fff}), {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0xe0, 1);
        y.delta.add(1, 0xa0, 2);
        y.delta.add(2, 0x80, 3);
        y.delta.add(3, 0xe0, 4);
        y.delta.add(4, 0xa4, 5);
        y.delta.add(5, 0x80, 6);
        y.delta.add(6, 0xe0, 7);
        y.delta.add(7, 0xa8, 8);
        y.delta.add(8, 0x80, 9);
        y.delta.add(9, 0xe0, 10);
        y.delta.add(10, 0xac, 11);
        y.delta.add(11, 0x80, 12);
        y.delta.add(12, 0xe0, 13);
        y.delta.add(13, 0xb0, 14);
        y.delta.add(14, 0x80, 15);
        y.delta.add(15, 0xe0, 16);
        y.delta.add(16, 0xb4, 17);
        y.delta.add(17, 0x80, 18);
        y.delta.add(18, 0xe6, 19);
        y.delta.add(19, 0x80, 20);
        y.delta.add(20, 0x80, 21);
        y.delta.add(21, 0xe7, 22);
        y.delta.add(22, 0x80, 23);
        y.delta.add(23, 0x80, 24);
        y.delta.add(24, 0xe7, 25);
        y.delta.add(25, 0xbf, 26);
        y.delta.add(26, 0xbf, 27);
        y.final.insert(27);
    }

    SECTION("between 0x10000 and 0x10FFFF")
    {
        Nfa x = mata::parser::create_nfa("\\x{10000}\\x{20000}\\x{30000}\\x{40000}\\x{50000}\\x{60000}\\x{70000}\\x{80000}\\x{10ffff}", false, 306, true, Encoding::UTF8);
        CHECK(x.is_in_lang(Run{mata::encode_word_utf8(Word{0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x60000, 0x70000, 0x80000, 0x10FFFF}), {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0xf0, 1);
        y.delta.add(1, 0x90, 2);
        y.delta.add(2, 0x80, 3);
        y.delta.add(3, 0x80, 4);
        y.delta.add(4, 0xf0, 5);
        y.delta.add(5, 0xa0, 6);
        y.delta.add(6, 0x80, 7);
        y.delta.add(7, 0x80, 8);
        y.delta.add(8, 0xf0, 9);
        y.delta.add(9, 0xb0, 10);
        y.delta.add(10, 0x80, 11);
        y.delta.add(11, 0x80, 12);
        y.delta.add(12, 0xf1, 13);
        y.delta.add(13, 0x80, 14);
        y.delta.add(14, 0x80, 15);
        y.delta.add(15, 0x80, 16);
        y.delta.add(16, 0xf1, 17);
        y.delta.add(17, 0x90, 18);
        y.delta.add(18, 0x80, 19);
        y.delta.add(19, 0x80, 20);
        y.delta.add(20, 0xf1, 21);
        y.delta.add(21, 0xa0, 22);
        y.delta.add(22, 0x80, 23);
        y.delta.add(23, 0x80, 24);
        y.delta.add(24, 0xf1, 25);
        y.delta.add(25, 0xb0, 26);
        y.delta.add(26, 0x80, 27);
        y.delta.add(27, 0x80, 28);
        y.delta.add(28, 0xf2, 29);
        y.delta.add(29, 0x80, 30);
        y.delta.add(30, 0x80, 31);
        y.delta.add(31, 0x80, 32);
        y.delta.add(32, 0xf4, 33);
        y.delta.add(33, 0x8f, 34);
        y.delta.add(34, 0xbf, 35);
        y.delta.add(35, 0xbf, 36);
        y.final.insert(36);
        CHECK(are_equivalent(x, y));
    }

    SECTION("mix")
    {
        Nfa x = mata::parser::create_nfa("\\x{01}\\x{90}\\x{8ac}\\x{100cc}", false, 306, true, Encoding::UTF8);
        CHECK(x.is_in_lang(Run{mata::encode_word_utf8(Word{0x01, 0x90, 0x8ac, 0x100cc}), {}}));
        Nfa y;
        y.initial.insert(0);
        y.delta.add(0, 0x01, 1);
        y.delta.add(1, 0xc2, 2);
        y.delta.add(2, 0x90, 3);
        y.delta.add(3, 0xe0, 4);
        y.delta.add(4, 0xa2, 5);
        y.delta.add(5, 0xac, 6);
        y.delta.add(6, 0xf0, 7);
        y.delta.add(7, 0x90, 8);
        y.delta.add(8, 0x83, 9);
        y.delta.add(9, 0x8c, 10);
        y.final.insert(10);
        CHECK(are_equivalent(x, y));
    }

    SECTION("Regex range [x70-x90]") {
        Nfa aut = mata::parser::create_nfa("[\\x{70}-\\x{90}]", false, 306, true, Encoding::UTF8);
        aut = aut.decode_utf8();

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for(Symbol c = 0x70; c <= 0x90; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(aut, result));
    }

    SECTION("Regex range [x790-x890]") {
        Nfa aut = mata::parser::create_nfa("[\\x{700}-\\x{900}]", false, 306, true, Encoding::UTF8);
        aut = aut.decode_utf8();

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for(Symbol c = 0x700; c <= 0x900; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(aut, result));
    }

    SECTION("Regex range [xFF90-x10090]") {
        Nfa aut = mata::parser::create_nfa("[\\x{FF90}-\\x{10090}]", false, 306, true, Encoding::UTF8);
        aut = aut.decode_utf8();

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for(Symbol c = 0xFF90; c <= 0x10090; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(aut, result));
    }

    SECTION("Regex (\\x{60}*\\x{80})|(\\x{900}*\\x{600})") {
        Nfa aut = mata::parser::create_nfa("(\\x{60}*\\x{80})|(\\x{900}*\\x{600})", false, 306, true, Encoding::UTF8);
        aut = aut.decode_utf8();

        Nfa result;
        result.delta.add(0, 0x60, 0);
        result.delta.add(0, 0x80, 1);
        result.delta.add(2, 0x900, 2);
        result.delta.add(2, 0x600, 3);
        result.initial.insert(0);
        result.initial.insert(2);
        result.final.insert(1);
        result.final.insert(3);
        CHECK(are_equivalent(aut, result));
    }

    // A proper test, but takes about 2 seconds to run.
    SECTION("Regex [\\x{00}-\\x{10FFFF}]") {
        Nfa aut = mata::parser::create_nfa("[\\x{00}-\\x{10FFFF}]", false, 306, true, Encoding::UTF8);
        aut = aut.decode_utf8();

        // Random symbols
        std::vector<Symbol> symbols = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x7f, 0x80,
        0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xff, 0x100, 0x110, 0x36f0, 0x57fc, 0x6177, 0x7498,
        0x8f3f, 0x9fc8, 0x1101e, 0x14348, 0x14e34, 0x19581, 0x1c48e, 0x1f1cc, 0x1f91d, 0x222a6, 0x22e11,
        0xe54f5, 0xe7934, 0xe93a4, 0xe998d, 0xebee8, 0xedb9e, 0xef98b, 0xf12af, 0xf51e2, 0xf557f, 0xf6b08,
        0xfa7f0, 0xfacb2, 0xfd719, 0x106d12, 0x106d66, 0x109220, 0x10a608, 0x10c1f5, 0x10FFFF };
        for(const Symbol c : symbols) {
            CHECK(aut.is_in_lang(Run{Word{c}, {}}));
        }
    }

} // }}}

TEST_CASE("mata::parser Parsing regexes with ^ and $") {
    Nfa nfa;
    Nfa expected{};

    SECTION("Handling of '\\'") {
        nfa = mata::parser::create_nfa("a\\\\b");
        expected = mata::nfa::builder::parse_from_mata(
        std::string{ R"(
            @NFA-explicit
            %Alphabet-auto
            %Initial q0
            %Final q3
            q0 97 q1
            q1 92 q2
            q2 98 q3)"
        });
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("a|b$, a simple OR example with end marker") {
        nfa = mata::parser::create_nfa("a|b$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("^a|b, a simple OR example with begin marker") {
        nfa = mata::parser::create_nfa("^a|b");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("^a|b$, a simple OR example with begin and end marker") {
        nfa = mata::parser::create_nfa("^a|b$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("^(a|b)$, a simple OR example with begin and end marker around capture group") {
        nfa = mata::parser::create_nfa("^(a|b)$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("a$|b, a simple OR example with end marker on the left side") {
        nfa = mata::parser::create_nfa("a$|b");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("^a$|^b$, a simple OR example with multiple begin and end markers") {
        nfa = mata::parser::create_nfa("^a$|^b$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(0, 'b', 1);
        expected.final.insert(1);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("aed|(bab)$, a simple OR example with trailing end marker") {
        nfa = mata::parser::create_nfa("aed|(bab)$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'e', 2);
        expected.delta.add(2, 'd', 3);
        expected.delta.add(0, 'b', 4);
        expected.delta.add(4, 'a', 5);
        expected.delta.add(5, 'b', 3);
        expected.final.insert(3);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("aed|bab$, a simple OR example with trailing end marker") {
        nfa = mata::parser::create_nfa("aed|bab$");
        expected.initial.insert(0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'e', 2);
        expected.delta.add(2, 'd', 3);
        expected.delta.add(0, 'b', 4);
        expected.delta.add(4, 'a', 5);
        expected.delta.add(5, 'b', 3);
        expected.final.insert(3);
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }

    SECTION("^systempath\\=https|ftp$ correct parentheses") {
        nfa = mata::parser::create_nfa("^[sS][yY][sS][tT][eE][mM][pP][aA][tT][hH]\\\\=(([hH][tT]{2}[pP][sS]?)|([fF][tT][pP]))$");
        expected = mata::nfa::builder::parse_from_mata(std::string{ R"(
            @NFA-explicit
            %Alphabet-auto
            %Initial q0
            %Final q16 q17
            q0 83 q1
            q0 115 q1
            q1 89 q2
            q1 121 q2
            q2 83 q3
            q2 115 q3
            q3 84 q4
            q3 116 q4
            q4 69 q5
            q4 101 q5
            q5 77 q6
            q5 109 q6
            q6 80 q7
            q6 112 q7
            q7 65 q8
            q7 97 q8
            q8 84 q9
            q8 116 q9
            q9 72 q10
            q9 104 q10
            q10 92 q11
            q11 61 q12
            q12 70 q18
            q12 72 q13
            q12 102 q18
            q12 104 q13
            q13 84 q14
            q13 116 q14
            q14 84 q15
            q14 116 q15
            q15 80 q16
            q15 112 q16
            q16 83 q17
            q16 115 q17
            q18 84 q19
            q18 116 q19
            q19 80 q17
            q19 112 q17
            )"
        });
        CHECK(mata::nfa::are_equivalent(nfa, expected));
    }
}

TEST_CASE("Foldcase") {
    Nfa nfa;

    SECTION("Regex [a-z]") {
        nfa = mata::parser::create_nfa("[a-z]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'a'; c <= 'z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [A-Z]") {
        nfa = mata::parser::create_nfa("[A-Z]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'A'; c <= 'Z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [A-Za-z]") {
        nfa = mata::parser::create_nfa("[A-Za-z]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'A'; c <= 'Z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        for (Symbol c = 'a'; c <= 'z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [a-zA-Z]") {
        nfa = mata::parser::create_nfa("[a-zA-Z]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'A'; c <= 'Z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        for (Symbol c = 'a'; c <= 'z'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [M-Ya-x]") {
        nfa = mata::parser::create_nfa("[M-Ya-x]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'a'; c <= 'x'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        for (Symbol c = 'M'; c <= 'Y'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [\\x00-\\x5a\\x5c-\\x7F]") {
        nfa = mata::parser::create_nfa("[\\x00-\\x5a\\x5c-\\x7F]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 0; c <= 0x7F; c++) {
            if (c == 0x5B) {
                continue;
            }
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [A-Ma-m]") {
        nfa = mata::parser::create_nfa("[A-Ma-m]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 'A'; c <= 'M'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        for (Symbol c = 'a'; c <= 'm'; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }

    SECTION("Regex [\\x00-\\x7F]") {
        nfa = mata::parser::create_nfa("[\\x00-\\x7F]");

        Nfa result;
        State initial_s = 0;
        State final_s = 1;
        result.initial.insert(initial_s);
        result.final.insert(final_s);
        for (Symbol c = 0; c <= 0x7F; c++) {
            result.delta.add(initial_s, c, final_s);
        }
        CHECK(are_equivalent(nfa, result));
    }
}
