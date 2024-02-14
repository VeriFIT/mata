/* tests-nft-plumbing.cc -- Tests plumbing versions of functions
 */


#include <unordered_set>

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"
#include "mata/nft/plumbing.hh"

using Symbol = mata::Symbol;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x) \
    x.initial = {1, 3}; \
    x.final = {5}; \
    x.delta.add(1, 'a', 3); \
    x.delta.add(1, 'a', 10); \
    x.delta.add(1, 'b', 7); \
    x.delta.add(3, 'a', 7); \
    x.delta.add(3, 'b', 9); \
    x.delta.add(9, 'a', 9); \
    x.delta.add(7, 'b', 1); \
    x.delta.add(7, 'a', 3); \
    x.delta.add(7, 'c', 3); \
    x.delta.add(10, 'a', 7); \
    x.delta.add(10, 'b', 7); \
    x.delta.add(10, 'c', 7); \
    x.delta.add(7, 'a', 5); \
    x.delta.add(5, 'a', 5); \
    x.delta.add(5, 'c', 9); \


// Automaton B
#define FILL_WITH_AUT_B(x) \
    x.initial = {4}; \
    x.final = {2, 12}; \
    x.delta.add(4, 'c', 8); \
    x.delta.add(4, 'a', 8); \
    x.delta.add(8, 'b', 4); \
    x.delta.add(4, 'a', 6); \
    x.delta.add(4, 'b', 6); \
    x.delta.add(6, 'a', 2); \
    x.delta.add(2, 'b', 2); \
    x.delta.add(2, 'a', 0); \
    x.delta.add(0, 'a', 2); \
    x.delta.add(2, 'c', 12); \
    x.delta.add(12, 'a', 14); \
    x.delta.add(14, 'b', 12); \

// }}}

TEST_CASE("Mata::nft::Plumbing") {
    mata::nft::Nft lhs{};
    mata::nft::Nft rhs{};
    mata::nft::Nft result{};
    OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b", "c" } };

    SECTION("Mata::nft::Plumbing::concatenate") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        mata::nft::plumbing::concatenate(&result, lhs, rhs);
        CHECK(result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::intersection") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        mata::nft::plumbing::intersection(&result, lhs, rhs);
        CHECK(result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::union") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        mata::nft::plumbing::uni(&result, lhs, rhs);
        CHECK(!result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::remove_epsilon") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::remove_epsilon(&result, lhs);
        CHECK(!result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::revert") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::revert(&result, lhs);
        CHECK(!result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::reduce") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::reduce(&result, lhs);
        CHECK(!result.is_lang_empty());
        CHECK(result.num_of_states() <= lhs.num_of_states());
    }

    SECTION("Mata::nft::Plumbing::determinize") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::determinize(&result, lhs);
        CHECK(!result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::minimize") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::minimize(&result, lhs);
        CHECK(!result.is_lang_empty());
    }

    SECTION("Mata::nft::Plumbing::complement") {
        FILL_WITH_AUT_A(lhs);
        mata::nft::plumbing::complement(&result, lhs, alph);
        CHECK(!result.is_lang_empty());
    }
}
