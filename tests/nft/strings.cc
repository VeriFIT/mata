// TODO: some header

#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/nfa/builder.hh"
#include "mata/parser/re2parser.hh"
#include "mata/parser/parser.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/strings.hh"

using namespace mata;
using namespace mata::nft;
using namespace mata::nft::strings;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;
using mata::EnumAlphabet;

TEST_CASE("nft::create_identity()") {
    Nft nft{};
    nft.initial = { 0 };
    nft.final = { 0 };
    SECTION("small identity nft") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        nft.alphabet = &alphabet;
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 0, 2);
        nft.delta.add(2, 0, 0);
        nft.delta.add(0, 1, 3);
        nft.delta.add(3, 1, 4);
        nft.delta.add(4, 1, 0);
        nft.delta.add(0, 2, 5);
        nft.delta.add(5, 2, 6);
        nft.delta.add(6, 2, 0);
        nft.delta.add(0, 3, 7);
        nft.delta.add(7, 3, 8);
        nft.delta.add(8, 3, 0);
        nft.num_of_levels = 3;
        nft.levels.resize(nft.num_of_levels * (alphabet.get_number_of_symbols() - 1));
        nft.levels[0] = 0;
        nft.levels[1] = 1;
        nft.levels[2] = 2;
        nft.levels[3] = 1;
        nft.levels[4] = 2;
        nft.levels[5] = 1;
        nft.levels[6] = 2;
        nft.levels[7] = 1;
        nft.levels[8] = 2;
        Nft nft_identity{ create_identity(&alphabet, 3) };
        CHECK(nft_identity.is_identical(nft));
    }

    SECTION("identity nft no symbols") {
        EnumAlphabet alphabet{};
        nft.alphabet = &alphabet;
        nft.num_of_levels = 3;
        nft.levels.resize(1);
        nft.levels[0] = 0;
        Nft nft_identity{ create_identity(&alphabet, 3) };
        CHECK(nft_identity.is_identical(nft));
    }

    SECTION("identity nft one symbol") {
        EnumAlphabet alphabet{ 0 };
        nft.alphabet = &alphabet;
        nft.num_of_levels = 2;
        nft.levels.resize(2);
        nft.levels[0] = 0;
        nft.levels[1] = 1;
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 0, 0);
        Nft nft_identity{ create_identity(&alphabet, 2) };
        CHECK(nft_identity.is_identical(nft));
        nft_identity = create_identity(&alphabet);
        CHECK(nft_identity.is_identical(nft));
    }

    SECTION("small identity nft one level") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        nft.alphabet = &alphabet;
        nft.delta.add(0, 0, 0);
        nft.delta.add(0, 1, 0);
        nft.delta.add(0, 2, 0);
        nft.delta.add(0, 3, 0);
        nft.num_of_levels = 1;
        nft.levels.resize(1);
        nft.levels[0] = 0;
        Nft nft_identity{ create_identity(&alphabet, 1) };
        CHECK(nft_identity.is_identical(nft));
    }
}

