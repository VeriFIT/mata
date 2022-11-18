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
    x.initial_states = {1, 3}; \
    x.final_states = {5}; \
    x.add_trans(1, 'a', 3); \
    x.add_trans(1, 'a', 10); \
    x.add_trans(1, 'b', 7); \
    x.add_trans(3, 'a', 7); \
    x.add_trans(3, 'b', 9); \
    x.add_trans(9, 'a', 9); \
    x.add_trans(7, 'b', 1); \
    x.add_trans(7, 'a', 3); \
    x.add_trans(7, 'c', 3); \
    x.add_trans(10, 'a', 7); \
    x.add_trans(10, 'b', 7); \
    x.add_trans(10, 'c', 7); \
    x.add_trans(7, 'a', 5); \
    x.add_trans(5, 'a', 5); \
    x.add_trans(5, 'c', 9); \


// Automaton B
#define FILL_WITH_AUT_B(x) \
    x.initial_states = {4}; \
    x.final_states = {2, 12}; \
    x.add_trans(4, 'c', 8); \
    x.add_trans(4, 'a', 8); \
    x.add_trans(8, 'b', 4); \
    x.add_trans(4, 'a', 6); \
    x.add_trans(4, 'b', 6); \
    x.add_trans(6, 'a', 2); \
    x.add_trans(2, 'b', 2); \
    x.add_trans(2, 'a', 0); \
    x.add_trans(0, 'a', 2); \
    x.add_trans(2, 'c', 12); \
    x.add_trans(12, 'a', 14); \
    x.add_trans(14, 'b', 12); \

// }}}

