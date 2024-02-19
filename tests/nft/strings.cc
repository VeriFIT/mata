// TODO: some header

#include <unordered_set>
#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/strings.hh"
#include "mata/parser/re2parser.hh"

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
        nft.levels_cnt = 3;
        nft.levels.resize(nft.levels_cnt * (alphabet.get_number_of_symbols() - 1));
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
        nft.levels_cnt = 3;
        nft.levels.resize(1);
        nft.levels[0] = 0;
        Nft nft_identity{ create_identity(&alphabet, 3) };
        CHECK(nft_identity.is_identical(nft));
    }

    SECTION("identity nft one symbol") {
        EnumAlphabet alphabet{ 0 };
        nft.alphabet = &alphabet;
        nft.levels_cnt = 2;
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
        nft.levels_cnt = 1;
        nft.levels.resize(1);
        nft.levels[0] = 0;
        Nft nft_identity{ create_identity(&alphabet, 1) };
        CHECK(nft_identity.is_identical(nft));
    }
}

TEST_CASE("nft::create_identity_with_single_replace()") {
    Nft nft{};
    nft.initial = { 0 };
    nft.final = { 0 };
    SECTION("small identity nft") {
        EnumAlphabet alphabet{ 0, 1, 2, 3 };
        nft.alphabet = &alphabet;
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 0, 0);
        nft.delta.add(0, 1, 2);
        nft.delta.add(2, 3, 0);
        nft.delta.add(0, 2, 3);
        nft.delta.add(3, 2, 0);
        nft.delta.add(0, 3, 4);
        nft.delta.add(4, 3, 0);
        nft.levels_cnt = 2;
        nft.levels.resize(5);
        nft.levels[0] = 0;
        nft.levels[1] = 1;
        nft.levels[2] = 1;
        nft.levels[3] = 1;
        nft.levels[4] = 1;
        Nft nft_identity_with_replace{ create_identity_with_single_replace(&alphabet, 1, 3) };
        CHECK(nft_identity_with_replace.is_identical(nft));
    }

    SECTION("identity nft no symbols") {
        EnumAlphabet alphabet{};
        CHECK_THROWS(create_identity_with_single_replace(&alphabet, 1, 2));
    }

    SECTION("identity nft one symbol") {
        EnumAlphabet alphabet{ 0 };
        nft.alphabet = &alphabet;
        nft.levels_cnt = 2;
        nft.levels.resize(2);
        nft.levels[0] = 0;
        nft.levels[1] = 1;
        nft.delta.add(0, 0, 1);
        nft.delta.add(1, 1, 0);
        Nft nft_identity{ create_identity_with_single_replace(&alphabet, 0, 1) };
        CHECK(nft_identity.is_identical(nft));
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
        dft_expected_end_marker.levels_cnt = 2;
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
        nfa::Nfa dfa_generic_end_marker{ generic_end_marker_dfa("cb+a+", &alphabet) };
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
        dft_expected.levels_cnt = 2;
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
        nfa::Nfa dfa_generic_end_marker{ generic_end_marker_dfa("ab+a+", &alphabet) };
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
        dft_expected.final = { 0, 2, 7, 14};
        dft_expected.levels_cnt = 2;
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

    SECTION("nft::begin_marker_nft() regex cb+a+") {
        nfa::Nfa nfa_begin_marker{ begin_marker_nfa("cb+a+", &alphabet) };
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
        nft_expected.levels_cnt = 2;
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
    }

    SECTION("nft::begin_marker_nft() regex ab+a+") {
        nfa::Nfa nfa_begin_marker{ begin_marker_nfa("ab+a+", &alphabet) };
        nfa::Nfa nfa_expected{ nfa::Delta{}, { 0 }, { 0, 1, 2, 4 }};
        nfa_expected.delta.add(1, 'a', 0);
        nfa_expected.delta.add(0, 'b', 0);
        nfa_expected.delta.add(0, 'c', 0);
        nfa_expected.delta.add(1, 'a', 1);
        nfa_expected.delta.add(2, 'b', 1);
        nfa_expected.delta.add(0, 'c', 1);
        nfa_expected.delta.add(3, 'a', 2);
        nfa_expected.delta.add(2, 'b', 2);
        nfa_expected.delta.add(0, 'c', 2);
        nfa_expected.delta.add(4, EPSILON, 3);
        nfa_expected.delta.add(3, 'a', 4);
        nfa_expected.delta.add(2, 'b', 4);
        nfa_expected.delta.add(0, 'c', 4);
        CHECK(nfa::are_equivalent(nfa_begin_marker, nfa_expected));

        Nft nft_begin_marker{ begin_marker_nft(nfa_begin_marker, MARKER) };
        Nft nft_expected{};
        nft_expected.initial.insert(0);
        nft_expected.final.insert(1);
        nft_expected.levels_cnt = 2;
        nft_expected.delta.add(0, EPSILON, 1);
        nft_expected.delta.add(0, EPSILON, 2);
        nft_expected.delta.add(0, EPSILON, 3);
        nft_expected.delta.add(0, EPSILON, 5);
        nft_expected.delta.add(1, 'b', 6);
        nft_expected.delta.add(6, 'b', 1);
        nft_expected.delta.add(1, 'c', 7);
        nft_expected.delta.add(7, 'c', 1);
        nft_expected.delta.add(7, 'c', 2);
        nft_expected.delta.add(7, 'c', 3);
        nft_expected.delta.add(7, 'c', 5);
        nft_expected.delta.add(2, 'a', 8);
        nft_expected.delta.add(8, 'a', 2);
        nft_expected.delta.add(8, 'a', 1);
        nft_expected.delta.add(3, 'b', 9);
        nft_expected.delta.add(9, 'b', 3);
        nft_expected.delta.add(9, 'b', 2);
        nft_expected.delta.add(9, 'b', 5);
        nft_expected.delta.add(4, 'a', 10);
        nft_expected.delta.add(10, 'a', 5);
        nft_expected.delta.add(10, 'a', 3);
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
    }
}