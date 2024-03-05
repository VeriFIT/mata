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

class ReluctantReplaceSUT: public nft::strings::ReluctantReplace {
    using super = nft::strings::ReluctantReplace;
public:
    using super::reluctant_nfa_with_marker, super::replace_literal_nft, super::generic_marker_dfa, super::end_marker_dfa,
          super::marker_nft, super::reluctant_leftmost_nft, super::begin_marker_nfa, super::begin_marker_nft,
          super::end_marker_dft;
};

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
    ReluctantReplaceSUT reluctant_replace{};
    SECTION("nft::end_marker_dfa()") {
        parser::create_nfa(&regex, "cb+a+");
        nfa::Nfa dfa_end_marker{ reluctant_replace.end_marker_dfa(regex) };
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
        Nft dft_end_marker{ reluctant_replace.end_marker_dft(dfa_end_marker, END_MARKER) };
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
        dft_expected_end_marker.delta.add(8, END_MARKER, 9);
        dft_expected_end_marker.delta.add(9, 'a', 10);
        dft_expected_end_marker.delta.add(10, 'a', 7);
        CHECK(dft_end_marker.is_deterministic());
        CHECK(nft::are_equivalent(dft_end_marker, dft_expected_end_marker));
    }

    SECTION("nft::generic_end_marker_dft() regex cb+a+") {
        nfa::Nfa dfa_generic_end_marker{ reluctant_replace.generic_marker_dfa("cb+a+", &alphabet) };
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

        Nft dft_generic_end_marker{ reluctant_replace.end_marker_dft(dfa_generic_end_marker, END_MARKER) };
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
        dft_expected.delta.add(13, END_MARKER, 14);
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
        nfa::Nfa dfa_generic_end_marker{ reluctant_replace.generic_marker_dfa("ab+a+", &alphabet) };
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

        Nft dft_generic_end_marker{ reluctant_replace.end_marker_dft(dfa_generic_end_marker, END_MARKER) };
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
        dft_expected.delta.add(13, END_MARKER, 14);
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
        nfa::Nfa nfa_begin_marker{ reluctant_replace.begin_marker_nfa("a+b+c", &alphabet) };
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

        Nft nft_begin_marker{ reluctant_replace.begin_marker_nft(nfa_begin_marker, BEGIN_MARKER) };
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
        nft_expected.delta.add(11, BEGIN_MARKER, 4);
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
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'b', 'c' }, Word{ BEGIN_MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'b', 'b', 'c', 'c', 'c' }, Word{ BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'c' }, Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'b', 'c' }, Word{ 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'b', 'b', 'a', 'b', 'c' }, Word{ 'a', 'a', 'b', 'b', 'b', BEGIN_MARKER, 'a', 'b', 'c' } }));
        CHECK(nft_begin_marker.is_tuple_in_lang({ Word{ 'a', 'a', 'b', 'b', 'b', 'c', 'a', 'b', 'c' }, Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'b', 'c', BEGIN_MARKER, 'a', 'b', 'c' } }));
    }

    SECTION("nft::begin_marker_nft() regex ab+a+") {
        nfa::Nfa nfa_begin_marker{ reluctant_replace.begin_marker_nfa("ab+a+", &alphabet) };
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

        Nft nft_begin_marker{ reluctant_replace.begin_marker_nft(nfa_begin_marker, BEGIN_MARKER) };
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
    ReluctantReplaceSUT reluctant_replace{};

    SECTION("regex cb+a+") {
        nfa::Nfa nfa{ [&]() {
            nfa::Nfa nfa{};
            mata::parser::create_nfa(&nfa, "cb+a+");
            return reluctant_replace.reluctant_nfa_with_marker(nfa, BEGIN_MARKER, &alphabet);
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
    ReluctantReplaceSUT reluctant_replace{};

    SECTION("all 'cb+a+' replaced with 'ddd'") {
        nft = reluctant_replace.reluctant_leftmost_nft("cb+a+", &alphabet, BEGIN_MARKER, Word{ 'd', 'd', 'd' }, ReplaceMode::All);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q13\n%Final q13\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:0 q6:1 q7:1 q8:0 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:1 q16:1 q17:1 q18:1 q19:0 q20:1 q21:0 q22:1 q23:0 q24:1 q25:0\n%LevelsCnt 2\nq0 99 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 98 q4\nq2 4294967195 q6\nq3 4294967295 q0\nq4 4294967295 q5\nq5 97 q7\nq5 98 q9\nq5 4294967195 q10\nq6 4294967295 q2\nq7 4294967295 q8\nq8 4294967295 q18\nq9 4294967295 q5\nq10 4294967295 q5\nq11 4294967195 q12\nq12 4294967295 q11\nq13 97 q14\nq13 98 q15\nq13 99 q16\nq13 4294967195 q17\nq14 97 q13\nq15 98 q13\nq16 99 q13\nq17 4294967295 q0\nq18 100 q19\nq19 4294967295 q20\nq20 100 q21\nq21 4294967295 q22\nq22 100 q23\nq23 4294967295 q24\nq24 4294967295 q25\nq25 4294967295 q13\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("single 'a+b+c' replaced with '' (empty string)") {
        nft = reluctant_replace.reluctant_leftmost_nft("a+b+c", &alphabet, BEGIN_MARKER, Word{}, ReplaceMode::Single);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q14\n%Final q20 q14\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:1 q6:0 q7:1 q8:1 q9:1 q10:0 q11:1 q12:0 q13:1 q14:0 q15:1 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:1 q23:1 q24:1\n%LevelsCnt 2\nq0 97 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 97 q4\nq2 98 q5\nq2 4294967195 q7\nq3 4294967295 q0\nq4 4294967295 q2\nq5 4294967295 q6\nq6 98 q8\nq6 99 q9\nq6 4294967195 q11\nq7 4294967295 q2\nq8 4294967295 q6\nq9 4294967295 q10\nq10 4294967295 q19\nq11 4294967295 q6\nq12 4294967195 q13\nq13 4294967295 q12\nq14 97 q15\nq14 98 q16\nq14 99 q17\nq14 4294967195 q18\nq15 97 q14\nq16 98 q14\nq17 99 q14\nq18 4294967295 q0\nq19 4294967295 q20\nq20 97 q21\nq20 98 q22\nq20 99 q23\nq20 4294967195 q24\nq21 97 q20\nq22 98 q20\nq23 99 q20\nq24 4294967295 q20\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
        CHECK(nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'c', 'b', 'a', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'b', BEGIN_MARKER, 'a', 'b', 'c', BEGIN_MARKER, 'a', 'b', 'c', 'b' }, Word{ 'c', 'b', 'a', 'b', 'a', 'b', 'c', 'a', 'b', 'c', 'b' } }));
    }

    SECTION("All 'a+b+c' replaced with 'd'") {
        nft = reluctant_replace.reluctant_leftmost_nft("a+b+c", &alphabet, BEGIN_MARKER, Word{ 'd' }, ReplaceMode::All);
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q14\n%Final q14\n%Levels q0:0 q1:1 q2:0 q3:1 q4:1 q5:1 q6:0 q7:1 q8:1 q9:1 q10:0 q11:1 q12:0 q13:1 q14:0 q15:1 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:0\n%LevelsCnt 2\nq0 97 q1\nq0 4294967195 q3\nq1 4294967295 q2\nq2 97 q4\nq2 98 q5\nq2 4294967195 q7\nq3 4294967295 q0\nq4 4294967295 q2\nq5 4294967295 q6\nq6 98 q8\nq6 99 q9\nq6 4294967195 q11\nq7 4294967295 q2\nq8 4294967295 q6\nq9 4294967295 q10\nq10 4294967295 q19\nq11 4294967295 q6\nq12 4294967195 q13\nq13 4294967295 q12\nq14 97 q15\nq14 98 q16\nq14 99 q17\nq14 4294967195 q18\nq15 97 q14\nq16 98 q14\nq17 99 q14\nq18 4294967295 q0\nq19 100 q20\nq20 4294967295 q21\nq21 4294967295 q22\nq22 4294967295 q14\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
        CHECK(nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'd', 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'd', 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'b', 'a', 'c' } }));
        CHECK(!nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'c' }, Word{ 'c', 'c', 'b', 'a', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ Word{ BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', BEGIN_MARKER, 'a', 'b', 'b', 'c', 'c', 'b', 'a', 'b', BEGIN_MARKER, 'a', 'b', 'c', BEGIN_MARKER, 'a', 'b', 'c', 'b' }, Word{ 'd', 'c', 'b', 'a', 'b', 'd', 'd', 'b' } }));
    }
}