TEST_CASE("nft::create_identity_with_single_symbol_replace()") {
    Nft expected{};
    expected.initial = { 0 };
    expected.final = { 0 };
    SECTION("small identity nft") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        expected.alphabet = &alphabet;
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 0, 0);
        expected.delta.add(0, 1, 2);
        expected.delta.add(2, 3, 0);
        expected.delta.add(0, 2, 3);
        expected.delta.add(3, 2, 0);
        expected.delta.add(0, 3, 4);
        expected.delta.add(4, 3, 0);
        expected.num_of_levels = 2;
        expected.levels.resize(5);
        expected.levels[0] = 0;
        expected.levels[1] = 1;
        expected.levels[2] = 1;
        expected.levels[3] = 1;
        expected.levels[4] = 1;
        Nft nft_identity_with_replace{ create_identity_with_single_symbol_replace(&alphabet, 1, 3) };
        CHECK(nft::are_equivalent(nft_identity_with_replace, expected));
    }

    SECTION("identity nft no symbols") {
        EnumAlphabet alphabet{};
        CHECK_THROWS(create_identity_with_single_symbol_replace(&alphabet, 1, 2));
    }

    SECTION("identity nft one symbol") {
        EnumAlphabet alphabet{ 0 };
        expected.alphabet = &alphabet;
        expected.num_of_levels = 2;
        expected.levels.resize(2);
        expected.levels[0] = 0;
        expected.levels[1] = 1;
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 1, 0);
        Nft nft_identity{ create_identity_with_single_symbol_replace(&alphabet, 0, 1) };
        CHECK(nft::are_equivalent(nft_identity, expected));
    }

    SECTION("small identity expected longer replace") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        expected.alphabet = &alphabet;
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 0, 0);
        expected.delta.add(0, 1, 2);
        expected.delta.add(2, 5, 5);
        expected.delta.add(5, EPSILON, 6);
        expected.delta.add(6, 6, 7);
        expected.delta.add(7, EPSILON, 8);
        expected.delta.add(8, 7, 0);
        expected.delta.add(0, 2, 3);
        expected.delta.add(3, 2, 0);
        expected.delta.add(0, 3, 4);
        expected.delta.add(4, 3, 0);
        expected.num_of_levels = 2;
        expected.levels.resize(9);
        expected.levels[0] = 0;
        expected.levels[1] = 1;
        expected.levels[2] = 1;
        expected.levels[3] = 1;
        expected.levels[4] = 1;
        expected.levels[5] = 0;
        expected.levels[6] = 1;
        expected.levels[7] = 0;
        expected.levels[8] = 1;
        Nft nft_identity_with_replace{ create_identity_with_single_symbol_replace(&alphabet, 1, Word{ 5, 6, 7 }) };
        CHECK(nft::are_equivalent(nft_identity_with_replace, expected));
    }

    SECTION("small identity expected replace symbol with empty string") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        expected.alphabet = &alphabet;
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 0, 0);
        expected.delta.add(0, 1, 2);
        expected.delta.add(2, EPSILON, 0);
        expected.delta.add(0, 2, 3);
        expected.delta.add(3, 2, 0);
        expected.delta.add(0, 3, 4);
        expected.delta.add(4, 3, 0);
        expected.num_of_levels = 2;
        expected.levels.resize(5);
        expected.levels[0] = 0;
        expected.levels[1] = 1;
        expected.levels[2] = 1;
        expected.levels[3] = 1;
        expected.levels[4] = 1;
        Nft nft_identity_with_replace{ create_identity_with_single_symbol_replace(&alphabet, 1, Word{}) };
        CHECK(nft::are_equivalent(nft_identity_with_replace, expected));
    }

    SECTION("identity expected one symbol with word replace") {
        EnumAlphabet alphabet{ 0 };
        expected.alphabet = &alphabet;
        expected.num_of_levels = 2;
        expected.levels.resize(2);
        expected.levels[0] = 0;
        expected.levels[1] = 1;
        expected.delta.add(0, 0, 1);
        expected.delta.add(1, 0, 0);
        Nft nft_identity{ create_identity_with_single_symbol_replace(&alphabet, 0, Word{ 0 }) };
        CHECK(nft::are_equivalent(nft_identity, expected));
    }

    SECTION("small identity expected longer replace single replacement") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0 q9\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:1 q7:0 q8:1 q9:0 q10:1 q11:1 q12:1 q13:1\n%LevelsCnt 2\nq0 0 q1\nq0 1 q2\nq0 2 q3\nq0 3 q4\nq1 0 q0\nq2 5 q5\nq3 2 q0\nq4 3 q0\nq5 4294967295 q6\nq6 6 q7\nq7 4294967295 q8\nq8 7 q9\nq9 0 q10\nq9 1 q11\nq9 2 q12\nq9 3 q13\nq10 0 q9\nq11 1 q9\nq12 2 q9\nq13 3 q9\n"
        ));
        Nft nft_identity_with_replace{ create_identity_with_single_symbol_replace(&alphabet, 1, Word{ 5, 6, 7 }, ReplaceMode::Single) };
        CHECK(nft::are_equivalent(nft_identity_with_replace, expected));
    }

    SECTION("small identity expected replace symbol with empty string single replace") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0 q5\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:1 q7:1 q8:1 q9:1\n%LevelsCnt 2\nq0 0 q1\nq0 1 q2\nq0 2 q3\nq0 3 q4\nq1 0 q0\nq2 4294967295 q5\nq3 2 q0\nq4 3 q0\nq5 0 q6\nq5 1 q7\nq5 2 q8\nq5 3 q9\nq6 0 q5\nq7 1 q5\nq8 2 q5\nq9 3 q5\n"
        ));
        Nft nft_identity_with_replace{ create_identity_with_single_symbol_replace(&alphabet, 1, Word{}, ReplaceMode::Single) };
        CHECK(nft::are_equivalent(nft_identity_with_replace, expected));
    }

    SECTION("identity expected one symbol with word replace single replace") {
        EnumAlphabet alphabet{ 0 };
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0 q2\n%Levels q0:0 q1:1 q2:0 q3:1\n%LevelsCnt 2\nq0 0 q1\nq1 0 q2\nq2 0 q3\nq3 0 q2\n"
        ));
        Nft nft_identity{ create_identity_with_single_symbol_replace(&alphabet, 0, Word{ 0 }, ReplaceMode::Single) };
        CHECK(nft::are_equivalent(nft_identity, expected));
    }
}

