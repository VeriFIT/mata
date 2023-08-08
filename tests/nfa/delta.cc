// TODO: some header

#include "nfa-util.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"

#include "../3rdparty/catch.hpp"

using namespace Mata::Nfa;

using Symbol = Mata::Symbol;

TEST_CASE("Mata::Nfa::SymbolPost") {
    CHECK(SymbolPost{ 0, StateSet{} } == SymbolPost{ 0, StateSet{ 0, 1 } });
    CHECK(SymbolPost{ 1, StateSet{} } != SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } < SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } > SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
}