TEST_CASE("mata::nft::strings::literal_replace_nft()") {
    Nft nft{};
    Nft expected{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };
    ReluctantReplaceSUT reluctant_replace{};

    SECTION("'abcc' replace with 'a' replace all") {
        nft = reluctant_replace.replace_literal_nft(Word{ 'a', 'b', 'c', 'c' }, Word{ 'a' },  &alphabet, END_MARKER, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c', 'a', END_MARKER },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'a', 'a', 'a' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q35\n%Levels q0:0 q1:0 q2:0 q3:0 q4:0 q5:1 q6:1 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:1 q14:0 q15:1 q16:1 q17:0 q18:1 q19:0 q20:1 q21:1 q22:1 q23:0 q24:1 q25:0 q26:1 q27:1 q28:0 q29:1 q30:0 q31:1 q32:0 q33:1 q34:1 q35:0 q36:1 q37:1 q38:1 q39:1 q40:0 q41:1 q42:1 q43:0 q44:1 q45:0 q46:1\n%LevelsCnt 2\nq0 97 q5\nq0 98 q6\nq0 99 q7\nq0 4294967196 q37\nq1 97 q8\nq1 98 q9\nq1 99 q10\nq1 4294967196 q38\nq2 97 q13\nq2 98 q16\nq2 99 q21\nq2 4294967196 q39\nq3 97 q22\nq3 98 q27\nq3 99 q34\nq3 4294967196 q42\nq4 4294967295 q36\nq5 4294967295 q1\nq6 98 q0\nq7 99 q0\nq8 97 q1\nq9 4294967295 q2\nq10 97 q11\nq11 4294967295 q12\nq12 99 q0\nq13 97 q14\nq14 4294967295 q15\nq15 98 q1\nq16 97 q17\nq17 4294967295 q18\nq18 98 q19\nq19 4294967295 q20\nq20 98 q0\nq21 4294967295 q3\nq22 97 q23\nq23 4294967295 q24\nq24 98 q25\nq25 4294967295 q26\nq26 99 q1\nq27 97 q28\nq28 4294967295 q29\nq29 98 q30\nq30 4294967295 q31\nq31 99 q32\nq32 4294967295 q33\nq33 98 q0\nq34 4294967295 q4\nq36 97 q0\nq37 4294967295 q35\nq38 97 q35\nq39 97 q40\nq40 4294967295 q41\nq41 98 q35\nq42 97 q43\nq43 4294967295 q44\nq44 98 q45\nq45 4294967295 q46\nq46 99 q35\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("'abcc' replace with 'bbb' replace single") {
        nft = reluctant_replace.replace_literal_nft(Word{ 'a', 'b', 'c', 'c' }, Word{ 'b', 'b', 'b' },  &alphabet, END_MARKER, ReplaceMode::Single);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c', END_MARKER },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'b', 'b', 'b', 'a', 'a', 'b', 'c', 'c' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q45 q46\n%Levels q0:0 q1:0 q2:0 q3:0 q4:0 q5:1 q6:1 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:1 q14:0 q15:1 q16:1 q17:0 q18:1 q19:0 q20:1 q21:1 q22:1 q23:0 q24:1 q25:0 q26:1 q27:1 q28:0 q29:1 q30:0 q31:1 q32:0 q33:1 q34:1 q35:1 q36:0 q37:1 q38:0 q39:1 q40:0 q41:1 q42:1 q43:1 q44:1 q45:0 q46:0 q47:1 q48:1 q49:1 q50:0 q51:1 q52:1 q53:0 q54:1 q55:0 q56:1\n%LevelsCnt 2\nq0 97 q5\nq0 98 q6\nq0 99 q7\nq0 4294967196 q47\nq1 97 q8\nq1 98 q9\nq1 99 q10\nq1 4294967196 q48\nq2 97 q13\nq2 98 q16\nq2 99 q21\nq2 4294967196 q49\nq3 97 q22\nq3 98 q27\nq3 99 q34\nq3 4294967196 q52\nq4 4294967295 q35\nq5 4294967295 q1\nq6 98 q0\nq7 99 q0\nq8 97 q1\nq9 4294967295 q2\nq10 97 q11\nq11 4294967295 q12\nq12 99 q0\nq13 97 q14\nq14 4294967295 q15\nq15 98 q1\nq16 97 q17\nq17 4294967295 q18\nq18 98 q19\nq19 4294967295 q20\nq20 98 q0\nq21 4294967295 q3\nq22 97 q23\nq23 4294967295 q24\nq24 98 q25\nq25 4294967295 q26\nq26 99 q1\nq27 97 q28\nq28 4294967295 q29\nq29 98 q30\nq30 4294967295 q31\nq31 99 q32\nq32 4294967295 q33\nq33 98 q0\nq34 4294967295 q4\nq35 98 q36\nq36 4294967295 q37\nq37 98 q38\nq38 4294967295 q39\nq39 98 q40\nq40 97 q41\nq40 98 q42\nq40 99 q43\nq40 4294967196 q44\nq41 97 q40\nq42 98 q40\nq43 99 q40\nq44 4294967295 q45\nq47 4294967295 q46\nq48 97 q46\nq49 97 q50\nq50 4294967295 q51\nq51 98 q46\nq52 97 q53\nq53 4294967295 q54\nq54 98 q55\nq55 4294967295 q56\nq56 99 q46\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("'aabac' replace with 'd' replace all") {
        nft = reluctant_replace.replace_literal_nft(Word{ 'a', 'a', 'b', 'a', 'c' }, Word{ 'd' }, &alphabet, END_MARKER,
                                  ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a', END_MARKER },
                                     { 'a', 'a', 'a', 'b', 'a', 'd', 'a' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q54\n%Levels q0:0 q1:0 q2:0 q3:0 q4:0 q5:0 q6:1 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:1 q14:0 q15:1 q16:1 q17:1 q18:1 q19:0 q20:1 q21:0 q22:1 q23:1 q24:1 q25:0 q26:1 q27:0 q28:1 q29:0 q30:1 q31:1 q32:0 q33:1 q34:0 q35:1 q36:0 q37:1 q38:1 q39:0 q40:1 q41:0 q42:1 q43:1 q44:0 q45:1 q46:0 q47:1 q48:0 q49:1 q50:0 q51:1 q52:1 q53:1 q54:0 q55:1 q56:1 q57:1 q58:0 q59:1 q60:1 q61:0 q62:1 q63:0 q64:1 q65:1 q66:0 q67:1 q68:0 q69:1 q70:0 q71:1\n%LevelsCnt 2\nq0 97 q6\nq0 98 q7\nq0 99 q8\nq0 4294967196 q55\nq1 97 q9\nq1 98 q10\nq1 99 q13\nq1 4294967196 q56\nq2 97 q16\nq2 98 q17\nq2 99 q18\nq2 4294967196 q57\nq3 97 q23\nq3 98 q24\nq3 99 q31\nq3 4294967196 q60\nq4 97 q38\nq4 98 q43\nq4 99 q52\nq4 4294967196 q65\nq5 4294967295 q53\nq6 4294967295 q1\nq7 98 q0\nq8 99 q0\nq9 4294967295 q2\nq10 97 q11\nq11 4294967295 q12\nq12 98 q0\nq13 97 q14\nq14 4294967295 q15\nq15 99 q0\nq16 97 q2\nq17 4294967295 q3\nq18 97 q19\nq19 4294967295 q20\nq20 97 q21\nq21 4294967295 q22\nq22 99 q0\nq23 4294967295 q4\nq24 97 q25\nq25 4294967295 q26\nq26 97 q27\nq27 4294967295 q28\nq28 98 q29\nq29 4294967295 q30\nq30 98 q0\nq31 97 q32\nq32 4294967295 q33\nq33 97 q34\nq34 4294967295 q35\nq35 98 q36\nq36 4294967295 q37\nq37 99 q0\nq38 97 q39\nq39 4294967295 q40\nq40 97 q41\nq41 4294967295 q42\nq42 98 q2\nq43 97 q44\nq44 4294967295 q45\nq45 97 q46\nq46 4294967295 q47\nq47 98 q48\nq48 4294967295 q49\nq49 97 q50\nq50 4294967295 q51\nq51 98 q0\nq52 4294967295 q5\nq53 100 q0\nq55 4294967295 q54\nq56 97 q54\nq57 97 q58\nq58 4294967295 q59\nq59 97 q54\nq60 97 q61\nq61 4294967295 q62\nq62 97 q63\nq63 4294967295 q64\nq64 98 q54\nq65 97 q66\nq66 4294967295 q67\nq67 97 q68\nq68 4294967295 q69\nq69 98 q70\nq70 4294967295 q71\nq71 97 q54\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }
}

TEST_CASE("mata::nft::strings::replace_reluctant_literal()") {
    Nft nft{};
    Nft expected{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };

    SECTION("'abcc' replace with 'a' replace all") {
        nft = nft::strings::replace_reluctant_literal(Word{ 'a', 'b', 'c', 'c' }, Word{ 'a' },  &alphabet, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'b', 'c', 'c' },
                                   { 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'b', 'c', 'c' },
                                   { 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'c', 'a', 'b', 'c', 'c' },
                                   { 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'c', 'a', 'b', 'c', 'c', 'a' },
                                   { 'c', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'c', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c' },
                                   { 'c', 'a', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'c', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c' },
                                   { 'a', 'c', 'a', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c', 'a' },
                                   { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'a', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { }, { } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'b', 'c' },
                                   { 'a', 'b', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'b', 'c' },
                                   { 'a', 'b', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ { 'c', 'c', 'c' },
                                   { 'c', 'c', 'c' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q35\n%Levels q0:0 q1:0 q2:0 q3:0 q4:0 q5:1 q6:1 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:1 q14:0 q15:1 q16:1 q17:0 q18:1 q19:0 q20:1 q21:1 q22:1 q23:0 q24:1 q25:0 q26:1 q27:1 q28:0 q29:1 q30:0 q31:1 q32:0 q33:1 q34:1 q35:0 q36:1 q37:1 q38:1 q39:1 q40:0 q41:1 q42:1 q43:0 q44:1 q45:0 q46:1\n%LevelsCnt 2\nq0 97 q5\nq0 98 q6\nq0 99 q7\nq0 4294967295 q37\nq1 97 q8\nq1 98 q9\nq1 99 q10\nq1 4294967295 q38\nq2 97 q13\nq2 98 q16\nq2 99 q21\nq2 4294967295 q39\nq3 97 q22\nq3 98 q27\nq3 99 q34\nq3 4294967295 q42\nq4 4294967295 q36\nq5 4294967295 q1\nq6 98 q0\nq7 99 q0\nq8 97 q1\nq9 4294967295 q2\nq10 97 q11\nq11 4294967295 q12\nq12 99 q0\nq13 97 q14\nq14 4294967295 q15\nq15 98 q1\nq16 97 q17\nq17 4294967295 q18\nq18 98 q19\nq19 4294967295 q20\nq20 98 q0\nq21 4294967295 q3\nq22 97 q23\nq23 4294967295 q24\nq24 98 q25\nq25 4294967295 q26\nq26 99 q1\nq27 97 q28\nq28 4294967295 q29\nq29 98 q30\nq30 4294967295 q31\nq31 99 q32\nq32 4294967295 q33\nq33 98 q0\nq34 4294967295 q4\nq36 97 q0\nq37 4294967295 q35\nq38 97 q35\nq39 97 q40\nq40 4294967295 q41\nq41 98 q35\nq42 97 q43\nq43 4294967295 q44\nq44 98 q45\nq45 4294967295 q46\nq46 99 q35\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

   SECTION("'abcc' replace with 'dd' replace single") {
       nft = nft::strings::replace_reluctant_literal(Word{ 'a', 'b', 'c', 'c' }, Word{ 'd', 'd' },  &alphabet, ReplaceMode::Single);
       CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c', 'c', 'a', 'a', 'b', 'c', 'c', 'a' },
                                  { 'a', 'a', 'a', 'b', 'a', 'a', 'd', 'd', 'a', 'a', 'b', 'c', 'c', 'a' } }));
       CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c' },
                                  { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'c' } }));
       expected = nft::builder::parse_from_mata(std::string(
         "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q5\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:0 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:1 q16:1 q17:1 q18:0 q19:1 q20:0 q21:1 q22:1 q23:1 q24:1 q25:0 q26:1 q27:0 q28:1 q29:0 q30:1 q31:0 q32:1 q33:0 q34:1 q35:1 q36:1 q37:1 q38:0 q39:1 q40:0 q41:1 q42:0 q43:1 q44:0 q45:1 q46:0 q47:1 q48:0 q49:1 q50:0 q51:1 q52:0 q53:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq0 4294967295 q4\nq1 4294967295 q6\nq2 98 q0\nq3 99 q0\nq4 4294967295 q5\nq6 97 q7\nq6 98 q8\nq6 99 q9\nq6 4294967295 q10\nq7 97 q6\nq8 4294967295 q13\nq9 97 q11\nq10 97 q5\nq11 4294967295 q12\nq12 99 q0\nq13 97 q14\nq13 98 q15\nq13 99 q16\nq13 4294967295 q17\nq14 97 q52\nq15 97 q48\nq16 4294967295 q20\nq17 97 q18\nq18 4294967295 q19\nq19 98 q5\nq20 97 q21\nq20 98 q22\nq20 99 q23\nq20 4294967295 q24\nq21 97 q44\nq22 97 q38\nq23 4294967295 q29\nq24 97 q25\nq25 4294967295 q26\nq26 98 q27\nq27 4294967295 q28\nq28 99 q5\nq29 4294967295 q30\nq30 100 q31\nq31 4294967295 q32\nq32 100 q33\nq33 97 q34\nq33 98 q35\nq33 99 q36\nq33 4294967295 q37\nq34 97 q33\nq35 98 q33\nq36 99 q33\nq37 4294967295 q5\nq38 4294967295 q39\nq39 98 q40\nq40 4294967295 q41\nq41 99 q42\nq42 4294967295 q43\nq43 98 q0\nq44 4294967295 q45\nq45 98 q46\nq46 4294967295 q47\nq47 99 q6\nq48 4294967295 q49\nq49 98 q50\nq50 4294967295 q51\nq51 98 q0\nq52 4294967295 q53\nq53 98 q6\n"
       ));
       CHECK(nft::are_equivalent(nft, expected));
   }

   SECTION("'aabac' replace with 'd' replace all") {
       nft = nft::strings::replace_reluctant_literal(Word{ 'a', 'a', 'b', 'a', 'c' }, Word{ 'd' }, &alphabet,
                                                   ReplaceMode::All);
       CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                  { 'a', 'a', 'a', 'b', 'a', 'd', 'a' } }));
       CHECK(nft.is_tuple_in_lang({ { }, { } }));
       CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                  { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' } }));
       expected = nft::builder::parse_from_mata(std::string(
          "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q5\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:0 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:0 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:0 q23:1 q24:0 q25:1 q26:0 q27:1 q28:1 q29:1 q30:1 q31:0 q32:1 q33:0 q34:1 q35:0 q36:1 q37:0 q38:1 q39:0 q40:1 q41:0 q42:1 q43:0 q44:1 q45:0 q46:1 q47:0 q48:1 q49:1 q50:1 q51:1 q52:0 q53:1 q54:0 q55:1 q56:0 q57:1 q58:0 q59:1 q60:0 q61:1 q62:0 q63:1 q64:0 q65:1 q66:0 q67:1 q68:0 q69:1 q70:0 q71:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq0 4294967295 q4\nq1 4294967295 q6\nq2 98 q0\nq3 99 q0\nq4 4294967295 q5\nq6 97 q7\nq6 98 q8\nq6 99 q9\nq6 4294967295 q10\nq7 4294967295 q15\nq8 97 q13\nq9 97 q11\nq10 97 q5\nq11 4294967295 q12\nq12 99 q0\nq13 4294967295 q14\nq14 98 q0\nq15 97 q16\nq15 98 q17\nq15 99 q18\nq15 4294967295 q19\nq16 97 q15\nq17 4294967295 q26\nq18 97 q22\nq19 97 q20\nq20 4294967295 q21\nq21 97 q5\nq22 4294967295 q23\nq23 97 q24\nq24 4294967295 q25\nq25 99 q0\nq26 97 q27\nq26 98 q28\nq26 99 q29\nq26 4294967295 q30\nq27 4294967295 q47\nq28 97 q41\nq29 97 q35\nq30 97 q31\nq31 4294967295 q32\nq32 97 q33\nq33 4294967295 q34\nq34 98 q5\nq35 4294967295 q36\nq36 97 q37\nq37 4294967295 q38\nq38 98 q39\nq39 4294967295 q40\nq40 99 q0\nq41 4294967295 q42\nq42 97 q43\nq43 4294967295 q44\nq44 98 q45\nq45 4294967295 q46\nq46 98 q0\nq47 97 q48\nq47 98 q49\nq47 99 q50\nq47 4294967295 q51\nq48 97 q68\nq49 97 q60\nq50 4294967295 q58\nq51 97 q52\nq52 4294967295 q53\nq53 97 q54\nq54 4294967295 q55\nq55 98 q56\nq56 4294967295 q57\nq57 97 q5\nq58 4294967295 q59\nq59 100 q0\nq60 4294967295 q61\nq61 97 q62\nq62 4294967295 q63\nq63 98 q64\nq64 4294967295 q65\nq65 97 q66\nq66 4294967295 q67\nq67 98 q0\nq68 4294967295 q69\nq69 97 q70\nq70 4294967295 q71\nq71 98 q15\n"
       ));
       CHECK(nft::are_equivalent(nft, expected));
   }

    SECTION("drop all 'aabac'") {
        nft = nft::strings::replace_reluctant_literal(Word{ 'a', 'a', 'b', 'a', 'c' }, Word{}, &alphabet,
                                                      ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'a', 'a', 'b', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a', 'a', 'b', 'a', 'c' },
                                     { 'a', 'a', 'a', 'b', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { }, { } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q5\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:0 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:0 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:0 q23:1 q24:0 q25:1 q26:0 q27:1 q28:1 q29:1 q30:1 q31:0 q32:1 q33:0 q34:1 q35:0 q36:1 q37:0 q38:1 q39:0 q40:1 q41:0 q42:1 q43:0 q44:1 q45:0 q46:1 q47:0 q48:1 q49:1 q50:1 q51:1 q52:0 q53:1 q54:0 q55:1 q56:0 q57:1 q58:0 q59:1 q60:0 q61:1 q62:0 q63:1 q64:0 q65:1 q66:0 q67:1 q68:0 q69:1 q70:0 q71:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq0 4294967295 q4\nq1 4294967295 q6\nq2 98 q0\nq3 99 q0\nq4 4294967295 q5\nq6 97 q7\nq6 98 q8\nq6 99 q9\nq6 4294967295 q10\nq7 4294967295 q15\nq8 97 q13\nq9 97 q11\nq10 97 q5\nq11 4294967295 q12\nq12 99 q0\nq13 4294967295 q14\nq14 98 q0\nq15 97 q16\nq15 98 q17\nq15 99 q18\nq15 4294967295 q19\nq16 97 q15\nq17 4294967295 q26\nq18 97 q22\nq19 97 q20\nq20 4294967295 q21\nq21 97 q5\nq22 4294967295 q23\nq23 97 q24\nq24 4294967295 q25\nq25 99 q0\nq26 97 q27\nq26 98 q28\nq26 99 q29\nq26 4294967295 q30\nq27 4294967295 q47\nq28 97 q41\nq29 97 q35\nq30 97 q31\nq31 4294967295 q32\nq32 97 q33\nq33 4294967295 q34\nq34 98 q5\nq35 4294967295 q36\nq36 97 q37\nq37 4294967295 q38\nq38 98 q39\nq39 4294967295 q40\nq40 99 q0\nq41 4294967295 q42\nq42 97 q43\nq43 4294967295 q44\nq44 98 q45\nq45 4294967295 q46\nq46 98 q0\nq47 97 q48\nq47 98 q49\nq47 99 q50\nq47 4294967295 q51\nq48 97 q68\nq49 97 q60\nq50 4294967295 q58\nq51 97 q52\nq52 4294967295 q53\nq53 97 q54\nq54 4294967295 q55\nq55 98 q56\nq56 4294967295 q57\nq57 97 q5\nq58 4294967295 q59\nq59 4294967295 q0\nq60 4294967295 q61\nq61 97 q62\nq62 4294967295 q63\nq63 98 q64\nq64 4294967295 q65\nq65 97 q66\nq66 4294967295 q67\nq67 98 q0\nq68 4294967295 q69\nq69 97 q70\nq70 4294967295 q71\nq71 98 q15\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop single 'aabac'") {
        nft = nft::strings::replace_reluctant_literal(Word{ 'a', 'a', 'b', 'a', 'c' }, Word{}, &alphabet,
                                                      ReplaceMode::Single);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a', 'a', 'b', 'a', 'c' },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c'} }));
        CHECK(nft.is_tuple_in_lang({ { }, { } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q5\n%Levels q0:0 q1:1 q2:1 q3:1 q4:1 q5:0 q6:0 q7:1 q8:1 q9:1 q10:1 q11:0 q12:1 q13:0 q14:1 q15:0 q16:1 q17:1 q18:1 q19:1 q20:0 q21:1 q22:0 q23:1 q24:0 q25:1 q26:0 q27:1 q28:1 q29:1 q30:1 q31:0 q32:1 q33:0 q34:1 q35:0 q36:1 q37:0 q38:1 q39:0 q40:1 q41:0 q42:1 q43:0 q44:1 q45:0 q46:1 q47:0 q48:1 q49:1 q50:1 q51:1 q52:0 q53:1 q54:0 q55:1 q56:0 q57:1 q58:0 q59:1 q60:0 q61:1 q62:1 q63:1 q64:1 q65:0 q66:1 q67:0 q68:1 q69:0 q70:1 q71:0 q72:1 q73:0 q74:1 q75:0 q76:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq0 4294967295 q4\nq1 4294967295 q6\nq2 98 q0\nq3 99 q0\nq4 4294967295 q5\nq6 97 q7\nq6 98 q8\nq6 99 q9\nq6 4294967295 q10\nq7 4294967295 q15\nq8 97 q13\nq9 97 q11\nq10 97 q5\nq11 4294967295 q12\nq12 99 q0\nq13 4294967295 q14\nq14 98 q0\nq15 97 q16\nq15 98 q17\nq15 99 q18\nq15 4294967295 q19\nq16 97 q15\nq17 4294967295 q26\nq18 97 q22\nq19 97 q20\nq20 4294967295 q21\nq21 97 q5\nq22 4294967295 q23\nq23 97 q24\nq24 4294967295 q25\nq25 99 q0\nq26 97 q27\nq26 98 q28\nq26 99 q29\nq26 4294967295 q30\nq27 4294967295 q47\nq28 97 q41\nq29 97 q35\nq30 97 q31\nq31 4294967295 q32\nq32 97 q33\nq33 4294967295 q34\nq34 98 q5\nq35 4294967295 q36\nq36 97 q37\nq37 4294967295 q38\nq38 98 q39\nq39 4294967295 q40\nq40 99 q0\nq41 4294967295 q42\nq42 97 q43\nq43 4294967295 q44\nq44 98 q45\nq45 4294967295 q46\nq46 98 q0\nq47 97 q48\nq47 98 q49\nq47 99 q50\nq47 4294967295 q51\nq48 97 q73\nq49 97 q65\nq50 4294967295 q58\nq51 97 q52\nq52 4294967295 q53\nq53 97 q54\nq54 4294967295 q55\nq55 98 q56\nq56 4294967295 q57\nq57 97 q5\nq58 4294967295 q59\nq59 4294967295 q60\nq60 97 q61\nq60 98 q62\nq60 99 q63\nq60 4294967295 q64\nq61 97 q60\nq62 98 q60\nq63 99 q60\nq64 4294967295 q5\nq65 4294967295 q66\nq66 97 q67\nq67 4294967295 q68\nq68 98 q69\nq69 4294967295 q70\nq70 97 q71\nq71 4294967295 q72\nq72 98 q0\nq73 4294967295 q74\nq74 97 q75\nq75 4294967295 q76\nq76 98 q15\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }
}

TEST_CASE("mata::nft::strings::replace_reluctant_symbol()") {
    Nft nft{};
    Nft expected{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };

    SECTION("'a' replace with 'b' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_single_symbol('a', 'd', &alphabet, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'd', 'd', 'b', 'd', 'd', 'd', 'b', 'd', 'c', 'd' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'd', 'd', 'd', 'b', 'd', 'd', 'd', 'b', 'd', 'd' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0 q1:1 q2:1 q3:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq1 100 q0\nq2 98 q0\nq3 99 q0\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));

        // Use replace symbol with literal containing a single symbol.
        nft = nft::strings::replace_reluctant_single_symbol('a', Word{ 'd' }, &alphabet, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'd', 'd', 'b', 'd', 'd', 'd', 'b', 'd', 'c', 'd' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'd', 'd', 'd', 'b', 'd', 'd', 'd', 'b', 'd', 'd' } }));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("'a' replace with 'b' replace single") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_single_symbol('a', 'd', &alphabet, ReplaceMode::Single);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'd', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0 q4\n%Levels q0:0 q1:1 q2:1 q3:1 q4:0 q5:1 q6:1 q7:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq1 100 q4\nq2 98 q0\nq3 99 q0\nq4 97 q5\nq4 98 q6\nq4 99 q7\nq5 97 q4\nq6 98 q4\nq7 99 q4\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));

        // Use replace symbol with literal containing a single symbol.
        nft = nft::strings::replace_reluctant_single_symbol('a', Word{ 'd' }, &alphabet, ReplaceMode::Single);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'd', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'b', 'b', 'b', 'b', 'a', 'a', 'a', 'b', 'a', 'a' },
                                     { 'b', 'b', 'b', 'b', 'd', 'a', 'a', 'b', 'a', 'a' } }));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("'a' replace with 'bb' replace all") {
        nft = nft::strings::replace_reluctant_single_symbol('a', Word{ 'b', 'b' }, &alphabet, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'c', 'b', 'b' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' },
                                     { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0 q1:1 q2:1 q3:1 q4:0 q5:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq1 98 q4\nq2 98 q0\nq3 99 q0\nq4 4294967295 q5\nq5 98 q0\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop all 'a'") {
        nft = nft::strings::replace_reluctant_single_symbol('a', Word{}, &alphabet, ReplaceMode::All);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'b', 'b', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' },
                                     { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0\n%Levels q0:0 q1:1 q2:1 q3:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq1 4294967295 q0\nq2 98 q0\nq3 99 q0\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop single 'a'") {
        nft = nft::strings::replace_reluctant_single_symbol('a', Word{}, &alphabet, ReplaceMode::Single);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' },
                                     { 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b' } }));
        expected = nft::builder::parse_from_mata(std::string(
            "@NFT-explicit\n%Alphabet-auto\n%Initial q0\n%Final q0 q4\n%Levels q0:0 q1:1 q2:1 q3:1 q4:0 q5:1 q6:1 q7:1\n%LevelsCnt 2\nq0 97 q1\nq0 98 q2\nq0 99 q3\nq1 4294967295 q4\nq2 98 q0\nq3 99 q0\nq4 97 q5\nq4 98 q6\nq4 99 q7\nq5 97 q4\nq6 98 q4\nq7 99 q4\n"
        ));
        CHECK(nft::are_equivalent(nft, expected));
    }
}

TEST_CASE("mata::nft::strings::replace_reluctant_regex()") {
    Nft nft{};
    Nft expected{};
    EnumAlphabet alphabet{ 'a', 'b', 'c' };

    SECTION("'a+b+c' replace with 'dd' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a+b+c", Word{ 'd', 'd' }, &alphabet, ReplaceMode::All);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'b', 'c', 'c', 'a', 'a' },
                                     { 'a', 'a', 'a', 'b', 'd', 'd', 'c', 'a', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'c', 'a', 'a', 'a', 'b', 'b', 'a', 'a', 'b', 'c' },
                                     { 'd', 'd', 'a', 'a', 'a', 'b', 'b', 'd', 'd' } }));
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'c', 'a', 'a', 'a', 'b', 'b', 'a', 'a', 'b', 'c', 'c', 'a', 'b', 'a', 'c' },
                                     { 'd', 'd', 'a', 'a', 'a', 'b', 'b', 'd', 'd', 'c', 'a', 'b', 'a', 'c' } }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

