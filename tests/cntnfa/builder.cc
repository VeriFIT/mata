// TODO: some header

#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/cntnfa/annotations.hh"
#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/types.hh"
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

    SECTION("Simple automaton - With annotations") {
        Nfa nfa;
        nfa.initial = { 0 };
        nfa.final = { 1 };

        // Counters
        size_t c0 = nfa.counter_set.insert("c0");
        size_t c1 = nfa.counter_set.insert("c1");

        // Annotations
        size_t ann0 = nfa.create_annotation_set();
        size_t ann1 = nfa.create_annotation_set();
        nfa.add_annotation(ann0, CounterTest{c0, 0});
        nfa.add_annotation(ann1, CounterIncrement{c1, 1});

        // Transitions
        nfa.delta.add(0, 0, Target{0, UNDEFINED_ANNOTATIONS});
        nfa.delta.add(0, 1, Target{1, ann0});
        nfa.delta.add(1, 2, Target{0, ann1});

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
            %Initial q0
            %Final q1
            %Registers c0 c1
            q0 0 q0
            q0 1 (test c0 0) q1
            q1 2 (increment c1 1) q0
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

            CHECK(parsed.get_annotation_set(0).size() == nfa.get_annotation_set(0).size());
            CHECK(parsed.get_annotation_set(1).size() == nfa.get_annotation_set(1).size());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-simple-withanns.mata";
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

            CHECK(parsed.get_annotation_set(0).size() == nfa.get_annotation_set(0).size());
            CHECK(parsed.get_annotation_set(1).size() == nfa.get_annotation_set(1).size());
        }
    }

    SECTION("Larger automaton - With annotations") {
        Nfa nfa;
        nfa.initial = { 0, 5 };
        nfa.final = { 8, 10 };

        // Counters
        size_t c0 = nfa.counter_set.insert("c0");
        size_t c1 = nfa.counter_set.insert("c1");
        size_t c2 = nfa.counter_set.insert("c2");

        // Annotations
        size_t ann0 = nfa.create_annotation_set();
        size_t ann1 = nfa.create_annotation_set();
        size_t ann2 = nfa.create_annotation_set();
        size_t ann3 = nfa.create_annotation_set();
        size_t ann4 = nfa.create_annotation_set();
        size_t ann5 = nfa.create_annotation_set();
        size_t ann6 = nfa.create_annotation_set();
        size_t ann7 = nfa.create_annotation_set();
        nfa.add_annotation(ann0, CounterTest{ c0, 0 });     // test c0 0
        nfa.add_annotation(ann1, CounterIncrement{ c1, 1 });// increment c1 1
        nfa.add_annotation(ann2, CounterTest{ c1, 1 });     // test c1 1
        nfa.add_annotation(ann2, CounterIncrement{ c2, 2 });// increment c2 2
        nfa.add_annotation(ann3, CounterIncrement{ c1, 1 });// increment c1 1
        nfa.add_annotation(ann4, CounterIncrement{ c0, 3 });// increment c0 3
        nfa.add_annotation(ann5, CounterTest{ c1, 1 });     // test c1 1
        nfa.add_annotation(ann5, CounterIncrement{ c2, 2 });// increment c2 2
        nfa.add_annotation(ann6, CounterTest{ c0, 0 });     // test c0 0
        nfa.add_annotation(ann7, CounterTest{ c1, 1 });     // test c1 1
        nfa.add_annotation(ann7, CounterIncrement{ c2, 2 });// increment c2 2

        // Transitions
        nfa.delta.add(0, 'a', Target{1, ann0});
        nfa.delta.add(1, 'b', Target{2, ann1});
        nfa.delta.add(2, 'c', Target{3, ann2});
        nfa.delta.add(3, 'd', Target{4, UNDEFINED_ANNOTATIONS});
        nfa.delta.add(4, 'e', Target{5, ann3});
        nfa.delta.add(5, 'f', Target{6, ann4});
        nfa.delta.add(6, 'g', Target{7, ann5});
        nfa.delta.add(7, 'h', Target{8, UNDEFINED_ANNOTATIONS});
        nfa.delta.add(8, 'i', Target{9, ann6});
        nfa.delta.add(9, 'j', Target{10, ann7});

        std::string input = R"(@CNTNFA-explicit
            %Alphabet-auto
            %Initial q0 q5
            %Final q8 q10
            %Registers c0 c1 c2
            q0 a (test c0 0) q1
            q1 b (increment c1 1) q2
            q2 c (test c1 1) (increment c2 2) q3
            q3 d q4
            q4 e (increment c1 1) q5
            q5 f (increment c0 3) q6
            q6 g (test c1 1) (increment c2 2) q7
            q7 h q8
            q8 i (test c0 0) q9
            q9 j (test c1 1) (increment c2 2) q10
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

            CHECK(parsed.get_annotation_set(0).size() == nfa.get_annotation_set(0).size());
            CHECK(parsed.get_annotation_set(1).size() == nfa.get_annotation_set(1).size());
        }

        SECTION("From file") {
            std::filesystem::path file_path = "temp-simple-withanns.mata";
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

            CHECK(parsed.get_annotation_set(0).size() == nfa.get_annotation_set(0).size());
            CHECK(parsed.get_annotation_set(1).size() == nfa.get_annotation_set(1).size());
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
