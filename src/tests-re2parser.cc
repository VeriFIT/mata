#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/re2parser.hh>
using namespace Mata::Nfa;

TEST_CASE("Mata::RE2Parser basic_parsing")
{ // {{{
    Mata::Nfa::Nfa aut;

    SECTION("Empty expression")
    {
        Mata::RE2Parser::create_nfa(&aut, "");
        REQUIRE(aut.finalstates.size() == aut.initialstates.size());
        REQUIRE(aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{}));
    }

    SECTION("Basic test")
    {
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
        Mata::RE2Parser::create_nfa(&aut, "\\x7f");
        REQUIRE(!aut.trans_empty());
        REQUIRE(!is_lang_empty(aut));
        REQUIRE(is_in_lang(aut, Word{127}));
    }

    SECTION("Wild card")
    {
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

    SECTION("Additional parenthesis") {
        Nfa expected{2};
        expected.make_initial(0);
        expected.make_final(1);
        expected.add_trans(0, 'a', 0);
        expected.add_trans(0, 'b', 1);

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

        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'a','b'}));
        CHECK(is_in_lang(aut, Word{'a','a','b'}));
        CHECK(!is_in_lang(aut, Word{'b','a'}));
        CHECK(equivalence_check(aut, expected));
    }

    SECTION("Complex regex") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)(b+)");
        CHECK(!aut.trans_empty());
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

    SECTION("Complex regex with additional plus") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)|(e)(w*)+(b+)");
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(!is_in_lang(aut, Word{'a', 'a'}));
        CHECK(!is_in_lang(aut, Word{'e'}));
        CHECK(is_in_lang(aut, Word{'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b'}));
        CHECK(!is_in_lang(aut, Word{'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'b', 'b'}));
        CHECK(is_in_lang(aut, Word{'w', 'w', 'b', 'b'}));
        CHECK(!is_in_lang(aut, Word{'w'}));
        CHECK(!is_in_lang(aut, Word{'a', 'w', 'b'}));
    }

    SECTION("Reduced complex regex with additional plus 3") {
        Mata::RE2Parser::create_nfa(&aut, "(b+)");
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
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
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)a+") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)a+");
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("a(a+)") {
        Mata::RE2Parser::create_nfa(&aut, "a(a+)");
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'a'}));
    }

    SECTION("(a+)b") {
        Mata::RE2Parser::create_nfa(&aut, "(a+)b");
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'b'}));
        CHECK(is_in_lang(aut, Word{'a', 'a', 'a', 'b'}));
    }

    SECTION("b(a+)") {
        Mata::RE2Parser::create_nfa(&aut, "b(a+)");
        CHECK(!aut.trans_empty());
        CHECK(!is_lang_empty(aut));
        CHECK(!is_in_lang(aut, Word{}));
        CHECK(!is_in_lang(aut, Word{'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'a'}));
        CHECK(is_in_lang(aut, Word{'b', 'a', 'a', 'a'}));
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
