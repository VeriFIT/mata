/* tests-nft-intersection.cc -- Tests for intersection of NFTs
 */


#include <unordered_set>

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"

using namespace mata::nft;
using namespace mata::utils;
using namespace mata::parser;

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

TEST_CASE("mata::nft::intersection()")
{ // {{{
    Nft a, b, res;
    std::unordered_map<std::pair<State, State>, State> prod_map;

    SECTION("Intersection of empty automata")
    {
        res = intersection(a, b, &prod_map);

        REQUIRE(res.initial.empty());
        REQUIRE(res.final.empty());
        REQUIRE(res.delta.empty());
        REQUIRE(prod_map.empty());
    }

    SECTION("Intersection of empty automata 2")
    {
        res = intersection(a, b);

        REQUIRE(res.initial.empty());
        REQUIRE(res.final.empty());
        REQUIRE(res.delta.empty());
    }

    a.add_state(5);
    b.add_state(6);

    SECTION("Intersection of automata with no transitions")
    {
        a.initial = {1, 3};
        a.final = {3, 5};

        b.initial = {4, 6};
        b.final = {4, 2};

        REQUIRE(!a.initial.empty());
        REQUIRE(!b.initial.empty());
        REQUIRE(!a.final.empty());
        REQUIRE(!b.final.empty());

        res = intersection(a, b, &prod_map);

        REQUIRE(!res.initial.empty());
        REQUIRE(!res.final.empty());

        State init_fin_st = prod_map[{3, 4}];

        REQUIRE(res.initial[init_fin_st]);
        REQUIRE(res.final[init_fin_st]);
    }

    a.add_state(10);
    b.add_state(14);

    SECTION("Intersection of automata with some transitions")
    {
        FILL_WITH_AUT_A(a);
        FILL_WITH_AUT_B(b);

        res = intersection(a, b, &prod_map);

        REQUIRE(res.initial[prod_map[{1, 4}]]);
        REQUIRE(res.initial[prod_map[{3, 4}]]);
        REQUIRE(res.final[prod_map[{5, 2}]]);

        //for (const auto& c : prod_map) std::cout << c.first.first << "," << c.first.second << " -> " << c.second << "\n";
        //std::cout << prod_map[{7, 2}] << " " <<  prod_map[{1, 2}] << '\n';
        REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
        REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
        REQUIRE(res.delta.contains(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
        REQUIRE(res.delta.contains(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
        REQUIRE(res.delta.contains(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
        REQUIRE(res.delta.contains(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
        // REQUIRE(res.delta.contains(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
        REQUIRE(res.delta.contains(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
        REQUIRE(res.delta.contains(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
        // REQUIRE(res.delta.contains(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.delta.contains(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
        REQUIRE(res.delta.contains(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.delta.contains(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.delta.contains(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
        REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
        REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
        // REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
        REQUIRE(res.delta.contains(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
        // REQUIRE(res.delta.contains(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
        REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.delta.contains(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
        // REQUIRE(res.delta.contains(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
        REQUIRE(res.delta.contains(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
        REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.delta.contains(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.delta.contains(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
        REQUIRE(res.delta.contains(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
        REQUIRE(res.delta.contains(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
        REQUIRE(res.delta.contains(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
    }

    SECTION("Intersection of automata with some transitions but without a final state")
    {
        FILL_WITH_AUT_A(a);
        FILL_WITH_AUT_B(b);
        b.final = {12};

        res = intersection(a, b, &prod_map);

        REQUIRE(res.initial[prod_map[{1, 4}]]);
        REQUIRE(res.initial[prod_map[{3, 4}]]);
        REQUIRE(res.is_lang_empty());
    }

    Nft expected;
    SECTION("Intersection of transducers with epsilon transitions.") {
        SECTION("The intersection results in an empty language.") {
            a = Nft(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            a.delta.add(0, EPSILON, 1);
            a.delta.add(1, 'b', 2);

            b = Nft(3, { 0 }, { 2 }, { 0, 1, 0 }, 2);
            b.delta.add(0, 'b', 1);
            b.delta.add(1, EPSILON, 2);

            res = intersection(a, b);

            CHECK(!res.initial.empty());
            CHECK(res.final.empty());
            CHECK(res.is_lang_empty());
        }

        SECTION("Epsilon is treated as an alphabet symbol.") {
            a = Nft(5, { 0 }, { 3, 4 }, { 0, 1, 1, 0, 0 }, 2);
            a.delta.add(0, EPSILON, 1);
            a.delta.add(0, 'b', 2);
            a.delta.add(1, 'a', 3);
            a.delta.add(2, 'a', 4);
            a.delta.add(4, EPSILON, 4);

            b = Nft(4, { 0 }, { 3 }, { 0, 1, 1, 0 }, 2);
            b.delta.add(0, EPSILON, 1);
            b.delta.add(0, 'b', 2);
            b.delta.add(1, 'a', 3);
            b.delta.add(1, 'b', 3);
            b.delta.add(2, 'a', 3);

            expected = Nft(4, { 0 }, { 3 }, { 0, 1, 1, 0 }, 2);
            expected.delta.add(0, EPSILON, 1);
            expected.delta.add(0, 'b', 2);
            expected.delta.add(1, 'a', 3);
            expected.delta.add(2, 'a', 3);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }
    }

    SECTION("Intersection of linear transducers with multiple levels.") {
        SECTION("Intersection 1") {
            a = Nft(4, { 0 }, { 3 }, { 0, 1, 3, 0 }, 4);
            a.delta.add(0, 'a', 1);
            a.delta.add(1, 'b', 2);
            a.delta.add(2, 'c', 3);

            b = Nft(3, { 0 }, { 2 }, { 0, 2, 0 }, 4);
            b.delta.add(0, 'a', 1);
            b.delta.add(1, 'b', 2);

            expected = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'b', 2);
            expected.delta.add(2, 'b', 3);
            expected.delta.add(3, 'c', 4);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }

        SECTION("Intersection 2") {
            a = Nft(2, { 0 }, { 1 }, { 0, 0 }, 1);
            a.delta.add(0, 'a', 1);

            b = Nft(3, { 0 }, { 2 }, { 0, 1, 0}, 1);
            b.delta.add(0, 'a', 1);
            b.delta.add(1, 'b', 2);

            expected = b;

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }

        SECTION("Intersection 3") {
            a = Nft(4, { 0 }, { 3 }, { 0, 2, 3, 0 }, 5);
            a.delta.add(0, 'a', 1);
            a.delta.add(1, 'b', 2);
            a.delta.add(2, 'a', 3);

            b = Nft(5, { 0 }, { 4 }, { 0, 1, 3, 4, 0 }, 5);
            b.delta.add(0, 'a', 1);
            b.delta.add(1, 'c', 2);
            b.delta.add(2, 'b', 3);
            b.delta.add(3, 'a', 4);

            std::unordered_map<std::pair<State, State>, State> prod_map;
            Nft res = intersection(a, b, &prod_map);

            REQUIRE(!res.initial.empty());
            REQUIRE(res.final.empty());
            REQUIRE(res.delta.contains(prod_map[{0, 0}], 'a', prod_map[{1, 1}]));
            REQUIRE(res.delta.contains(prod_map[{1, 1}], 'c', prod_map[{1, 2}]));
            REQUIRE(res.delta.contains(prod_map[{1, 2}], 'b', prod_map[{2, 2}]));
            CHECK(res.is_lang_empty());
        }
    }

    SECTION("Intersection of complex transducers with multiple levels and an epsilon transition") {
        a = Nft(8, { 0 }, { 5, 6, 7 }, { 0, 1, 1, 2, 2, 0, 0, 0 }, 3);
        a.delta.add(0, 'a', 1);
        a.delta.add(0, 'b', 2);
        a.delta.add(0, 'a', 4);
        a.delta.add(1, 'c', 3);
        a.delta.add(2, 'a', 4);
        a.delta.add(2, 'c', 7);
        a.delta.add(3, 'a', 5);
        a.delta.add(4, 'b', 6);
        a.delta.add(5, 'a', 3);
        a.delta.add(6, EPSILON, 4);
        a.delta.add(7, 'c', 2);

        b = Nft(5, { 0 }, { 3, 4 }, { 0, 1, 2, 0, 0 }, 3);
        b.delta.add(0, 'a', 1);
        b.delta.add(0, 'b', 1);
        b.delta.add(0, 'a', 3);
        b.delta.add(1, 'a', 2);
        b.delta.add(1, 'c', 4);
        b.delta.add(2, 'b', 4);
        b.delta.add(3, 'c', 3);
        b.delta.add(4, EPSILON, 4);

        expected = Nft(12, { 0 }, { 4, 5, 9, 11 }, { 0, 1, 1, 2, 0, 0, 2, 1, 2, 0, 2, 0 }, 3);
        expected.delta.add(0, 'b', 1);
        expected.delta.add(0, 'a', 2);
        expected.delta.add(0, 'a', 7);
        expected.delta.add(0, 'a', 10);
        expected.delta.add(1, 'a', 3);
        expected.delta.add(1, 'c', 4);
        expected.delta.add(2, 'a', 3);
        expected.delta.add(2, 'c', 6);
        expected.delta.add(3, 'b', 5);
        expected.delta.add(5, EPSILON, 6);
        expected.delta.add(6, 'b', 5);
        expected.delta.add(7, 'c', 8);
        expected.delta.add(8, 'a', 9);
        expected.delta.add(10, 'b', 11);

        res = intersection(a, b);

        CHECK(are_equivalent(res, expected));
    }

    SECTION("Intersection of transducers with the DONT_CARE symbol") {
        SECTION("DONT_CARE is in the lhs.") {
            a = Nft(3, { 0 }, { 2 }, { 0, 2, 0 }, 3);
            a.delta.add(0, DONT_CARE, 1);
            a.delta.add(1, 'c', 2);

            b = Nft(7, { 0 }, { 4, 5, 6 }, { 0, 1, 1, 1, 0, 0, 0 }, 3);
            b.delta.add(0, 'a', 1);
            b.delta.add(0, 'b', 2);
            b.delta.add(0, 'a', 3);
            b.delta.add(1, 'c', 4);
            b.delta.add(2, 'd', 5);
            b.delta.add(3, 'e', 6);

            expected = Nft(10, { 0 }, { 7, 8, 9 }, { 0, 1, 1, 1, 2, 2, 2, 0, 0, 0 }, 3);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(0, 'b', 2);
            expected.delta.add(0, 'a', 3);
            expected.delta.add(1, 'c', 4);
            expected.delta.add(2, 'd', 5);
            expected.delta.add(3, 'e', 6);
            expected.delta.add(4, 'c', 7);
            expected.delta.add(5, 'c', 8);
            expected.delta.add(6, 'c', 9);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }

        SECTION("DONT_CARE is in the rhs.") {
            a = Nft(3, { 0 }, { 2 }, { 0, 2, 0 }, 3);
            a.delta.add(0, 'a', 1);
            a.delta.add(1, 'c', 2);

            b = Nft(7, { 0 }, { 4, 5, 6 }, { 0, 1, 1, 1, 0, 0, 0 }, 3);
            b.delta.add(0, DONT_CARE, 1);
            b.delta.add(0, DONT_CARE, 2);
            b.delta.add(0, DONT_CARE, 3);
            b.delta.add(1, 'c', 4);
            b.delta.add(2, 'd', 5);
            b.delta.add(3, 'e', 6);

            expected = Nft(8, { 0 }, { 5, 6, 7 }, { 0, 1, 2, 2, 2, 0, 0, 0 }, 3);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'c', 2);
            expected.delta.add(1, 'd', 3);
            expected.delta.add(1, 'e', 4);
            expected.delta.add(2, 'c', 5);
            expected.delta.add(3, 'c', 6);
            expected.delta.add(4, 'c', 7);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }

        SECTION("DONT_CARE is in both lhs and rhs. In lhs, DONT_CARE is at a higher level than it is in rhs.") {
            a = Nft(3, { 0 }, { 2 }, { 0, 2, 0 }, 3);
            a.delta.add(0, DONT_CARE, 1);
            a.delta.add(1, DONT_CARE, 2);

            b = Nft(7, { 0 }, { 4, 5, 6 }, { 0, 1, 1, 1, 0, 0, 0 }, 3);
            b.delta.add(0, DONT_CARE, 1);
            b.delta.add(0, DONT_CARE, 2);
            b.delta.add(0, DONT_CARE, 3);
            b.delta.add(1, 'c', 4);
            b.delta.add(2, 'd', 5);
            b.delta.add(3, 'e', 6);

            expected = Nft(10, { 0 }, { 7, 8, 9 }, { 0, 1, 1, 1, 2, 2, 2, 0, 0, 0 }, 3);
            expected.delta.add(0, DONT_CARE, 1);
            expected.delta.add(0, DONT_CARE, 2);
            expected.delta.add(0, DONT_CARE, 3);
            expected.delta.add(1, 'c', 4);
            expected.delta.add(2, 'd', 5);
            expected.delta.add(3, 'e', 6);
            expected.delta.add(4, DONT_CARE, 7);
            expected.delta.add(5, DONT_CARE, 8);
            expected.delta.add(6, DONT_CARE, 9);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }

        SECTION("DONT_CARE is in the rhs at a higher level than it is in the lhs.") {
            a = Nft(3, { 0 }, { 2 }, { 0, 1, 0 }, 3);
            a.delta.add(0, 'a', 1);
            a.delta.add(1, DONT_CARE, 2);

            b = Nft(3, { 0 }, { 2 }, { 0, 2, 0 }, 3);
            b.delta.add(0, 'a', 1);
            b.delta.add(1, DONT_CARE, 2);

            expected = Nft(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, DONT_CARE, 2);
            expected.delta.add(2, DONT_CARE, 3);

            res = intersection(a, b);

            CHECK(are_equivalent(res, expected));
        }
    }
} // }}}


TEST_CASE("mata::nft::intersection() for profiling", "[.profiling],[intersection]")
{
    Nft a{4};
    a.initial.insert(0);
    a.final.insert({ 0, 2, 3});
    a.delta.add(0, 'a', 0);
    a.delta.add(0, 'b', 0);
    a.delta.add(0, 'c', 1);
    a.delta.add(1, 'a', 3);
    a.delta.add(1, 'b', 2);

    Nft b{9};
    b.initial.insert(0);
    b.final.insert({2, 4, 8, 7});
    b.delta.add(0, 'b', 1);
    b.delta.add(0, 'a', 2);
    b.delta.add(0, 'c', 3);
    b.delta.add(2, 'a', 8);
    b.delta.add(2, 'b', 8);
    b.delta.add(3, 'a', 6);
    b.delta.add(3, 'a', 4);
    b.delta.add(3, 'a', 7);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nft result{intersection(a, b) };
    }
}

TEST_CASE("Move semantics of NFT", "[.profiling][std::move]") {
    Nft b{10};
    b.initial.insert(0);
    b.final.insert({2, 4, 8, 7});
    b.delta.add(0, 'b', 1);
    b.delta.add(0, 'a', 2);
    b.delta.add(0, 'c', 3);
    b.delta.add(2, 'a', 8);
    b.delta.add(2, 'b', 8);
    b.delta.add(3, 'a', 6);
    b.delta.add(3, 'a', 4);
    b.delta.add(3, 'a', 7);

    for (size_t i{ 0 }; i < 1'000'000; ++i) {
        Nft a{ std::move(b) };
        a.initial.insert(1);
        b = std::move(a);
    }
}
