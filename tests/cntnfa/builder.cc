// TODO: some header

#include <fstream>
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/builder.hh"

using namespace mata::cntnfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("CntNfa: construct_counter_nfa - Empty automaton") {
    Delta delta;
    Nfa expected{ delta, {}, {} };

    SECTION("from string") {
        std::string input_str = R"(@CNTNFA
            %Initial
            %Final
            %Registers
        )";

        mata::parser::ParsedSection section = mata::parser::parse_mf_section(input_str);
        OnTheFlyAlphabet alphabet;
        Nfa parsed = builder::construct_counter_nfa(section, &alphabet);
        CHECK(parsed.initial.empty());
        CHECK(parsed.final.empty());
        CHECK(parsed.counter_set.size() == 0);
        CHECK(parsed.annotation_collection.size() == 0);
        CHECK(parsed.delta.num_of_transitions() == 0);
    }

    SECTION("from file") {
        std::filesystem::path temp_path = "./temp-test-construct_counter_nfa-empty.mata";
        {
            std::ofstream out(temp_path);
            out << "@CNTNFA\n%Initial\n%Final\n%Registers\n";
        }

        std::ifstream file_stream(temp_path);
        mata::parser::ParsedSection section = mata::parser::parse_mf_section(file_stream);
        file_stream.close();
        std::filesystem::remove(temp_path);

        OnTheFlyAlphabet alphabet;
        Nfa parsed = builder::construct_counter_nfa(section, &alphabet);
        CHECK(parsed.initial.empty());
        CHECK(parsed.final.empty());
        CHECK(parsed.counter_set.size() == 0);
        CHECK(parsed.annotation_collection.size() == 0);
        CHECK(parsed.delta.num_of_transitions() == 0);
    }
}

TEST_CASE("CntNfa: parse_from_mata()") {
    Delta delta;

    SECTION("Empty automaton - No initial and final") {
        Nfa nfa{ delta, {}, {} };
        SECTION("from String") {
            std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n";
            Nfa empty_nfa{ mata::cntnfa::builder::parse_from_mata(empty_nfa_str) };
            CHECK(are_equivalent(empty_nfa, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-empty_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            file << "@NFA-explicit\n%Alphabet-auto\n";
            file.close();
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_file) };
            std::filesystem::remove(nfa_file);
            CHECK(are_equivalent(parsed, nfa));
        }

    }

    SECTION("Empty automaton with empty final and initial") {
        Nfa nfa{ delta, {}, {} };
        SECTION("from String") {
            std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
            Nfa empty_nfa{ mata::cntnfa::builder::parse_from_mata(empty_nfa_str) };
            CHECK(are_equivalent(empty_nfa, nfa));
        }
        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-empty_nfa-empty_initial_final.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            file << "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
            file.close();
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_file) };
            std::filesystem::remove(nfa_file);
            CHECK(are_equivalent(parsed, nfa));
        }
    }

    SECTION("Simple automaton") {
        delta.add(0, 0, 0);
        delta.add(0, 1, 1);
        delta.add(1, 2, 0);
        Nfa nfa{ delta, { 0 }, { 1 } };

        SECTION("from string") {
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa.print_to_mata()) };
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from stream") {
            std::stringstream nfa_stream;
            nfa.print_to_mata(nfa_stream);
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_stream) };
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-simple_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            nfa.print_to_mata(file);
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_file) };
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
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa.print_to_mata()) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from stream") {
            std::stringstream nfa_stream;
            nfa.print_to_mata(nfa_stream);
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_stream) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-larger_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            nfa.print_to_mata(file);
            Nfa parsed{ mata::cntnfa::builder::parse_from_mata(nfa_file) };
            file.close();
            std::filesystem::remove(nfa_file);

            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }
    }
}