TEST_CASE("nft::reluctant_replacement()") {
    Nft nft{};
    nfa::Nfa regex{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };
    constexpr Symbol MARKER{ EPSILON - 100 };
    SECTION("nft::end_marker_dfa()") {
        parser::create_nfa(&regex, "cb+a+");
        nfa::Nfa dfa_end_marker{ nft::strings::end_marker_dfa(regex) };
        nfa::Nfa dfa_expected_end_marker{};
        dfa_expected_end_marker.initial = { 0 };
        dfa_expected_end_marker.final = { 4 };
        dfa_expected_end_marker.delta.add(0, 'c', 1);
        dfa_expected_end_marker.delta.add(1, 'b', 2);
        dfa_expected_end_marker.delta.add(2, 'b', 2);
        dfa_expected_end_marker.delta.add(2, 'a', 3);
        dfa_expected_end_marker.delta.add(3, EPSILON, 4);
        dfa_expected_end_marker.delta.add(4, 'a', 3);
        CHECK(dfa_end_marker.is_deterministic());
        CHECK(nfa::are_equivalent(dfa_end_marker, dfa_expected_end_marker));
        Nft dft_end_marker{ end_marker_dft(dfa_end_marker, MARKER) };
        Nft dft_expected_end_marker{};
        dft_expected_end_marker.num_of_levels = 2;
        dft_expected_end_marker.levels = { 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 };
        dft_expected_end_marker.initial = { 0 };
        dft_expected_end_marker.final = { 9 };
        dft_expected_end_marker.delta.add(0, 'c', 1);
        dft_expected_end_marker.delta.add(1, 'c', 2);
        dft_expected_end_marker.delta.add(2, 'b', 3);
        dft_expected_end_marker.delta.add(3, 'b', 4);
        dft_expected_end_marker.delta.add(4, 'b', 5);
        dft_expected_end_marker.delta.add(5, 'b', 4);
        dft_expected_end_marker.delta.add(4, 'a', 6);
        dft_expected_end_marker.delta.add(6, 'a', 7);
        dft_expected_end_marker.delta.add(7, EPSILON, 8);
        dft_expected_end_marker.delta.add(8, MARKER, 9);
        dft_expected_end_marker.delta.add(9, 'a', 10);
        dft_expected_end_marker.delta.add(10, 'a', 7);
        CHECK(dft_end_marker.is_deterministic());
        CHECK(nft::are_equivalent(dft_end_marker, dft_expected_end_marker));
    }

    SECTION("nft::generic_end_marker_dft() regex cb+a+") {
        nfa::Nfa dfa_generic_end_marker{ generic_marker_dfa("cb+a+", &alphabet) };
        nfa::Nfa dfa_expected{ nfa::Delta{}, { 0 }, { 0, 1, 2, 4 }};
        dfa_expected.delta.add(0, 'a', 0);
        dfa_expected.delta.add(0, 'b', 0);
        dfa_expected.delta.add(0, 'c', 1);
        dfa_expected.delta.add(1, 'a', 0);
        dfa_expected.delta.add(1, 'b', 2);
        dfa_expected.delta.add(1, 'c', 1);
        dfa_expected.delta.add(2, 'a', 3);
        dfa_expected.delta.add(2, 'b', 2);
        dfa_expected.delta.add(2, 'c', 1);
        dfa_expected.delta.add(3, EPSILON, 4);
        dfa_expected.delta.add(4, 'a', 3);
        dfa_expected.delta.add(4, 'b', 0);
        dfa_expected.delta.add(4, 'c', 1);
        CHECK(nfa::are_equivalent(dfa_generic_end_marker, dfa_expected));

        Nft dft_generic_end_marker{ end_marker_dft(dfa_generic_end_marker, MARKER) };
        Nft dft_expected{};
        dft_expected.initial.insert(0);
        dft_expected.final = { 0, 4, 7, 14 };
        dft_expected.num_of_levels = 2;
        dft_expected.delta.add(0, 'a', 1);
        dft_expected.delta.add(1, 'a', 0);
        dft_expected.delta.add(0, 'b', 2);
        dft_expected.delta.add(2, 'b', 0);
        dft_expected.delta.add(0, 'c', 3);
        dft_expected.delta.add(3, 'c', 4);
        dft_expected.delta.add(4, 'a', 5);
        dft_expected.delta.add(5, 'a', 0);
        dft_expected.delta.add(4, 'b', 6);
        dft_expected.delta.add(6, 'b', 7);
        dft_expected.delta.add(4, 'c', 8);
        dft_expected.delta.add(8, 'c', 4);
        dft_expected.delta.add(7, 'a', 9);
        dft_expected.delta.add(9, 'a', 10);
        dft_expected.delta.add(7, 'b', 11);
        dft_expected.delta.add(11, 'b', 7);
        dft_expected.delta.add(7, 'c', 12);
        dft_expected.delta.add(12, 'c', 4);
        dft_expected.delta.add(10, EPSILON, 13);
        dft_expected.delta.add(13, MARKER, 14);
        dft_expected.delta.add(14, 'a', 15);
        dft_expected.delta.add(15, 'a', 10);
        dft_expected.delta.add(14, 'b', 16);
        dft_expected.delta.add(16, 'b', 0);
        dft_expected.delta.add(14, 'c', 17);
        dft_expected.delta.add(17, 'c', 4);
        dft_expected.levels.resize(18);
        dft_expected.levels[0] = 0;
        dft_expected.levels[1] = 1;
        dft_expected.levels[2] = 1;
        dft_expected.levels[3] = 1;
        dft_expected.levels[4] = 0;
        dft_expected.levels[5] = 1;
        dft_expected.levels[6] = 1;
        dft_expected.levels[7] = 0;
        dft_expected.levels[8] = 1;
        dft_expected.levels[9] = 1;
        dft_expected.levels[10] = 0;
        dft_expected.levels[11] = 1;
        dft_expected.levels[12] = 1;
        dft_expected.levels[13] = 1;
        dft_expected.levels[14] = 0;
        dft_expected.levels[15] = 1;
        dft_expected.levels[16] = 1;
        dft_expected.levels[17] = 1;
        CHECK(nft::are_equivalent(dft_generic_end_marker, dft_expected));
    }

    SECTION("nft::generic_end_marker_dft() regex ab+a+") {
        nfa::Nfa dfa_generic_end_marker{ generic_marker_dfa("ab+a+", &alphabet) };
        nfa::Nfa dfa_expected{ nfa::Delta{}, { 0 }, { 0, 1, 2, 4 }};
        dfa_expected.delta.add(0, 'a', 1);
        dfa_expected.delta.add(0, 'b', 0);
        dfa_expected.delta.add(0, 'c', 0);
        dfa_expected.delta.add(1, 'a', 1);
        dfa_expected.delta.add(1, 'b', 2);
        dfa_expected.delta.add(1, 'c', 0);
        dfa_expected.delta.add(2, 'a', 3);
        dfa_expected.delta.add(2, 'b', 2);
        dfa_expected.delta.add(2, 'c', 0);
        dfa_expected.delta.add(3, EPSILON, 4);
        dfa_expected.delta.add(4, 'a', 3);
        dfa_expected.delta.add(4, 'b', 2);
        dfa_expected.delta.add(4, 'c', 0);
        CHECK(nfa::are_equivalent(dfa_generic_end_marker, dfa_expected));

        Nft dft_generic_end_marker{ end_marker_dft(dfa_generic_end_marker, MARKER) };
        Nft dft_expected{};
        dft_expected.initial.insert(0);
        dft_expected.final = { 0, 2, 7, 14 };
        dft_expected.num_of_levels = 2;
        dft_expected.delta.add(0, 'a', 1);
        dft_expected.delta.add(1, 'a', 2);
        dft_expected.delta.add(0, 'b', 3);
        dft_expected.delta.add(3, 'b', 0);
        dft_expected.delta.add(0, 'c', 4);
        dft_expected.delta.add(4, 'c', 0);
        dft_expected.delta.add(2, 'a', 5);
        dft_expected.delta.add(5, 'a', 2);
        dft_expected.delta.add(2, 'b', 6);
        dft_expected.delta.add(6, 'b', 7);
        dft_expected.delta.add(2, 'c', 8);
        dft_expected.delta.add(8, 'c', 0);
        dft_expected.delta.add(7, 'a', 9);
        dft_expected.delta.add(9, 'a', 10);
        dft_expected.delta.add(7, 'b', 11);
        dft_expected.delta.add(11, 'b', 7);
        dft_expected.delta.add(7, 'c', 12);
        dft_expected.delta.add(12, 'c', 0);
        dft_expected.delta.add(10, EPSILON, 13);
        dft_expected.delta.add(13, MARKER, 14);
        dft_expected.delta.add(14, 'a', 15);
        dft_expected.delta.add(15, 'a', 10);
        dft_expected.delta.add(14, 'b', 16);
        dft_expected.delta.add(16, 'b', 7);
        dft_expected.delta.add(14, 'c', 17);
        dft_expected.delta.add(17, 'c', 0);
        dft_expected.levels.resize(18);
        dft_expected.levels[0] = 0;
        dft_expected.levels[1] = 1;
        dft_expected.levels[2] = 0;
        dft_expected.levels[3] = 1;
        dft_expected.levels[4] = 1;
        dft_expected.levels[5] = 1;
        dft_expected.levels[6] = 1;
        dft_expected.levels[7] = 0;
        dft_expected.levels[8] = 1;
        dft_expected.levels[9] = 1;
        dft_expected.levels[10] = 0;
        dft_expected.levels[11] = 1;
        dft_expected.levels[12] = 1;
        dft_expected.levels[13] = 1;
        dft_expected.levels[14] = 0;
        dft_expected.levels[15] = 1;
        dft_expected.levels[16] = 1;
        dft_expected.levels[17] = 1;
        CHECK(nft::are_equivalent(dft_generic_end_marker, dft_expected));
    }

    SECTION("nft::begin_marker_nft() regex a+b+c") {
        nfa::Nfa nfa_begin_marker{ begin_marker_nfa("a+b+c", &alphabet) };
        nfa::Nfa nfa_expected{ nfa::Delta{}, { 0 }, { 0, 1, 2, 4 }};
        nfa_expected.delta.add(0, 'a', 0);
        nfa_expected.delta.add(0, 'b', 0);
        nfa_expected.delta.add(1, 'c', 0);
        nfa_expected.delta.add(0, 'a', 1);
        nfa_expected.delta.add(2, 'b', 1);
        nfa_expected.delta.add(1, 'c', 1);
        nfa_expected.delta.add(3, 'a', 2);
        nfa_expected.delta.add(2, 'b', 2);
        nfa_expected.delta.add(1, 'c', 2);
        nfa_expected.delta.add(4, EPSILON, 3);
        nfa_expected.delta.add(3, 'a', 4);
        nfa_expected.delta.add(0, 'b', 4);
        nfa_expected.delta.add(1, 'c', 4);
        CHECK(nfa::are_equivalent(nfa_begin_marker, nfa_expected));

        Nft nft_begin_marker{ begin_marker_nft(nfa_begin_marker, MARKER) };
        Nft nft_expected{};
        nft_expected.initial.insert(0);
        nft_expected.final.insert(1);
        nft_expected.num_of_levels = 2;
        nft_expected.delta.add(0, EPSILON, 1);
        nft_expected.delta.add(0, EPSILON, 2);
        nft_expected.delta.add(0, EPSILON, 3);
        nft_expected.delta.add(0, EPSILON, 5);
        nft_expected.delta.add(1, 'a', 6);
        nft_expected.delta.add(6, 'a', 1);
        nft_expected.delta.add(6, 'a', 2);
        nft_expected.delta.add(1, 'b', 7);
        nft_expected.delta.add(7, 'b', 1);
        nft_expected.delta.add(7, 'b', 5);
        nft_expected.delta.add(2, 'c', 8);
        nft_expected.delta.add(8, 'c', 2);
        nft_expected.delta.add(8, 'c', 1);
        nft_expected.delta.add(8, 'c', 3);
        nft_expected.delta.add(8, 'c', 5);
        nft_expected.delta.add(3, 'b', 9);
        nft_expected.delta.add(9, 'b', 3);
        nft_expected.delta.add(9, 'b', 2);
        nft_expected.delta.add(4, 'a', 10);
        nft_expected.delta.add(10, 'a', 3);
        nft_expected.delta.add(10, 'a', 5);
        nft_expected.delta.add(5, EPSILON, 11);
        nft_expected.delta.add(11, MARKER, 4);
        nft_expected.levels.resize(12);
        nft_expected.levels[0] = 0;
        nft_expected.levels[1] = 0;
        nft_expected.levels[2] = 0;
        nft_expected.levels[3] = 0;
        nft_expected.levels[4] = 0;
        nft_expected.levels[5] = 0;
        nft_expected.levels[6] = 1;
        nft_expected.levels[7] = 1;
        nft_expected.levels[8] = 1;
        nft_expected.levels[9] = 1;
        nft_expected.levels[10] = 1;
        nft_expected.levels[11] = 1;
        CHECK(nft::are_equivalent(nft_begin_marker, nft_expected));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'b', 'c' }, Word{ MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'b', 'b', 'c', 'c', 'c' }, Word{ MARKER, 'a', 'b', 'b', 'c', 'c', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'c' }, Word{ MARKER, 'a', MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'b', 'c' }, Word{ 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'b', 'b', 'a', 'b', 'c' }, Word{ 'a', 'a', 'b', 'b', 'b', MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'b', 'b', 'c', 'a', 'b', 'c' }, Word{ MARKER, 'a', MARKER, 'a', 'b', 'b', 'b', 'c', MARKER, 'a', 'b', 'c' } }));
    }

    SECTION("nft::begin_marker_nft() regex ab+a+") {
        nfa::Nfa nfa_begin_marker{ begin_marker_nfa("ab+a+", &alphabet) };
        nfa::Nfa nfa_expected{ nfa::Delta{}, { 0 }, { 0, 1, 2, 4 }};
        nfa_expected.delta.add(1, 'a', 0);
        nfa_expected.delta.add(0, 'b', 0);
        nfa_expected.delta.add(0, 'c', 0);
        nfa_expected.delta.add(1, 'a', 1);
        nfa_expected.delta.add(1, 'a', 4);
        nfa_expected.delta.add(2, 'b', 1);
        nfa_expected.delta.add(0, 'c', 1);
        nfa_expected.delta.add(3, 'a', 2);
        nfa_expected.delta.add(2, 'b', 2);
        nfa_expected.delta.add(0, 'c', 2);
        nfa_expected.delta.add(4, EPSILON, 3);
        nfa_expected.delta.add(2, 'b', 4);
        nfa_expected.delta.add(0, 'c', 4);
        CHECK(nfa::are_equivalent(nfa_begin_marker, nfa_expected));

        Nft nft_begin_marker{ begin_marker_nft(nfa_begin_marker, MARKER) };
        Nft nft_expected{ nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q11\n%Final q0\n%Levels q0:0 q1:1 q2:1 q3:0 q4:0 q5:0 q6:1 q7:1 q8:0 q9:1 q10:1 q11:0\n%LevelsCnt 2\nq0 98 q1\nq0 99 q2\nq1 98 q0\nq2 99 q0\nq2 99 q3\nq2 99 q4\nq2 99 q5\nq3 97 q6\nq4 98 q7\nq5 4294967295 q10\nq6 97 q0\nq6 97 q3\nq6 97 q5\nq7 98 q3\nq7 98 q4\nq7 98 q5\nq8 97 q9\nq9 97 q4\nq10 4294967195 q8\nq11 4294967295 q0\nq11 4294967295 q3\nq11 4294967295 q4\nq11 4294967295 q5\n"
        )) };
        CHECK(nft::are_equivalent(nft_begin_marker, nft_expected));
    }
}

