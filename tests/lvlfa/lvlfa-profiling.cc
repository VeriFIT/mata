// TODO: some header

#include <catch2/catch.hpp>
#include "utils.hh"

#include "mata/lvlfa/lvlfa.hh"
#include "mata/parser/re2parser.hh"

using namespace mata::lvlfa;
using namespace mata::parser;

/////////////////////////////
// Profiling revert and trim
/////////////////////////////

TEST_CASE("mata::lvlfa::fragile_revert() speed, simple ", "[.profiling]") {
    Lvlfa B;
    FILL_WITH_AUT_D(B);
    for (int i = 0; i < 300000; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("mata::lvlfa::simple_revert() speed, simple ", "[.profiling]") {
    Lvlfa B;
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("mata::lvlfa::simple_revert() speed, harder", "[.profiling]") {
    Lvlfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    mata::parser::create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("mata::lvlfa::fragile_revert() speed, harder", "[.profiling]") {
    Lvlfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("mata::lvlfa::somewhat_simple_revert() speed, harder", "[.profiling]") {
    Lvlfa B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
//FILL_WITH_AUT_C(B);
    for (int i = 0; i < 200; i++) {
        B = somewhat_simple_revert(B);
    }
}

TEST_CASE("mata::lvlfa::trim_inplace() speed, simple", "[.profiling]") {
    Lvlfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        A = B;
        A.trim();
    }
}

TEST_CASE("mata::lvlfa::trim_inplace() speed, harder", "[.profiling]") {
    Lvlfa A, B;
//this gives an interesting test case if the parser is not trimming and reducing
    create_nfa(&B, "((.*){10})*");
    for (int i = 0; i < 200; i++) {
        A = B;
        A.trim();
    }
}

//////////////////////////////
// Profiling get_used_symbols
//////////////////////////////

TEST_CASE("mata::lvlfa::get_used_symbols speed, harder", "[.profiling]") {
    Lvlfa A;
    create_nfa(&A, "((.*){10})*");
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols();
    }
}

TEST_CASE("mata::lvlfa::get_used_symbols_bv speed, harder", "[.profiling]") {
    Lvlfa A;
    create_nfa(&A, "((.*){10})*");
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_bv();
    }
}

TEST_CASE("mata::lvlfa::get_used_symbols_vec speed, harder", "[.profiling]") {
    Lvlfa A;
    create_nfa(&A, "((.*){10})*");
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_vec();
    }
}

TEST_CASE("mata::lvlfa::get_used_symbols_set speed, harder", "[.profiling]") {
    Lvlfa A;
    create_nfa(&A, "((.*){10})*");
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_set();
    }
}

TEST_CASE("mata::lvlfa::get_used_symbols_sps speed, harder", "[.profiling]") {
    Lvlfa A;
    create_nfa(&A, "((.*){10})*");
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_sps();
    }
}

/////////////////////////////
/////////////////////////////
