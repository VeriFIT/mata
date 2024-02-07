// TODO: some header

#include <unordered_set>
#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/lvlfa/lvlfa.hh"
#include "mata/lvlfa/builder.hh"

using namespace mata::lvlfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("parse_from_mata()") {
    Delta delta;

    SECTION("Simple automaton") {
        delta.add(0, 0, 0);
        delta.add(0, 1, 1);
        delta.add(1, 2, 0);
        Lvlfa lvlfa{ delta, { 0 }, { 1 }, {}, 0};

        SECTION("from string") {
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from stream") {
            std::stringstream lvlfa_stream;
            lvlfa.print_to_mata(lvlfa_stream);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from file") {
            std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-simple_lvlfa.mata" };
            std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            lvlfa.print_to_mata(file);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
            file.close();
            std::filesystem::remove(lvlfa_file);

            CHECK(are_equivalent(parsed, lvlfa));
        }

    }

    SECTION("larger automaton") {
        Lvlfa lvlfa;
        lvlfa.initial = { 1, 2, 50 };
        lvlfa.delta.add(1, 'a', 2);
        lvlfa.delta.add(1, 'a', 3);
        lvlfa.delta.add(1, 'b', 4);
        lvlfa.delta.add(2, 'a', 2);
        lvlfa.delta.add(2, 'b', 2);
        lvlfa.delta.add(2, 'a', 3);
        lvlfa.delta.add(2, 'b', 4);
        lvlfa.delta.add(3, 'b', 4);
        lvlfa.delta.add(3, 'c', 7);
        lvlfa.delta.add(3, 'b', 2);
        lvlfa.delta.add(5, 'c', 3);
        lvlfa.delta.add(7, 'a', 8);
        lvlfa.delta.add(12, 'b', 15);
        lvlfa.delta.add(1, 'b', 40);
        lvlfa.delta.add(51, 'z', 42);
        lvlfa.final = { 3, 103 };
        // lvlfa.levels = std::vector<Level>(lvlfa.num_of_states(), 0);
        // lvlfa.max_level = 0;

        SECTION("from string") {
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa.print_to_mata()) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from stream") {
            std::stringstream lvlfa_stream;
            lvlfa.print_to_mata(lvlfa_stream);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_stream) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, lvlfa));
        }

        SECTION("from file") {
            std::filesystem::path lvlfa_file{ "./temp-test-parse_from_mata-larger_lvlfa.mata" };
            std::fstream file{ lvlfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            lvlfa.print_to_mata(file);
            Lvlfa parsed{ mata::lvlfa::builder::parse_from_mata(lvlfa_file) };
            file.close();
            std::filesystem::remove(lvlfa_file);

            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, lvlfa));
        }
    }
}