TEST_CASE("mata::nft::strings::reluctant_nfa_with_marker()") {
    Nft nft{};
    nfa::Nfa regex{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };
    constexpr Symbol MARKER{ EPSILON - 100 };

    SECTION("regex cb+a+") {
        nfa::Nfa nfa{ [&]() {
            nfa::Nfa nfa{};
            mata::parser::create_nfa(&nfa, "cb+a+");
            return reluctant_nfa_with_marker(nfa, MARKER, &alphabet);
        }() };
        nfa::Nfa expected{ nfa::builder::parse_from_mata(std::string(
            "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q3\nq0 99 q1\nq0 4294967195 q0\nq1 98 q2\nq1 4294967195 q1\nq2 97 q3\nq2 98 q2\nq2 4294967195 q2\n")) };
        CHECK(nfa::are_equivalent(nfa, expected));
    }
}

TEST_CASE("mata::nft::strings::reluctant_leftmost_nft()") {
    Nft nft{};
    Nft expected{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };
    constexpr Symbol MARKER{ EPSILON - 100 };

    SECTION("all 'cb+a+' replaced with 'ddd'") {
        nft = reluctant_leftmost_nft("cb+a+", &alphabet, MARKER, Word{ 'd', 'd', 'd' }, ReplaceMode::All);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q13\n%Final q13\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:0 q6:1 q7:1 q8:0 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:1 q16:1 q17:1 q18:1 q19:0 q20:1 q21:0 q22:1 q23:0 q24:1 q25:0\n%LevelsCnt 2\nq0 99 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 98 q4\nq2 4294967195 q6\nq3 4294967295 q0\nq4 4294967295 q5\nq5 97 q7\nq5 98 q9\nq5 4294967195 q10\nq6 4294967295 q2\nq7 4294967295 q8\nq8 4294967295 q18\nq9 4294967295 q5\nq10 4294967295 q5\nq11 4294967195 q12\nq12 4294967295 q11\nq13 97 q14\nq13 98 q15\nq13 99 q16\nq13 4294967195 q17\nq14 97 q13\nq15 98 q13\nq16 99 q13\nq17 4294967295 q0\nq18 100 q19\nq19 4294967295 q20\nq20 100 q21\nq21 4294967295 q22\nq22 100 q23\nq23 4294967295 q24\nq24 4294967295 q25\nq25 4294967295 q13\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("single 'a+b+c' replaced with '' (empty string)") {
        nft = reluctant_leftmost_nft("a+b+c", &alphabet, MARKER, Word{}, ReplaceMode::Single);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q14\n%Final q20 q14\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:1 q6:0 q7:1 q8:1 q9:1 q10:0 q11:1 q12:0 q13:1 q14:0 q15:1 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:1 q23:1 q24:1\n%LevelsCnt 2\nq0 97 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 97 q4\nq2 98 q5\nq2 4294967195 q7\nq3 4294967295 q0\nq4 4294967295 q2\nq5 4294967295 q6\nq6 98 q8\nq6 99 q9\nq6 4294967195 q11\nq7 4294967295 q2\nq8 4294967295 q6\nq9 4294967295 q10\nq10 4294967295 q19\nq11 4294967295 q6\nq12 4294967195 q13\nq13 4294967295 q12\nq14 97 q15\nq14 98 q16\nq14 99 q17\nq14 4294967195 q18\nq15 97 q14\nq16 98 q14\nq17 99 q14\nq18 4294967295 q0\nq19 4294967295 q20\nq20 97 q21\nq20 98 q22\nq20 99 q23\nq20 4294967195 q24\nq21 97 q20\nq22 98 q20\nq23 99 q20\nq24 4294967295 q20\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
        CHECK(nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'c', 'b', 'a', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'b', MARKER, 'a', 'b', 'c', MARKER, 'a', 'b', 'c', 'b' }, Word{ 'c', 'b', 'a', 'b', 'a', 'b', 'c', 'a', 'b', 'c', 'b' } }));
    }

    SECTION("All 'a+b+c' replaced with 'd'") {
        nft = reluctant_leftmost_nft("a+b+c", &alphabet, MARKER, Word{ 'd' }, ReplaceMode::All);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q14\n%Final q14\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:1 q6:0 q7:1 q8:1 q9:1 q10:0 q11:1 q12:0 q13:1 q14:0 q15:1 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:0\n%LevelsCnt 2\nq0 97 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 97 q4\nq2 98 q5\nq2 4294967195 q7\nq3 4294967295 q0\nq4 4294967295 q2\nq5 4294967295 q6\nq6 98 q8\nq6 99 q9\nq6 4294967195 q11\nq7 4294967295 q2\nq8 4294967295 q6\nq9 4294967295 q10\nq10 4294967295 q19\nq11 4294967295 q6\nq12 4294967195 q13\nq13 4294967295 q12\nq14 97 q15\nq14 98 q16\nq14 99 q17\nq14 4294967195 q18\nq15 97 q14\nq16 98 q14\nq17 99 q14\nq18 4294967295 q0\nq19 100 q20\nq20 4294967295 q21\nq21 4294967295 q22\nq22 4294967295 q14\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
        CHECK(nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'd', 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'd', 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'c', 'b', 'a', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ Word{ MARKER, 'a', MARKER, 'a', MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'b', MARKER, 'a', 'b', 'c', MARKER, 'a', 'b', 'c', 'b' }, Word{ 'd', 'c', 'b', 'a', 'b', 'd', 'd', 'b' } }));
    }
}