TEST_CASE("Mata::Nfa::intersection()")
{ // {{{
    Nfa a, b, res;
    std::unordered_map<std::pair<State, State>, State> prod_map;

    SECTION("Intersection of empty automata")
    {
        res = intersection(a, b, false, &prod_map);

        REQUIRE(res.initial_states.empty());
        REQUIRE(res.final_states.empty());
        REQUIRE(res.has_no_transitions());
        REQUIRE(prod_map.empty());
    }

    SECTION("Intersection of empty automata 2")
    {
        res = intersection(a, b);

        REQUIRE(res.initial_states.empty());
        REQUIRE(res.final_states.empty());
        REQUIRE(res.has_no_transitions());
    }

    a.increase_size(6);
    b.increase_size(7);

    SECTION("Intersection of automata with no transitions")
    {
        a.initial_states = {1, 3};
        a.final_states = {3, 5};

        b.initial_states = {4, 6};
        b.final_states = {4, 2};

        REQUIRE(!a.initial_states.empty());
        REQUIRE(!b.initial_states.empty());
        REQUIRE(!a.final_states.empty());
        REQUIRE(!b.final_states.empty());

        res = intersection(a, b, false, &prod_map);

        REQUIRE(!res.initial_states.empty());
        REQUIRE(!res.final_states.empty());

        State init_fin_st = prod_map[{3, 4}];

        REQUIRE(res.has_initial(init_fin_st));
        REQUIRE(res.has_final(init_fin_st));
    }

    a.increase_size(11);
    b.increase_size(15);

    SECTION("Intersection of automata with some transitions")
    {
        FILL_WITH_AUT_A(a);
        FILL_WITH_AUT_B(b);

        res = intersection(a, b, false, &prod_map);

        REQUIRE(res.has_initial(prod_map[{1, 4}]));
        REQUIRE(res.has_initial(prod_map[{3, 4}]));
        REQUIRE(res.has_final(prod_map[{5, 2}]));

        //for (const auto& c : prod_map) std::cout << c.first.first << "," << c.first.second << " -> " << c.second << "\n";
        //std::cout << prod_map[{7, 2}] << " " <<  prod_map[{1, 2}] << '\n';
        REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
        REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
        REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
        REQUIRE(res.has_trans(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
        REQUIRE(res.has_trans(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
        REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
        // REQUIRE(res.has_trans(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
        REQUIRE(res.has_trans(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
        REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
        // REQUIRE(res.has_trans(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.has_trans(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
        REQUIRE(res.has_trans(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.has_trans(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
        REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
        REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
        // REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
        REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
        // REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
        REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
        // REQUIRE(res.has_trans(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
        REQUIRE(res.has_trans(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
        REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
        REQUIRE(res.has_trans(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
        REQUIRE(res.has_trans(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
        REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
        REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
        REQUIRE(res.has_trans(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
    }

    SECTION("Intersection of automata with some transitions but without a final state")
    {
        FILL_WITH_AUT_A(a);
        FILL_WITH_AUT_B(b);
        b.final_states = {12};

        res = intersection(a, b, false, &prod_map);

        REQUIRE(res.has_initial(prod_map[{1, 4}]));
        REQUIRE(res.has_initial(prod_map[{3, 4}]));
        REQUIRE(is_lang_empty(res));
    }
} // }}}

TEST_CASE("Mata::Nfa::intersection() with preserving epsilon transitions")
{
    std::unordered_map<std::pair<State, State>, State> prod_map;

    Nfa a{6};
    a.make_initial(0);
    a.make_final({1, 4, 5});
    a.add_trans(0, EPSILON, 1);
    a.add_trans(1, 'a', 1);
    a.add_trans(1, 'b', 1);
    a.add_trans(1, 'c', 2);
    a.add_trans(2, 'b', 4);
    a.add_trans(2, EPSILON, 3);
    a.add_trans(3, 'a', 5);

    Nfa b{10};
    b.make_initial(0);
    b.make_final({2, 4, 8, 7});
    b.add_trans(0, 'b', 1);
    b.add_trans(0, 'a', 2);
    b.add_trans(2, 'a', 4);
    b.add_trans(2, EPSILON, 3);
    b.add_trans(3, 'b', 4);
    b.add_trans(0, 'c', 5);
    b.add_trans(5, 'a', 8);
    b.add_trans(5, EPSILON, 6);
    b.add_trans(6, 'a', 9);
    b.add_trans(6, 'b', 7);

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
    CHECK(result.states_number() == 13);

    CHECK(result.has_initial(prod_map[{0, 0}]));
    CHECK(result.initial_states.size() == 1);

    CHECK(result.has_final(prod_map[{1, 2}]));
    CHECK(result.has_final(prod_map[{1, 4}]));
    CHECK(result.has_final(prod_map[{4, 7}]));
    CHECK(result.has_final(prod_map[{5, 8}]));
    CHECK(result.final_states.size() == 4);

    // Check transitions.
    CHECK(result.get_num_of_trans() == 15);

    CHECK(result.has_trans(prod_map[{0, 0}], EPSILON, prod_map[{1, 0}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 0, 0 }]).size() == 1);

    CHECK(result.has_trans(prod_map[{1, 0}], 'b', prod_map[{1, 1}]));
    CHECK(result.has_trans(prod_map[{1, 0}], 'a', prod_map[{1, 2}]));
    CHECK(result.has_trans(prod_map[{1, 0}], 'c', prod_map[{2, 5}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 0 }]).size() == 3);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 1 }]).empty());

    CHECK(result.has_trans(prod_map[{1, 2}], EPSILON, prod_map[{1, 3}]));
    CHECK(result.has_trans(prod_map[{1, 2}], 'a', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 2 }]).size() == 2);

    CHECK(result.has_trans(prod_map[{1, 3}], 'b', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 3 }]).size() == 1);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 1, 4 }]).empty());

    CHECK(result.has_trans(prod_map[{2, 5}], EPSILON, prod_map[{3, 5}]));
    CHECK(result.has_trans(prod_map[{2, 5}], EPSILON, prod_map[{2, 6}]));
    CHECK(result.has_trans(prod_map[{2, 5}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 2, 5 }]).size() == 3);

    CHECK(result.has_trans(prod_map[{3, 5}], 'a', prod_map[{5, 8}]));
    CHECK(result.has_trans(prod_map[{3, 5}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 3, 5 }]).size() == 2);

    CHECK(result.has_trans(prod_map[{2, 6}], 'b', prod_map[{4, 7}]));
    CHECK(result.has_trans(prod_map[{2, 6}], EPSILON, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 2, 6 }]).size() == 2);

    CHECK(result.has_trans(prod_map[{3, 6}], 'a', prod_map[{5, 9}]));
    CHECK(result.get_trans_from_as_sequence(prod_map[{ 3, 6 }]).size() == 1);

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 4, 7 }]).empty());

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 5, 9 }]).empty());

    CHECK(result.get_trans_from_as_sequence(prod_map[{ 5, 8 }]).empty());
}

TEST_CASE("Mata::Nfa::intersection() for profiling", "[.profiling],[intersection]")
{
    Nfa a{6};
    a.make_initial(0);
    a.make_final({1, 4, 5});
    a.add_trans(0, EPSILON, 1);
    a.add_trans(1, 'a', 1);
    a.add_trans(1, 'b', 1);
    a.add_trans(1, 'c', 2);
    a.add_trans(2, 'b', 4);
    a.add_trans(2, EPSILON, 3);
    a.add_trans(3, 'a', 5);

    Nfa b{10};
    b.make_initial(0);
    b.make_final({2, 4, 8, 7});
    b.add_trans(0, 'b', 1);
    b.add_trans(0, 'a', 2);
    b.add_trans(2, 'a', 4);
    b.add_trans(2, EPSILON, 3);
    b.add_trans(3, 'b', 4);
    b.add_trans(0, 'c', 5);
    b.add_trans(5, 'a', 8);
    b.add_trans(5, EPSILON, 6);
    b.add_trans(6, 'a', 9);
    b.add_trans(6, 'b', 7);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nfa result{intersection(a, b, true) };
    }
}
