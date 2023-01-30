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
#include <mata/re2parser.hh>

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::util;
using namespace Mata::Parser;

using Symbol = Mata::Symbol;

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

TEST_CASE("Mata::Nfa::concatenate()") {
    Nfa lhs{};
    Nfa rhs{};
    Nfa result{};

    SECTION("Empty automaton without states") {
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states") {
        rhs.add_state();
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states") {
        lhs.add_state();
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states with other with initial states") {
        lhs.add_state();
        lhs.initial.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other with initial states") {
        rhs.add_state();
        rhs.initial.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states with other non-empty automaton") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other non-empty automaton") {
        rhs.add_state();
        rhs.initial.add(0);
        rhs.final.add(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty automaton") {
        lhs.add_state();
        rhs.add_state();
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty language") {
        lhs.add_state();
        lhs.initial.add(0);
        rhs.add_state();
        rhs.initial.add(0);

        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state();
        rhs.initial.add(0);

        result = concatenate(lhs, rhs);
        CHECK(is_lang_empty(result));
    }

    SECTION("Single state automata accepting an empty string") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state();
        rhs.initial.add(0);
        rhs.final.add(0);

        result = concatenate(lhs, rhs);

        CHECK(!is_lang_empty(result));
        CHECK(is_in_lang(result, Run{ {}, {} }));
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);

        result = concatenate(lhs, rhs);

        CHECK(!result.initial.empty());
        CHECK(!result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Simple two state rhs automaton") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(!result.initial.empty());
        CHECK(!result.final.empty());
    }

    SECTION("Simple two state automata") {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(!result.initial.empty());
        CHECK(!result.final.empty());

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state automata with higher state num for non-final state") {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(3);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(0, 'c', 3);

        result = concatenate(lhs, rhs);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton") {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state();
        rhs.initial.add(0);
        rhs.final.add(0);
        rhs.delta.add(0, 'a', 0);

        result = concatenate(lhs, rhs);
        CHECK(is_in_lang(result, Run{ { 'b' }, {} }));
        CHECK(is_in_lang(result, Run{ { 'b', 'a' }, {} }));
        CHECK(is_in_lang(result, Run{ { 'b', 'a', 'a' }, {} }));
        CHECK(!is_in_lang(result, Run{ { 'a' }, {} }));
        CHECK(!is_in_lang(result, Run{ { 'a', 'b' }, {} }));
        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b' }) != shortest_words.end());
    }

    SECTION("Automaton A concatenate automaton B") {
        lhs.add_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.add_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(lhs, rhs);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Automaton B concatenate automaton A") {
        lhs.add_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.add_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(rhs, lhs);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Sample automata") {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        lhs.delta.add(0, 58, 0);
        lhs.delta.add(0, 65, 0);
        lhs.delta.add(0, 102, 0);
        lhs.delta.add(0, 112, 0);
        lhs.delta.add(0, 115, 0);
        lhs.delta.add(0, 116, 0);

        rhs.add_state(5);
        rhs.final.add({0, 5});
        rhs.initial.add(5);
        rhs.delta.add(1, 112, 0);
        rhs.delta.add(2, 116, 1);
        rhs.delta.add(3, 102, 2);
        rhs.delta.add(4, 115, 3);
        rhs.delta.add(5, 102, 2);
        rhs.delta.add(5, 112, 0);
        rhs.delta.add(5, 115, 3);
        rhs.delta.add(5, 116, 1);

        result = concatenate(lhs, rhs);
        CHECK(!is_lang_empty(result));
        // TODO: Add more checks.
    }
}

TEST_CASE("Mata::Nfa::concatenate() over epsilon symbol") {
    Nfa lhs{};
    Nfa rhs{};
    Nfa result{};

    SECTION("Empty automaton") {
        lhs.add_state();
        rhs.add_state();
        result = concatenate(lhs, rhs, true);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty language") {
        lhs.add_state();
        lhs.initial.add(0);
        rhs.add_state();
        rhs.initial.add(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state();
        rhs.initial.add(0);

        result = concatenate(lhs, rhs, true);
        CHECK(is_lang_empty(result));
    }

    SECTION("Single state automata accepting an empty string")
    {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state();
        rhs.initial.add(0);
        rhs.final.add(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[1]);
        CHECK(result.size() == 2);
        CHECK(result.get_num_of_trans() == 1);
        CHECK(result.delta.contains(0, EPSILON, 1));
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.size() == 3);
        CHECK(result.get_num_of_trans() == 1);
        CHECK(result.delta.contains(0, EPSILON, 1));
    }

    SECTION("Simple two state rhs automaton")
    {
        lhs.add_state();
        lhs.initial.add(0);
        lhs.final.add(0);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.size() == 3);
        CHECK(result.get_num_of_trans() == 2);
        CHECK(result.delta.contains(1, 'a', 2));
        CHECK(result.delta.contains(0, EPSILON, 1));
    }

    SECTION("Simple two state automata")
    {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(1);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[3]);
        CHECK(result.size() == 4);
        CHECK(result.get_num_of_trans() == 3);
        CHECK(result.delta.contains(0, 'b', 1));
        CHECK(result.delta.contains(2, 'a', 3));
        CHECK(result.delta.contains(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', EPSILON, 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state automata with higher state num for non-final state")
    {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(3);
        rhs.initial.add(0);
        rhs.final.add(1);
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(0, 'c', 3);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial[0]);
        CHECK(result.final[3]);
        CHECK(result.size() == 6);
        CHECK(result.get_num_of_trans() == 4);
        CHECK(result.delta.contains(0, 'b', 1));
        CHECK(result.delta.contains(2, 'a', 3));
        CHECK(result.delta.contains(2, 'c', 5));
        CHECK(result.delta.contains(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', EPSILON, 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton")
    {
        lhs.add_state(1);
        lhs.initial.add(0);
        lhs.final.add(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state();
        rhs.initial.add(0);
        rhs.final.add(0);
        rhs.delta.add(0, 'a', 0);

        StateToStateMap lhs_map{};
        StateToStateMap rhs_map{};
        result = concatenate(lhs, rhs, true, &lhs_map, &rhs_map);

        CHECK(rhs_map == StateToStateMap{ { 0, 2 } });

        CHECK(result.initial[0]);
        CHECK(result.final[2]);
        CHECK(result.size() == 3);
        CHECK(result.get_num_of_trans() == 3);
        CHECK(result.delta.contains(0, 'b', 1));
        CHECK(result.delta.contains(2, 'a', 2));
        CHECK(result.delta.contains(1, EPSILON, 2));

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b' }) != shortest_words.end());
    }

    SECTION("Automaton A concatenate automaton B")
    {
        lhs.add_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.add_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(lhs, rhs, true);

        CHECK(result.initial.size() == 2);
        CHECK(result.initial[1]);
        CHECK(result.initial[3]);

        CHECK(result.size() == 26);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', EPSILON, 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Automaton B concatenate automaton A")
    {
        lhs.add_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.add_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(rhs, lhs, true);

        CHECK(result.size() == 26);

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

TEST_CASE("(a|b)*") {
    Nfa aut1;
    Mata::RE2Parser::create_nfa(&aut1, "a*");
    Nfa aut2;
    Mata::RE2Parser::create_nfa(&aut2, "b*");
    Nfa aut3;
    Mata::RE2Parser::create_nfa(&aut3, "a*b*");
    auto concatenated_aut{ concatenate(aut1, aut2) };
    CHECK(are_equivalent(concatenated_aut, aut3));
}
