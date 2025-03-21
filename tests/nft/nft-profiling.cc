// TODO: some header

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "utils.hh"

#include "mata/nft/nft.hh"
#include "mata/parser/re2parser.hh"

using namespace mata::nft;
using namespace mata::parser;

/////////////////////////////
// Profiling revert and trim
/////////////////////////////

TEST_CASE("mata::nft::fragile_revert() speed, simple ", "[.profiling]") {
    Nft B;
    FILL_WITH_AUT_D(B);
    for (int i = 0; i < 300000; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("mata::nft::simple_revert() speed, simple ", "[.profiling]") {
    Nft B;
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("mata::nft::simple_revert() speed, harder", "[.profiling]") {
    //this gives an interesting test case if the parser is not trimming and reducing
    Nft B { mata::parser::create_nfa("((.*){10})*", false, EPSILON, false) };
    for (int i = 0; i < 200; i++) {
        B = simple_revert(B);
    }
}

TEST_CASE("mata::nft::fragile_revert() speed, harder", "[.profiling]") {
    //this gives an interesting test case if the parser is not trimming and reducing
    Nft B { mata::parser::create_nfa("((.*){10})*", false, EPSILON, false) };
    for (int i = 0; i < 200; i++) {
        B = fragile_revert(B);
    }
}

TEST_CASE("mata::nft::somewhat_simple_revert() speed, harder", "[.profiling]") {
    //this gives an interesting test case if the parser is not trimming and reducing
    Nft B { mata::parser::create_nfa("((.*){10})*", false, EPSILON, false) };
    for (int i = 0; i < 200; i++) {
        B = somewhat_simple_revert(B);
    }
}

TEST_CASE("mata::nft::trim_inplace() speed, simple", "[.profiling]") {
    Nft A, B;
    FILL_WITH_AUT_B(B);
    for (int i = 0; i < 300000; i++) {
        A = B;
        A.trim();
    }
}

TEST_CASE("mata::nft::trim_inplace() speed, harder", "[.profiling]") {
    Nft A;
    //this gives an interesting test case if the parser is not trimming and reducing
    Nft B { mata::parser::create_nfa("((.*){10})*", false, EPSILON, false) };
    for (int i = 0; i < 200; i++) {
        A = B;
        A.trim();
    }
}

//////////////////////////////
// Profiling get_used_symbols
//////////////////////////////

TEST_CASE("mata::nft::get_used_symbols speed, harder", "[.profiling]") {
    Nft A { create_nfa("((.*){10})*") };
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols();
    }
}

TEST_CASE("mata::nft::get_used_symbols_bv speed, harder", "[.profiling]") {
    Nft A { create_nfa("((.*){10})*") };
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_bv();
    }
}

TEST_CASE("mata::nft::get_used_symbols_vec speed, harder", "[.profiling]") {
    Nft A { create_nfa("((.*){10})*") };
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_vec();
    }
}

TEST_CASE("mata::nft::get_used_symbols_set speed, harder", "[.profiling]") {
    Nft A { create_nfa("((.*){10})*") };
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_set();
    }
}

TEST_CASE("mata::nft::get_used_symbols_sps speed, harder", "[.profiling]") {
    Nft A { create_nfa("((.*){10})*") };
    for (int i = 0; i < 2000000; i++) {
        A.delta.get_used_symbols_sps();
    }
}

/////////////////////////////
/////////////////////////////
