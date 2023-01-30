#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/re2parser.hh>
using namespace Mata::Nfa;

using Symbol = Mata::Symbol;
using Word = std::vector<Symbol>;
using OnTheFlyAlphabet = Mata::OnTheFlyAlphabet;

bool is_in_lang(const Nfa& aut, const Word& word)
{
    return is_in_lang(aut, Run{word, {}});
}

// Some example regexes were taken from RegExr under GPL v3: https://github.com/gskinner/regexr.

TEST_CASE("Mata::RE2Parser basic_parsing")
{ // {{{
    Nfa aut;

    SECTION("Empty expression")
    {
        Mata::RE2Parser::create_nfa(&aut, "");
        REQUIRE(aut.final.size() == aut.initial.size());
        REQUIRE(aut.delta.empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{}));
    }

    SECTION("Basic test")
    {
        Mata::RE2Parser::create_nfa(&aut, "abcd");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }

    SECTION("Hex symbol encoding")
    {
        Mata::RE2Parser::create_nfa(&aut, "\\x7f");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{127}));
    }

    SECTION("Wild card")
    {
        Mata::RE2Parser::create_nfa(&aut, ".*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{'w','h','a','t','e','v','e','r'}));
        REQUIRE(is_in_lang(aut, Word{127}));
        REQUIRE(is_in_lang(aut, Word{0x7f}));
        REQUIRE(is_in_lang(aut, Word{}));
        OnTheFlyAlphabet alph{};
        REQUIRE(is_universal(aut,alph));
    }

    SECTION("Special character") {
        Mata::RE2Parser::create_nfa(&aut, "\\t");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{'\t'}));
        CHECK(!is_in_lang(aut, Word{'t'}));
        CHECK(!is_in_lang(aut, Word{}));
    }

    SECTION("Whitespace") {
        Mata::RE2Parser::create_nfa(&aut, "a\\sb");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{'a', '\t', 'b'}));
        CHECK(!is_in_lang(aut, Word{}));
    }

    SECTION("Iteration test")
    {
        Mata::RE2Parser::create_nfa(&aut, "ab*cd*");
        REQUIRE(!aut.delta.empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{'a','b','c'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','b','c','d'}));
        REQUIRE(is_in_lang(aut, Word{'a','b','c','d','d'}));
        REQUIRE(!is_in_lang(aut, Word{'a','d','c'}));
    }

    SECTION("Additional parenthesis") {
        Nfa expected{2};
        expected.initial.add(0);
        expected.final.add(1);
        expected.delta.add(0, 'a', 0);
        expected.delta.add(0, 'b', 1);

        SECTION("No parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "a*b");
        }

        SECTION("Around example parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "(a*b)");
        }

        SECTION("Around variable 'a' parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "(a)*b");
        }

        SECTION("Around variable 'b' parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "a*(b)");
        }

        SECTION("Parenthesis after iteration") {
            Mata::RE2Parser::create_nfa(&aut, "((a)*)b");
        }

        SECTION("Double parenthesis around 'b'") {
            Mata::RE2Parser::create_nfa(&aut, "(a*(b))");
        }

        SECTION("Double parenthesis around 'a'") {
            Mata::RE2Parser::create_nfa(&aut, "((a)*b)");
        }

        SECTION("Many parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "(((a)*)b)");
        }

        SECTION("Double parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "((a))*((b))");
        }

        SECTION("Double parenthesis after iteration") {
            Mata::RE2Parser::create_nfa(&aut, "((((a))*))((b))");
        }

        SECTION("Many parenthesis with double parenthesis") {
            Mata::RE2Parser::create_nfa(&aut, "(((((a))*))((b)))");
        }

        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'a','b'}));
        CHECK(is_in_lang(aut, Word{'a','a','b'}));
        CHECK(!is_in_lang(aut, Word{'b','a'}));
        CHECK(are_equivalent(aut, expected));
    }

    SECTION("Complex regex") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'e', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e', 'b'}));
    }

    SECTION("Complex regex with additional plus") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'e', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus") {
        Mata::RE2Parser::create_nfa(&aut, "(e)(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'e', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2") {
        Mata::RE2Parser::create_nfa(&aut, "(w*)+(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.5") {
        Mata::RE2Parser::create_nfa(&aut, "(w*)(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.63") {
        Mata::RE2Parser::create_nfa(&aut, "w*b+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.75") {
        Mata::RE2Parser::create_nfa(&aut, "w(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 2.85") {
        Mata::RE2Parser::create_nfa(&aut, "w*(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 3") {
        Mata::RE2Parser::create_nfa(&aut, "(b+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Complex regex 2") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'e', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w'}));
        CHECK(is_in_lang(aut, Word{'e', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Complex regex 2 with additional plus") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)+(b*)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'e', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w'}));
        CHECK(is_in_lang(aut, Word{'e', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'e', 'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("a+b+") {
        Mata::RE2Parser::create_nfa(&aut, "a+b+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a', 'b', 'a'}));
    }

    SECTION("a+b+a*") {
        Mata::RE2Parser::create_nfa(&aut, "a+b+a*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("a+(b+)a*") {
        Mata::RE2Parser::create_nfa(&aut, "a+(b+)a*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("(a+(b+)a*)") {
        Mata::RE2Parser::create_nfa(&aut, "(a+(b+)a*)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("(a+b*a*)") {
        Mata::RE2Parser::create_nfa(&aut, "(a+b*a*)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b', 'b', 'a', 'a'}));
    }

    SECTION("a+a+") {
        Mata::RE2Parser::create_nfa(&aut, "a+a+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)a+") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)a+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("a(a+)") {
        Mata::RE2Parser::create_nfa(&aut, "a(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)b") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)b");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b'}));
    }

    SECTION("b(a+)") {
        Mata::RE2Parser::create_nfa(&aut, "b(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'a', 'a'}));
    }

    SECTION("b|(a+)") {
        Mata::RE2Parser::create_nfa(&aut, "b|(a+)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
    }

    SECTION("b|a+") {
        Mata::RE2Parser::create_nfa(&aut, "b|a+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
    }

    SECTION("b|a") {
        Mata::RE2Parser::create_nfa(&aut, "b|a");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a', 'a'}));
    }

    SECTION("b|a*") {
        Mata::RE2Parser::create_nfa(&aut, "b|a*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a'}));
        CHECK(!is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
    }

    SECTION("bba+") {
        Mata::RE2Parser::create_nfa(&aut, "bba+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a', 'a', 'a'}));
    }

    SECTION("b*ba+") {
        Mata::RE2Parser::create_nfa(&aut, "b*ba+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'a', 'a', 'a'}));
    }

    SECTION("b*ca+") {
        Mata::RE2Parser::create_nfa(&aut, "b*ca+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'c', 'a'}));
        CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'c', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b', 'c', 'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'c', 'a', 'a', 'a'}));
    }

    SECTION("[abcd]") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
    }

    SECTION("[abcd]*") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'c'}));
        CHECK(is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c'}));
    }

    SECTION("[abcd]*e*") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]*e*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'c'}));
        CHECK(is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c'}));

        CHECK(is_in_lang(aut, Word{'a', 'e'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e', 'e'}));
    }

    SECTION("[abcd]*e+") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]*e+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'c'}));
        CHECK(!is_in_lang(aut, Word{'d'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'c', 'c'}));
        CHECK(!is_in_lang(aut, Word{'d', 'd'}));
        CHECK(!is_in_lang(aut, Word{'a', 'd'}));
        CHECK(!is_in_lang(aut, Word{'a', 'd', 'c'}));

        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'e'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e', 'e'}));
    }

    SECTION("[abcd]*.*") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]*.*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'c'}));
        CHECK(is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c'}));

        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'e'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e', 'e'}));

        CHECK(is_in_lang(aut, Word{'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'g'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'g', 'g'}));
    }

    SECTION("[abcd]*.+") {
        Mata::RE2Parser::create_nfa(&aut, "[abcd]*.+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'c'}));
        CHECK(is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c'}));

        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'e'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'e', 'e'}));

        CHECK(is_in_lang(aut, Word{'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'g'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'd', 'c', 'g', 'g'}));
    }

    SECTION("[a-c]+") {
        Mata::RE2Parser::create_nfa(&aut, "[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(!is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'c', 'c'}));
        CHECK(!is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
    }

    SECTION("d[a-c]+") {
        Mata::RE2Parser::create_nfa(&aut, "d[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'c'}));
        CHECK(!is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'d', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'d', 'c', 'c'}));
        CHECK(!is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'b', 'c'}));
    }

    SECTION("d*[a-c]+") {
        Mata::RE2Parser::create_nfa(&aut, "d*[a-c]+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'c'}));
        CHECK(!is_in_lang(aut, Word{'d'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'d', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'d', 'c', 'c'}));
        CHECK(!is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'d', 'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'d', 'd', 'a', 'b', 'c'}));
    }

    SECTION("[^a-c]") {
        Mata::RE2Parser::create_nfa(&aut, "[^a-c]");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'c'}));
        CHECK(is_in_lang(aut, Word{'d'}));
        CHECK(!is_in_lang(aut, Word{'d', 'd'}));
        CHECK(is_in_lang(aut, Word{'e'}));
        CHECK(!is_in_lang(aut, Word{'e', 'e'}));
    }

    SECTION("(ha)+") {
        Mata::RE2Parser::create_nfa(&aut, "(ha)+");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'h'}));
        CHECK(is_in_lang(aut, Word{'h', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'h'}));
        CHECK(is_in_lang(aut, Word{'h', 'a', 'h', 'a'}));
        CHECK(!is_in_lang(aut, Word{'h', 'a', 'h'}));
        CHECK(!is_in_lang(aut, Word{'h', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'h', 'a', 'h', 'a', 'h', 'a'}));
    }

    SECTION("(ha)*") {
        Mata::RE2Parser::create_nfa(&aut, "(ha)*");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'h'}));
        CHECK(is_in_lang(aut, Word{'h', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'h'}));
        CHECK(is_in_lang(aut, Word{'h', 'a', 'h', 'a'}));
        CHECK(!is_in_lang(aut, Word{'h', 'a', 'h'}));
        CHECK(!is_in_lang(aut, Word{'h', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'h', 'a', 'h', 'a', 'h', 'a'}));
    }

    SECTION("b\\w{2,3}") {
        Mata::RE2Parser::create_nfa(&aut, "b\\w{2,3}");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'e'}));
        CHECK(is_in_lang(aut, Word{'b', 'e', 'e'}));
        CHECK(is_in_lang(aut, Word{'b', 'e', 'e', 'r'}));
        CHECK(!is_in_lang(aut, Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b\\w+?") {
        Mata::RE2Parser::create_nfa(&aut, "b\\w+?");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'b', 'e'}));
        CHECK(!is_in_lang(aut, Word{'b', 'e', 'e'}));
        CHECK(!is_in_lang(aut, Word{'b', 'e', 'e', 'r'}));
        CHECK(!is_in_lang(aut, Word{'b', 'e', 'e', 'r', 's'}));
    }

    SECTION("b(a|e|i)d") {
        Mata::RE2Parser::create_nfa(&aut, "b(a|e|i)d");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'd'}));
        CHECK(!is_in_lang(aut, Word{'b', 'u', 'd'}));
        CHECK(!is_in_lang(aut, Word{'b', 'o', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'e', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'i', 'd'}));
    }

    SECTION("[ab](c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("[ab](c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "[ab](c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("[ab]+(c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "[ab]+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'a', 'c'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("([ab])+(c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'a', 'c'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("(([ab])+)(c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "(([ab])+)(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'a', 'c'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("g|((([ab])+)(c|d))") {
        Mata::RE2Parser::create_nfa(&aut, "(g|(([ab])+))(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'a', 'c'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
        CHECK(is_in_lang(aut, Word{'g', 'c'}));
        CHECK(is_in_lang(aut, Word{'g', 'd'}));
    }

    SECTION("g|([ab])+(c|d)") {
        Mata::RE2Parser::create_nfa(&aut, "g|([ab])+(c|d)");
        CHECK(!aut.delta.empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(is_in_lang(aut, Word{'g'}));
        CHECK(is_in_lang(aut, Word{'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'd'}));
        CHECK(is_in_lang(aut, Word{'b', 'd'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'c'}));
        CHECK(is_in_lang(aut, Word{'a', 'b', 'a', 'c'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'c', 'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'e'}));
        CHECK(!is_in_lang(aut, Word{'a', 'c', 'd'}));
    }

    SECTION("Star iteration") {
        Nfa expected{2};
        expected.initial.add(0);
        expected.final.add({0, 1});
        expected.delta.add(0, 'c', 0);
        expected.delta.add(0, 'a', 1);
        expected.delta.add(1, 'a', 1);

        SECTION("(((c)*)((a)*))") {
            Mata::RE2Parser::create_nfa(&aut, "(((c)*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(is_in_lang(aut, Word{}));
            CHECK(is_in_lang(aut, Word{'c'}));
            CHECK(is_in_lang(aut, Word{'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c'}));
            CHECK(is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("((c*)((a)*))") {
            Mata::RE2Parser::create_nfa(&aut, "((c*)((a)*))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(is_in_lang(aut, Word{}));
            CHECK(is_in_lang(aut, Word{'c'}));
            CHECK(is_in_lang(aut, Word{'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c'}));
            CHECK(is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(c*(a*))") {
            Mata::RE2Parser::create_nfa(&aut, "(c*(a*))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(is_in_lang(aut, Word{}));
            CHECK(is_in_lang(aut, Word{'c'}));
            CHECK(is_in_lang(aut, Word{'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c'}));
            CHECK(is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(c*a*)") {
            Mata::RE2Parser::create_nfa(&aut, "(c*a*)");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(is_in_lang(aut, Word{}));
            CHECK(is_in_lang(aut, Word{'c'}));
            CHECK(is_in_lang(aut, Word{'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c'}));
            CHECK(is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("c*a*") {
            Mata::RE2Parser::create_nfa(&aut, "c*a*");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(is_in_lang(aut, Word{}));
            CHECK(is_in_lang(aut, Word{'c'}));
            CHECK(is_in_lang(aut, Word{'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c'}));
            CHECK(is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            CHECK(are_equivalent(aut, expected));
        }

        SECTION("(((c)+)((a)+))") {
            Mata::RE2Parser::create_nfa(&aut, "(((c)+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("((c+)((a)+))") {
            Mata::RE2Parser::create_nfa(&aut, "((c+)((a)+))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("((c+)(a+))") {
            Mata::RE2Parser::create_nfa(&aut, "((c+)(a+))");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("(c+)(a+)") {
            Mata::RE2Parser::create_nfa(&aut, "(c+)(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("c+(a+)") {
            Mata::RE2Parser::create_nfa(&aut, "c+(a+)");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("(c+)a+") {
            Mata::RE2Parser::create_nfa(&aut, "(c+)a+");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }

        SECTION("c+a+") {
            Mata::RE2Parser::create_nfa(&aut, "c+a+");
            CHECK(!aut.delta.empty());
            CHECK(!is_lang_empty(aut));
            CHECK(!is_in_lang(aut, Word{}));
            CHECK(!is_in_lang(aut, Word{'c'}));
            CHECK(!is_in_lang(aut, Word{'a'}));
            CHECK(!is_in_lang(aut, Word{'c', 'c'}));
            CHECK(!is_in_lang(aut, Word{'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a'}));
            CHECK(!is_in_lang(aut, Word{'a', 'c'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'a', 'a'}));
            CHECK(is_in_lang(aut, Word{'c', 'c', 'a', 'a'}));
            Nfa expected_plus_iteration{ 3 };
            expected_plus_iteration.initial.add(0);
            expected_plus_iteration.final.add(2);
            expected_plus_iteration.delta.add(0, 'c', 1);
            expected_plus_iteration.delta.add(1, 'c', 1);
            expected_plus_iteration.delta.add(1, 'a', 2);
            expected_plus_iteration.delta.add(2, 'a', 2);
            CHECK(are_equivalent(aut, expected_plus_iteration));
        }
    }
} // }}}

TEST_CASE("Mata::RE2Parser error")
{ // {{{
    SECTION("Complex regex that fails")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "((aa)*)*(b)*");
        REQUIRE(!aut.delta.empty());
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
        REQUIRE(!aut1.delta.empty());
        REQUIRE(!is_lang_empty(aut1));
        REQUIRE(!aut2.delta.empty());
        REQUIRE(!is_lang_empty(aut2));
        REQUIRE(is_in_lang(aut1, Word{'Q','R','q','r'}));
        REQUIRE(is_in_lang(aut2, Word{'q','r','q','r'}));
        REQUIRE(!is_in_lang(aut2, Word{'q','R','q'}));
    }

    SECTION("Regex from issue #139") {
        Nfa x;
        Mata::RE2Parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)");
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));

        x.clear();
        Mata::RE2Parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)", false, 306, false);
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
    }

    SECTION("Another failing regex") {
        Nfa x;
        Mata::RE2Parser::create_nfa(&x, "(cd(abcde)+)|(a(aaa)+|ccc+)");
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'c', 'c' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'd' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'c' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'c', 'c', 'c', 'c', 'c' }, {} }));

        x.clear();
        Mata::RE2Parser::create_nfa(&x, "(cd(abcde)+)|(a(aaa)+|ccc+)", false, 306, false);
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'a', 'a', 'a', 'a', 'a', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'd', 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'e' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'c', 'c' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'd' }, {} }));
        CHECK(!is_in_lang(x, Run{ Word{ 'c', 'c' }, {} }));
        CHECK(is_in_lang(x, Run{ Word{ 'c', 'c', 'c', 'c', 'c', 'c' }, {} }));
    }
} // }}}

TEST_CASE("Mata::RE2Parser bug epsilon")
{ // {{{
    SECTION("failing regex")
    {
        Nfa x;
        Mata::RE2Parser::create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)");
        CHECK(is_in_lang(x, Run{Word{'a', 'a', 'a', 'a'}, {}}));
    }
} // }}}
