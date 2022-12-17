/* tests-nfa-concatenation.cc -- Tests for concatenation of NFAs
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
#include <mata/nfa-strings.hh>

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::util;
using namespace Mata::Parser;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x) \
    x.initial = {1, 3}; \
    x.final = {5}; \
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
    x.initial = {4}; \
    x.final = {2, 12}; \
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

TEST_CASE("Mata::Nfa::concatenate()") {
    Nfa lhs{};
    Nfa rhs{};
    Nfa result{};

    SECTION("Empty automaton without states") {
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states") {
        rhs.increase_size(1);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states") {
        lhs.increase_size(1);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states with other with initial states") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other with initial states") {
        rhs.increase_size(1);
        rhs.initial.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states with other non-empty automaton") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other non-empty automaton") {
        rhs.increase_size(1);
        rhs.initial.add(0);
        rhs.final.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty automaton") {
        lhs.increase_size(1);
        rhs.increase_size(1);
        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty language") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);

        result = concatenate(lhs, rhs);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
    }

    SECTION("Empty language rhs automaton") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final.empty());
        CHECK(result.states_number() == 1);
        CHECK(result.has_no_transitions());
    }

    SECTION("Single state automata accepting an empty string") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);
        rhs.final.add(0);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[0]);
        CHECK(result.states_number() == 1);
        CHECK(result.has_no_transitions());
    }

    SECTION("Empty language rhs automaton") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[1]);
        CHECK(result.states_number() == 2);
        CHECK(result.has_no_transitions());
    }

    SECTION("Simple two state rhs automaton") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[1]);
        CHECK(result.states_number() == 2);
        CHECK(result.has_trans(0, 'a', 1));
    }

    SECTION("Simple two state automata") {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.states_number() == 3);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state automata with higher state num for non-final state") {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(4);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);
        rhs.add_trans(0, 'c', 3);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.states_number() == 5);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton") {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(1);
        rhs.initial.add(0);
        rhs.final.add(0);
        rhs.add_trans(0, 'a', 0);

        result = concatenate(lhs, rhs);

        CHECK(result.initial[0]);
        CHECK(result.final[1]);
        CHECK(result.states_number() == 2);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 1));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b' }) != shortest_words.end());
    }

    SECTION("Automaton A concatenate automaton B") {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(lhs, rhs);

        CHECK(result.initial.size() == 2);
        CHECK(result.initial[1]);
        CHECK(result.initial[3]);

        CHECK(result.states_number() == 25);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Automaton B concatenate automaton A") {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(rhs, lhs);

        CHECK(result.states_number() == 24);

        CHECK(result.initial.size() == 1);
        // Final state 2 in automaton B will not stay in the result automaton.
        // Hence, initial state 4 in aut B will be initial state 3 in the result.
        CHECK(result.initial[3]);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Sample automata") {
        lhs.increase_size_for_state(0);
        lhs.initial.add(0);
        lhs.final.add(0);
        lhs.add_trans(0, 58, 0);
        lhs.add_trans(0, 65, 0);
        lhs.add_trans(0, 102, 0);
        lhs.add_trans(0, 112, 0);
        lhs.add_trans(0, 115, 0);
        lhs.add_trans(0, 116, 0);

        rhs.increase_size_for_state(5);
        rhs.final.add({0, 5});
        rhs.initial.add(5);
        rhs.add_trans(1, 112, 0);
        rhs.add_trans(2, 116, 1);
        rhs.add_trans(3, 102, 2);
        rhs.add_trans(4, 115, 3);
        rhs.add_trans(5, 102, 2);
        rhs.add_trans(5, 112, 0);
        rhs.add_trans(5, 115, 3);
        rhs.add_trans(5, 116, 1);

        result = concatenate(lhs, rhs);
        CHECK(result.initial[5]);
        // TODO: Add more checks.
    }
}

TEST_CASE("Mata::Nfa::concatenate() over epsilon symbol") {
    Nfa lhs{};
    Nfa rhs{};
    Nfa result{};

    SECTION("Empty automaton") {
        lhs.increase_size(1);
        rhs.increase_size(1);
        result = concatenate(lhs, rhs, true);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty language") {
        lhs.increase_size(1);
        lhs.initial.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.states_number() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.has_no_transitions());
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final.empty());
        CHECK(result.states_number() == 2);
        CHECK(result.get_num_of_trans() == 1);
        CHECK(result.has_trans(0, EPSILON, 1));
    }

    SECTION("Single state automata accepting an empty string")
    {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(1);
        rhs.initial.add(0);
        rhs.final.add(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[1]);
        CHECK(result.states_number() == 2);
        CHECK(result.get_num_of_trans() == 1);
        CHECK(result.has_trans(0, EPSILON, 1));
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.states_number() == 3);
        CHECK(result.get_num_of_trans() == 1);
        CHECK(result.has_trans(0, EPSILON, 1));
    }

    SECTION("Simple two state rhs automaton")
    {
        lhs.increase_size(1);
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.states_number() == 3);
        CHECK(result.get_num_of_trans() == 2);
        CHECK(result.has_trans(1, 'a', 2));
        CHECK(result.has_trans(0, EPSILON, 1));
    }

    SECTION("Simple two state automata")
    {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(2);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[3]);
        CHECK(result.states_number() == 4);
        CHECK(result.get_num_of_trans() == 3);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(2, 'a', 3));
        CHECK(result.has_trans(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', EPSILON, 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state automata with higher state num for non-final state")
    {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(4);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.add_trans(0, 'a', 1);
        rhs.add_trans(0, 'c', 3);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[3]);
        CHECK(result.states_number() == 6);
        CHECK(result.get_num_of_trans() == 4);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(2, 'a', 3));
        CHECK(result.has_trans(2, 'c', 5));
        CHECK(result.has_trans(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', EPSILON, 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton")
    {
        lhs.increase_size(2);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(1);
        rhs.initial.add(0);
        rhs.final.add(0);
        rhs.add_trans(0, 'a', 0);

        StateToStateMap lhs_map{};
        StateToStateMap rhs_map{};
        result = concatenate(lhs, rhs, true, &lhs_map, &rhs_map);

        CHECK(lhs_map.empty());
        CHECK(rhs_map == StateToStateMap{ { 0, 2 } });

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.states_number() == 3);
        CHECK(result.get_num_of_trans() == 3);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(2, 'a', 2));
        CHECK(result.has_trans(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', EPSILON }) != shortest_words.end());
    }

    SECTION("Automaton A concatenate automaton B")
    {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial.size() == 2);
        CHECK(result.initial[1]);
        CHECK(result.initial[3]);

        CHECK(result.states_number() == 26);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Automaton B concatenate automaton A")
    {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(rhs, lhs, true);

        CHECK(result.states_number() == 26);

        CHECK(result.initial.size() == 1);
        CHECK(result.initial[4]);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
    }
}