//     SECTION("'a' replace with 'd' replace all") {
//         // Use replace symbol with symbol.
//         nft = nft::strings::replace_reluctant_regex("a", Word{ 'd' }, &alphabet, ReplaceMode::All);
//         nft.print_to_DOT(std::cout);
//         CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
//                                      { 'd', 'd', 'd', 'b', 'd', 'd', 'd', 'b', 'd', 'c', 'd' } }));
//         CHECK(nft.is_tuple_in_lang({ {},
//                                      {} }));
// //        expected = nft::builder::parse_from_mata(std::string(
// //        ));
// //        CHECK(nft::are_equivalent(nft, expected));
//     }
    // TODO(nft): Test dropping regex, correctly replacing shortest/longest match, ...

    SECTION("drop 'a' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a", Word{}, &alphabet, ReplaceMode::All);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'b', 'b', 'c' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop 'a' replace single") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a", Word{}, &alphabet, ReplaceMode::Single);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop 'a+b' replace single") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a+b", Word{}, &alphabet, ReplaceMode::Single);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("drop 'a+b' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a+b", Word{}, &alphabet, ReplaceMode::All);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("replace 'a+b' with 'dd' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a+b", Word{ 'd', 'd' }, &alphabet, ReplaceMode::All);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'd', 'd', 'd', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }

    SECTION("replace 'a+b' with 'dd' replace all") {
        // Use replace symbol with symbol.
        nft = nft::strings::replace_reluctant_regex("a+b", Word{ 'd', 'd' }, &alphabet, ReplaceMode::Single);
        nft.print_to_DOT(std::cout);
        CHECK(nft.is_tuple_in_lang({ { 'a', 'a', 'a', 'b', 'a', 'a', 'a', 'b', 'a', 'c', 'a' },
                                     { 'd', 'd', 'a', 'a', 'a', 'b', 'a', 'c', 'a' } }));
        CHECK(nft.is_tuple_in_lang({ {},
                                     {} }));
//        expected = nft::builder::parse_from_mata(std::string(
//        ));
//        CHECK(nft::are_equivalent(nft, expected));
    }
}
