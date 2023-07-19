// TODO: some header

#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include "mata/nfa/nfa.hh"
#include "mata/nfa/builder.hh"

using Symbol = Mata::Symbol;
using IntAlphabet = Mata::IntAlphabet;
using OnTheFlyAlphabet = Mata::OnTheFlyAlphabet;
using StringToSymbolMap = Mata::StringToSymbolMap;

using Word = std::vector<Symbol>;

TEST_CASE("parse_from_mata()") {
    Delta delta;
    delta.add(0, 0, 0);
    delta.add(0, 1, 1);
    delta.add(1, 2, 0);
    Nfa nfa{ delta, { 0 }, { 1 } };

    SECTION("from string") {
        Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa.print_to_mata()) };
        CHECK(are_equivalent(parsed, nfa));
    }

    SECTION("from stream") {
        std::stringstream nfa_stream;
        nfa.print_to_mata(nfa_stream);
        Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa_stream) };
        CHECK(are_equivalent(parsed, nfa));
    }

    SECTION("from file") {
        std::filesystem::path nfa_file{ "./tests/example-automata/nfa/simple.mata" };
        Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa_file) };
        CHECK(are_equivalent(parsed, nfa));
    }
}
