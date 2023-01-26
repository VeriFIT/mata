/* tests-nfa-intersection.cc -- Tests for intersection of NFAs
 *
 * Copyright (c) 2022 David Chocholatý <chocholaty.david@protonmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>

using namespace Mata::Nfa;
using namespace Mata::util;
using namespace Mata::Parser;

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

TEST_CASE("Mata::Nfa::intersection()")
{ // {{{
    Nfa a, b, res;
    std::unordered_map<std::pair<State, State>, State> prod_map;

    SECTION("Intersection of empty automata")
    {
        res = intersection(a, b, false, &prod_map);

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

        res = intersection(a, b, false, &prod_map);

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

        res = intersection(a, b, false, &prod_map);

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

        res = intersection(a, b, false, &prod_map);

        REQUIRE(res.initial[prod_map[{1, 4}]]);
        REQUIRE(res.initial[prod_map[{3, 4}]]);
        REQUIRE(is_lang_empty(res));
    }
} // }}}

TEST_CASE("Mata::Nfa::intersection() with preserving epsilon transitions")
{
    std::unordered_map<std::pair<State, State>, State> prod_map;

    Nfa a{6};
    a.initial.add(0);
    a.final.add({1, 4, 5});
    a.delta.add(0, EPSILON, 1);
    a.delta.add(1, 'a', 1);
    a.delta.add(1, 'b', 1);
    a.delta.add(1, 'c', 2);
    a.delta.add(2, 'b', 4);
    a.delta.add(2, EPSILON, 3);
    a.delta.add(3, 'a', 5);

    Nfa b{10};
    b.initial.add(0);
    b.final.add({2, 4, 8, 7});
    b.delta.add(0, 'b', 1);
    b.delta.add(0, 'a', 2);
    b.delta.add(2, 'a', 4);
    b.delta.add(2, EPSILON, 3);
    b.delta.add(3, 'b', 4);
    b.delta.add(0, 'c', 5);
    b.delta.add(5, 'a', 8);
    b.delta.add(5, EPSILON, 6);
    b.delta.add(6, 'a', 9);
    b.delta.add(6, 'b', 7);

    Nfa result{intersection(a, b, true, &prod_map) };

    // Check states.
    CHECK(result.is_state(prod_map[{0, 0}]));
    CHECK(result.is_state(prod_map[{1, 0}]));
    CHECK(result.is_state(prod_map[{1, 1}]));
    CHECK(result.is_state(prod_map[{1, 2}]));
    CHECK(result.is_state(prod_map[{1, 3}]));
    CHECK(result.is_state(prod_map[{1, 4}]));
    CHECK(result.is_state(prod_map[{2, 5}]));
    CHECK(result.is_state(prod_map[{3, 5}]));
    CHECK(result.is_state(prod_map[{2, 6}]));
    CHECK(result.is_state(prod_map[{3, 6}]));
    CHECK(result.is_state(prod_map[{4, 7}]));
    CHECK(result.is_state(prod_map[{5, 9}]));
    CHECK(result.is_state(prod_map[{5, 8}]));
    CHECK(result.size() == 13);

    CHECK(result.initial[prod_map[{0, 0}]]);
    CHECK(result.initial.size() == 1);

    CHECK(result.final[prod_map[{1, 2}]]);
    CHECK(result.final[prod_map[{1, 4}]]);
    CHECK(result.final[prod_map[{4, 7}]]);
    CHECK(result.final[prod_map[{5, 8}]]);
    CHECK(result.final.size() == 4);

    // Check transitions.
    CHECK(result.get_num_of_trans() == 15);

    CHECK(result.delta.contains(prod_map[{0, 0}], EPSILON, prod_map[{1, 0}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 0, 0 }]).size() == 1);

    CHECK(result.delta.contains(prod_map[{1, 0}], 'b', prod_map[{1, 1}]));
    CHECK(result.delta.contains(prod_map[{1, 0}], 'a', prod_map[{1, 2}]));
    CHECK(result.delta.contains(prod_map[{1, 0}], 'c', prod_map[{2, 5}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 0 }]).size() == 3);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 1 }]).empty());

    CHECK(result.delta.contains(prod_map[{1, 2}], EPSILON, prod_map[{1, 3}]));
    CHECK(result.delta.contains(prod_map[{1, 2}], 'a', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 2 }]).size() == 2);

    CHECK(result.delta.contains(prod_map[{1, 3}], 'b', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 3 }]).size() == 1);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 4 }]).empty());

    CHECK(result.delta.contains(prod_map[{2, 5}], EPSILON, prod_map[{3, 5}]));
    CHECK(result.delta.contains(prod_map[{2, 5}], EPSILON, prod_map[{2, 6}]));
    CHECK(result.delta.contains(prod_map[{2, 5}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 2, 5 }]).size() == 3);

    CHECK(result.delta.contains(prod_map[{3, 5}], 'a', prod_map[{5, 8}]));
    CHECK(result.delta.contains(prod_map[{3, 5}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 3, 5 }]).size() == 2);

    CHECK(result.delta.contains(prod_map[{2, 6}], 'b', prod_map[{4, 7}]));
    CHECK(result.delta.contains(prod_map[{2, 6}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 2, 6 }]).size() == 2);

    CHECK(result.delta.contains(prod_map[{3, 6}], 'a', prod_map[{5, 9}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 3, 6 }]).size() == 1);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 4, 7 }]).empty());

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 5, 9 }]).empty());

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 5, 8 }]).empty());
}

TEST_CASE("Mata::Nfa::intersection() for profiling", "[.profiling],[intersection]")
{
    Nfa a{6};
    a.initial.add(0);
    a.final.add({1, 4, 5});
    a.delta.add(0, EPSILON, 1);
    a.delta.add(1, 'a', 1);
    a.delta.add(1, 'b', 1);
    a.delta.add(1, 'c', 2);
    a.delta.add(2, 'b', 4);
    a.delta.add(2, EPSILON, 3);
    a.delta.add(3, 'a', 5);

    Nfa b{10};
    b.initial.add(0);
    b.final.add({2, 4, 8, 7});
    b.delta.add(0, 'b', 1);
    b.delta.add(0, 'a', 2);
    b.delta.add(2, 'a', 4);
    b.delta.add(2, EPSILON, 3);
    b.delta.add(3, 'b', 4);
    b.delta.add(0, 'c', 5);
    b.delta.add(5, 'a', 8);
    b.delta.add(5, EPSILON, 6);
    b.delta.add(6, 'a', 9);
    b.delta.add(6, 'b', 7);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nfa result{intersection(a, b, true) };
    }
}
