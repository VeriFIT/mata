// TODO: some header

#include <unordered_set>
#include <fstream>

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

    SECTION("Simple automaton") {
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
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-simple_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            nfa.print_to_mata(file);
            Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa_file) };
            file.close();
            std::filesystem::remove(nfa_file);

            CHECK(are_equivalent(parsed, nfa));
        }

    }

    SECTION("larger automaton") {
        Nfa nfa;
        nfa.initial = { 1, 2, 50 };
        nfa.delta.add(1, 'a', 2);
        nfa.delta.add(1, 'a', 3);
        nfa.delta.add(1, 'b', 4);
        nfa.delta.add(2, 'a', 2);
        nfa.delta.add(2, 'b', 2);
        nfa.delta.add(2, 'a', 3);
        nfa.delta.add(2, 'b', 4);
        nfa.delta.add(3, 'b', 4);
        nfa.delta.add(3, 'c', 7);
        nfa.delta.add(3, 'b', 2);
        nfa.delta.add(5, 'c', 3);
        nfa.delta.add(7, 'a', 8);
        nfa.delta.add(12, 'b', 15);
        nfa.delta.add(1, 'b', 40);
        nfa.delta.add(51, 'z', 42);
        nfa.final = { 3, 103 };

        SECTION("from string") {
            Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa.print_to_mata()) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from stream") {
            std::stringstream nfa_stream;
            nfa.print_to_mata(nfa_stream);
            Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa_stream) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-larger_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            nfa.print_to_mata(file);
            Nfa parsed{ Mata::Nfa::Builder::parse_from_mata(nfa_file) };
            file.close();
            std::filesystem::remove(nfa_file);

            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }
    }
}
