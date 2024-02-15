// TODO: some header

#include <unordered_set>
#include <vector>
#include <fstream>

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/strings.hh"
#include "mata/parser/re2parser.hh"

using namespace mata::nft;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;
using mata::EnumAlphabet;

using Word = std::vector<Symbol>;

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
        nft.levels.resize(nft.levels_cnt * ( alphabet.get_number_of_symbols() - 1));
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
        EnumAlphabet alphabet{ };
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
    mata::nfa::Nfa regex{};
    SECTION("nft::end_marker_dfa()") {
        mata::parser::create_nfa(&regex, "cb+a+");
        mata::nfa::Nfa nfa_end_marker{ end_marker_dfa(regex) };
        mata::nfa::Nfa dfa_expected_end_marker{};
        dfa_expected_end_marker.initial = { 0 };
        dfa_expected_end_marker.final = { 4 };
        dfa_expected_end_marker.delta.add(0, 'c', 1);
        dfa_expected_end_marker.delta.add(1, 'b', 2);
        dfa_expected_end_marker.delta.add(2, 'b', 2);
        dfa_expected_end_marker.delta.add(2, 'a', 3);
        dfa_expected_end_marker.delta.add(3, EPSILON, 4);
        dfa_expected_end_marker.delta.add(4, 'a', 3);
        CHECK(nfa_end_marker.is_deterministic());
        CHECK(mata::nfa::are_equivalent(nfa_end_marker, dfa_expected_end_marker));
    }
}
