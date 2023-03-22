// TODO: some header

#include <unordered_set>

#include "../3rdparty/catch.hpp"
#include "tests-nfa-util.hh"

#include <mata/nfa.hh>
#include <mata/nfa-plumbing.hh>
#include <mata/nfa-strings.hh>
#include <mata/nfa-algorithms.hh>
#include <mata/re2parser.hh>

using namespace Mata::Nfa;
using namespace Mata::RE2Parser;

/////////////////////////////
// Profiling revert and trim
/////////////////////////////

TEST_CASE("Mata::Nfa::fragile_revert() speed, simple ", "[.profiling]") {
    Nfa B;
    FILL_WITH_AUT_D(B);
    for (int i = 0; i < 300000; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("Mata::Nfa::simple_revert() speed, simple ", "[.profiling]") {
    Nfa B;
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("Mata::Nfa::simple_revert() speed, harder", "[.profiling]") {
    Nfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    Mata::RE2Parser::create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("Mata::Nfa::fragile_revert() speed, harder", "[.profiling]") {
    Nfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("Mata::Nfa::somewhat_simple_revert() speed, harder", "[.profiling]") {
    Nfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
//FILL_WITH_AUT_C(B);
    for (int i = 0; i < 200; i++) {
        B = somewhat_simple_revert(B);
    }
}

TEST_CASE("Mata::Nfa::trim_inplace() speed, simple", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        A = B;
        A.trim_inplace();
    }
}

TEST_CASE("Mata::Nfa::trim_reverting() speed, simple", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        A = B;
        A.trim_reverting();
    }
}

TEST_CASE("Mata::Nfa::trim_inplace() speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        A = B;
        A.trim_inplace();
    }
}

TEST_CASE("Mata::Nfa::trim_reverting() speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        A = B;
        A.trim_reverting();
    }
}

//////////////////////////////
// Profiling get_used_symbols
//////////////////////////////

TEST_CASE("Mata::Nfa::get_used_symbols speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 20000000; i++) {
        A.get_used_symbols();
    }
}

TEST_CASE("Mata::Nfa::get_used_symbols_bv speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 20000000; i++) {
        A.get_used_symbols_bv();
    }
}

TEST_CASE("Mata::Nfa::get_used_symbols_vec speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 20000000; i++) {
        A.get_used_symbols_vec();
    }
}

TEST_CASE("Mata::Nfa::get_used_symbols_set speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 20000000; i++) {
        A.get_used_symbols_set();
    }
}

TEST_CASE("Mata::Nfa::get_used_symbols_np speed, harder", "[.profiling]") {
    Nfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 20000000; i++) {
        A.get_used_symbols_np();
    }
}

/////////////////////////////
/////////////////////////////
