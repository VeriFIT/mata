// TODO: some header

#include <unordered_set>
#include <fstream>
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/nfa/builder.hh"

using namespace mata::nfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("parse_from_mata()") {
    Delta delta;

    SECTION("Empty automaton - No initial and final") {
        Nfa nfa{ delta, {}, {} };
        SECTION("from String") {
            std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n";
            Nfa empty_nfa{ mata::nfa::builder::parse_from_mata(empty_nfa_str) };
            CHECK(are_equivalent(empty_nfa, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-empty_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            file << "@NFA-explicit\n%Alphabet-auto\n";
            file.close();
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_file) };
            std::filesystem::remove(nfa_file);
            CHECK(are_equivalent(parsed, nfa));
        }

    }

    SECTION("Empty automaton with empty final and initial") {
        Nfa nfa{ delta, {}, {} };
        SECTION("from String") {
            std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
            Nfa empty_nfa{ mata::nfa::builder::parse_from_mata(empty_nfa_str) };
            CHECK(are_equivalent(empty_nfa, nfa));
        }
        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-empty_nfa-empty_initial_final.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            file << "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
            file.close();
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_file) };
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
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa.print_to_mata()) };
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from stream") {
            std::stringstream nfa_stream;
            nfa.print_to_mata(nfa_stream);
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_stream) };
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-simple_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
            nfa.print_to_mata(file);
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_file) };
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
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa.print_to_mata()) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from stream") {
            std::stringstream nfa_stream;
            nfa.print_to_mata(nfa_stream);
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_stream) };
            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }

        SECTION("from file") {
            std::filesystem::path nfa_file{ "./temp-test-parse_from_mata-larger_nfa.mata" };
            std::fstream file{ nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc };
            nfa.print_to_mata(file);
            Nfa parsed{ mata::nfa::builder::parse_from_mata(nfa_file) };
            file.close();
            std::filesystem::remove(nfa_file);

            parsed.final.contains(103);
            parsed.initial.contains(50);
            parsed.delta.contains(51, 'z', 42);
            CHECK(are_equivalent(parsed, nfa));
        }
    }
}

TEST_CASE("Create Tabakov-Vardi NFA") {
    size_t num_of_states;
    size_t alphabet_size;
    double states_trans_ratio_per_symbol;
    double final_state_density;

    SECTION("EMPTY") {
        num_of_states = 0;
        alphabet_size = 0;
        states_trans_ratio_per_symbol = 0;
        final_state_density = 0;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == 0);
        CHECK(nfa.initial.size() == 0);
        CHECK(nfa.final.size() == 0);
        CHECK(nfa.delta.empty());
    }

    SECTION("10-5-0.5-0.5") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = 0.5;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == 5);
        CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
        CHECK(nfa.delta.num_of_transitions() == 25);
    }

    SECTION("Min final") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = 0.0001;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
        CHECK(nfa.delta.num_of_transitions() == 25);
    }

    SECTION("Max final") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = 1;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == num_of_states);
        CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
        CHECK(nfa.delta.num_of_transitions() == 25);
    }

    SECTION("Min transitions") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0;
        final_state_density = 0.5;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == 5);
        CHECK(nfa.delta.get_used_symbols().size() == 0);
        CHECK(nfa.delta.num_of_transitions() == 0);
    }

    SECTION("Max transitions") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 10;
        final_state_density = 0.5;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == 5);
        CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
        CHECK(nfa.delta.num_of_transitions() == 500);
    }

    SECTION("BIG") {
        num_of_states = 200;
        alphabet_size = 100;
        states_trans_ratio_per_symbol = 5;
        final_state_density = 1;

        Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density);
        CHECK(nfa.num_of_states() == num_of_states);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.final.size() == num_of_states);
        CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
        CHECK(nfa.delta.num_of_transitions() == 100000);

    }

    SECTION("Throw runtime_error. transition_density < 0") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = static_cast<double>(-0.1);
        final_state_density = 0.5;

        CHECK_THROWS_AS(mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density), std::runtime_error);
    }

    SECTION("Throw runtime_error. transition_density > num_of_states") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 11;
        final_state_density = 0.5;

        CHECK_THROWS_AS(mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density), std::runtime_error);
    }

    SECTION("Throw runtime_error. final_state_density < 0") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = static_cast<double>(-0.1);

        CHECK_THROWS_AS(mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density), std::runtime_error);
    }

    SECTION("Throw runtime_error. final_state_density > 1") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = static_cast<double>(1.1);

        CHECK_THROWS_AS(mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density), std::runtime_error);
    }

    SECTION("Same seed results in same NFA.") {
        num_of_states = 10;
        alphabet_size = 5;
        states_trans_ratio_per_symbol = 0.5;
        final_state_density = 0.5;
        std::optional<unsigned int> seed1{ 3171643142 };
        std::optional<unsigned int> seed2{ 4283451011 };

        Nfa nfa1_1 = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed1);
        Nfa nfa1_2 = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed1);
        Nfa nfa2 = mata::nfa::builder::create_random_nfa_tabakov_vardi(num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed2);
        CHECK(nfa1_1.is_identical(nfa1_2));
        CHECK(!nfa1_2.is_identical(nfa2));
    }
}
