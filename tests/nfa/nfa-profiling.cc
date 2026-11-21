// TODO: some header

#include "utils.hh"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/parser/re2parser.hh"

using namespace mata::nfa;
using namespace mata::parser;

/////////////////////////////
// Profiling revert and trim
/////////////////////////////

TEST_CASE("mata::nfa::revert() speed, simple ", "[.profiling]") {
	Nfa B;
	FILL_WITH_AUT_B(B);
	for (int i = 0; i < 300'000; i++) { B = revert(B); }
}

TEST_CASE("mata::nfa::revert() speed, harder", "[.profiling]") {
	// this gives an interesting test case if the parser is not trimming and reducing
	Nfa B{mata::parser::create_nfa("((.*){10})*", false, EPSILON, false)};
	for (int i = 0; i < 200; i++) { B = revert(B); }
}

TEST_CASE("mata::nfa::trim_inplace() speed, simple", "[.profiling]") {
	Nfa A, B;
	FILL_WITH_AUT_B(B);
	for (int i = 0; i < 300'000; i++) {
		A = B;
		A.trim();
	}
}

TEST_CASE("mata::nfa::trim_inplace() speed, harder", "[.profiling]") {
	Nfa A;
	// this gives an interesting test case if the parser is not trimming and reducing
	Nfa B{create_nfa("((.*){10})*", false, EPSILON, false)};
	for (int i = 0; i < 200; i++) {
		A = B;
		A.trim();
	}
}

//////////////////////////////
// Profiling get_used_symbols
//////////////////////////////

TEST_CASE("mata::nfa::get_used_symbols speed, harder", "[.profiling]") {
	Nfa A{create_nfa("((.*){10})*")};
	for (int i = 0; i < 2'000'000; i++) { A.delta.get_used_symbols(); }
}

TEST_CASE("mata::nfa::get_used_symbols_bv speed, harder", "[.profiling]") {
	Nfa A{create_nfa("((.*){10})*")};
	for (int i = 0; i < 2'000'000; i++) { A.delta.get_used_symbols_bv(); }
}

TEST_CASE("mata::nfa::get_used_symbols_vec speed, harder", "[.profiling]") {
	Nfa A{create_nfa("((.*){10})*")};
	for (int i = 0; i < 2'000'000; i++) { A.delta.get_used_symbols_vec(); }
}

TEST_CASE("mata::nfa::get_used_symbols_set speed, harder", "[.profiling]") {
	Nfa A{create_nfa("((.*){10})*")};
	for (int i = 0; i < 2'000'000; i++) { A.delta.get_used_symbols_set(); }
}

TEST_CASE("mata::nfa::get_used_symbols_sps speed, harder", "[.profiling]") {
	Nfa A{create_nfa("((.*){10})*")};
	for (int i = 0; i < 2'000'000; i++) { A.delta.get_used_symbols_sps(); }
}

/////////////////////////////
/////////////////////////////
