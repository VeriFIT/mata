// TODO: some header

#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/builder.hh"

using namespace mata::cntnfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("CntNfa: construct_counter_nfa()") {
    SECTION("Empty automaton - No initial and final states, no registers") {
        Nfa nfa;

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
        )";

        SECTION("From string") {
            auto section = mata::parser::parse_mf_section(input);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-empty-noinitfinalregs.mata";
            std::ofstream(file_path) << input;
            std::ifstream file(file_path);
            auto section = mata::parser::parse_mf_section(file);
            file.close();
            std::filesystem::remove(file_path);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }
    }

    SECTION("Empty automaton - Empty initial and final states, empty registers") {
        Nfa nfa;

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
            %Initial
            %Final
            %Registers
        )";

        SECTION("From string") {
            auto section = mata::parser::parse_mf_section(input);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-empty-initfinalregs.mata";
            std::ofstream(file_path) << input;
            std::ifstream file(file_path);
            auto section = mata::parser::parse_mf_section(file);
            file.close();
            std::filesystem::remove(file_path);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }
    }

    SECTION("Simple automaton - Empty registers") {
        Nfa nfa;
        nfa.initial = { 0 };
        nfa.delta.add(0, 0, 0);
        nfa.delta.add(0, 1, 1);
        nfa.delta.add(1, 2, 0);
        nfa.final = { 1 };

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
            %Initial q0
            %Final q1
            %Registers
            q0 0 q0
            q0 1 q1
            q1 2 q0
        )";

        SECTION("From string") {
            auto section = mata::parser::parse_mf_section(input);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-simple-emptyregs.mata";
            std::ofstream(file_path) << input;
            std::ifstream file(file_path);
            auto section = mata::parser::parse_mf_section(file);
            file.close();
            std::filesystem::remove(file_path);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }
    }

    SECTION("Larget automaton - Empty registers") {
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

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
            %Initial q1 q2 q50
            %Final q3 q103
            %Registers
            q1 a q2
            q1 a q3
            q1 b q4
            q2 a q2
            q2 b q2
            q2 a q3
            q2 b q4
            q3 b q4
            q3 c q7
            q3 b q2
            q5 c q3
            q7 a q8
            q12 b q15
            q1 b q40
            q51 z q42
        )";

        SECTION("From string") {
            mata::parser::ParsedSection section = mata::parser::parse_mf_section(input);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-larger-emptyregs.mata";
            std::ofstream(file_path) << input;
            std::ifstream file(file_path);
            auto section = mata::parser::parse_mf_section(file);
            file.close();
            std::filesystem::remove(file_path);

            OnTheFlyAlphabet alphabet;
            Nfa parsed = builder::construct_counter_nfa(section, &alphabet);

            CHECK(parsed.initial == nfa.initial);
            CHECK(parsed.final == nfa.final);
            CHECK(parsed.counter_set.size() == nfa.counter_set.size());
            CHECK(parsed.annotation_collection.size() == nfa.annotation_collection.size());
            CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
        }
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
