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

#include "mata/nfa/nfa.hh"
#include "mata/nfa/strings.hh"
#include "mata/parser/re2parser.hh"

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::Util;
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
        lhs.initial.insert(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other with initial states") {
        rhs.add_state();
        rhs.initial.insert(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states with other non-empty automaton") {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Other empty automaton without states with other non-empty automaton") {
        rhs.add_state();
        rhs.initial.insert(0);
        rhs.final.insert(0);
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
        lhs.initial.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);

        result = concatenate(lhs, rhs);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton") {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);

        result = concatenate(lhs, rhs);
        CHECK(is_lang_empty(result));
    }

    SECTION("Single state automata accepting an empty string") {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);
        rhs.final.insert(0);

        result = concatenate(lhs, rhs);

        CHECK(!is_lang_empty(result));
        CHECK(is_in_lang(result, Run{ {}, {} }));
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton") {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);

        result = concatenate(lhs, rhs);

        CHECK(!result.initial.empty());
        CHECK(!result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Simple two state rhs automaton") {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);
        rhs.delta.add(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(!result.initial.empty());
        CHECK(!result.final.empty());
    }

    SECTION("Simple two state automata") {
        lhs.add_state(1);
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);
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
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(3);
        rhs.initial.insert(0);
        rhs.final.insert(1);
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(0, 'c', 3);

        result = concatenate(lhs, rhs);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton") {
        lhs.add_state(1);
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state();
        rhs.initial.insert(0);
        rhs.final.insert(0);
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
        lhs.initial.insert(0);
        lhs.final.insert(0);
        lhs.delta.add(0, 58, 0);
        lhs.delta.add(0, 65, 0);
        lhs.delta.add(0, 102, 0);
        lhs.delta.add(0, 112, 0);
        lhs.delta.add(0, 115, 0);
        lhs.delta.add(0, 116, 0);

        rhs.add_state(5);
        rhs.final.insert({0, 5});
        rhs.initial.insert(5);
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
        lhs.initial.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);

        result = concatenate(lhs, rhs, true);

        CHECK(result.size() == 0);
        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);

        result = concatenate(lhs, rhs, true);
        CHECK(is_lang_empty(result));
    }

    SECTION("Single state automata accepting an empty string")
    {
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state();
        rhs.initial.insert(0);
        rhs.final.insert(0);

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
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);

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
        lhs.initial.insert(0);
        lhs.final.insert(0);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);
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
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(1);
        rhs.initial.insert(0);
        rhs.final.insert(1);
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
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state(3);
        rhs.initial.insert(0);
        rhs.final.insert(1);
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
        lhs.initial.insert(0);
        lhs.final.insert(1);
        lhs.delta.add(0, 'b', 1);
        rhs.add_state();
        rhs.initial.insert(0);
        rhs.final.insert(0);
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
    Mata::Parser::create_nfa(&aut1, "a*");
    Nfa aut2;
    Mata::Parser::create_nfa(&aut2, "b*");
    Nfa aut3;
    Mata::Parser::create_nfa(&aut3, "a*b*");
    auto concatenated_aut{ concatenate(aut1, aut2) };
    CHECK(are_equivalent(concatenated_aut, aut3));
}

TEST_CASE("Bug with epsilon transitions") {
    Nfa nfa1{};
    nfa1.initial.insert(0);
    nfa1.final.insert(3);
    nfa1.delta.add(0, 97, 0);
    nfa1.delta.add(0, 98, 0);
    nfa1.delta.add(0, 99, 0);
    nfa1.delta.add(0, 100, 0);
    nfa1.delta.add(0, EPSILON, 1);
    nfa1.delta.add(1, 97, 2);
    nfa1.delta.add(2, 98, 3);

    Nfa nfa2{};
    nfa2.initial.insert(0);
    nfa2.final.insert(0);
    nfa2.delta.add(0, 97, 0);
    nfa2.delta.add(0, 98, 0);
    nfa2.delta.add(0, 99, 0);
    nfa2.delta.add(0, 100, 0);

    auto result{ concatenate(nfa1, nfa2, true) };

    Nfa expected{ nfa1 };
    expected.delta.add(3, EPSILON, 4);
    expected.delta.add(4, 97, 4);
    expected.delta.add(4, 98, 4);
    expected.delta.add(4, 99, 4);
    expected.delta.add(4, 100, 4);
    expected.final = { 4 };

    CHECK(are_equivalent(result, expected));
}

TEST_CASE("Mata::Nfa::concatenate() inplace") {


    SECTION("Empty automaton without states") {
        Nfa lhs{};
        Nfa rhs{};
        Nfa result{};
        result = lhs.concatenate(rhs);

        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("One empty automaton without states") {
        Nfa lhs{};
        Nfa rhs{};
        Nfa result{};
        rhs.add_state();
        result = lhs.concatenate(rhs);

        CHECK(result.initial.empty());
        CHECK(result.final.empty());
        CHECK(result.delta.empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Automaton A concatenate automaton B") {
        Nfa lhs{};
        Nfa rhs{};
        Nfa result{};
        lhs.add_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.add_state(14);
        FILL_WITH_AUT_B(rhs);

        result = lhs.concatenate(rhs);

        auto shortest_words{ get_shortest_words(result) };
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Sample automata") {
        Nfa lhs{};
        Nfa rhs{};
        Nfa result{};
        lhs.add_state();
        lhs.initial.insert(0);
        lhs.final.insert(0);
        lhs.delta.add(0, 58, 0);
        lhs.delta.add(0, 65, 0);
        lhs.delta.add(0, 102, 0);
        lhs.delta.add(0, 112, 0);
        lhs.delta.add(0, 115, 0);
        lhs.delta.add(0, 116, 0);

        rhs.add_state(5);
        rhs.final.insert({0, 5});
        rhs.initial.insert(5);
        rhs.delta.add(1, 112, 0);
        rhs.delta.add(2, 116, 1);
        rhs.delta.add(3, 102, 2);
        rhs.delta.add(4, 115, 3);
        rhs.delta.add(5, 102, 2);
        rhs.delta.add(5, 112, 0);
        rhs.delta.add(5, 115, 3);
        rhs.delta.add(5, 116, 1);

        result = lhs.concatenate(rhs);
        CHECK(!is_lang_empty(result));
        // TODO: Add more checks.
    }
}

TEST_CASE("Concat_inplace performance", "[.profiling]") {
    Nfa base;
    base.initial.insert(0);
    base.final.insert(4);
    base.delta.add(0, 45, 1);
    base.delta.add(0, 46, 1);
    base.delta.add(0, 48, 1);
    base.delta.add(0, 49, 1);
    base.delta.add(0, 50, 1);
    base.delta.add(0, 51, 1);
    base.delta.add(0, 52, 1);
    base.delta.add(0, 53, 1);
    base.delta.add(0, 54, 1);
    base.delta.add(0, 55, 1);
    base.delta.add(0, 56, 1);
    base.delta.add(0, 57, 1);
    base.delta.add(0, 65, 1);
    base.delta.add(0, 66, 1);
    base.delta.add(0, 67, 1);
    base.delta.add(0, 68, 1);
    base.delta.add(0, 69, 1);
    base.delta.add(0, 70, 1);
    base.delta.add(0, 71, 1);
    base.delta.add(0, 72, 1);
    base.delta.add(0, 73, 1);
    base.delta.add(0, 74, 1);
    base.delta.add(0, 75, 1);
    base.delta.add(0, 76, 1);
    base.delta.add(0, 77, 1);
    base.delta.add(0, 78, 1);
    base.delta.add(0, 79, 1);
    base.delta.add(0, 80, 1);
    base.delta.add(0, 81, 1);
    base.delta.add(0, 82, 1);
    base.delta.add(0, 83, 1);
    base.delta.add(0, 84, 1);
    base.delta.add(0, 85, 1);
    base.delta.add(0, 86, 1);
    base.delta.add(0, 87, 1);
    base.delta.add(0, 88, 1);
    base.delta.add(0, 89, 1);
    base.delta.add(0, 90, 1);
    base.delta.add(0, 95, 1);
    base.delta.add(0, 97, 1);
    base.delta.add(0, 98, 1);
    base.delta.add(0, 99, 1);
    base.delta.add(0, 100, 1);
    base.delta.add(0, 101, 1);
    base.delta.add(0, 102, 1);
    base.delta.add(0, 103, 1);
    base.delta.add(0, 104, 1);
    base.delta.add(0, 105, 1);
    base.delta.add(0, 106, 1);
    base.delta.add(0, 107, 1);
    base.delta.add(0, 108, 1);
    base.delta.add(0, 109, 1);
    base.delta.add(0, 110, 1);
    base.delta.add(0, 111, 1);
    base.delta.add(0, 112, 1);
    base.delta.add(0, 113, 1);
    base.delta.add(0, 114, 1);
    base.delta.add(0, 115, 1);
    base.delta.add(0, 116, 1);
    base.delta.add(0, 117, 1);
    base.delta.add(0, 118, 1);
    base.delta.add(0, 119, 1);
    base.delta.add(0, 120, 1);
    base.delta.add(0, 121, 1);
    base.delta.add(0, 122, 1);
    base.delta.add(0, 124, 1);
    base.delta.add(1, 45, 2);
    base.delta.add(1, 46, 2);
    base.delta.add(1, 48, 2);
    base.delta.add(1, 49, 2);
    base.delta.add(1, 50, 2);
    base.delta.add(1, 51, 2);
    base.delta.add(1, 52, 2);
    base.delta.add(1, 53, 2);
    base.delta.add(1, 54, 2);
    base.delta.add(1, 55, 2);
    base.delta.add(1, 56, 2);
    base.delta.add(1, 57, 2);
    base.delta.add(1, 65, 2);
    base.delta.add(1, 66, 2);
    base.delta.add(1, 67, 2);
    base.delta.add(1, 68, 2);
    base.delta.add(1, 69, 2);
    base.delta.add(1, 70, 2);
    base.delta.add(1, 71, 2);
    base.delta.add(1, 72, 2);
    base.delta.add(1, 73, 2);
    base.delta.add(1, 74, 2);
    base.delta.add(1, 75, 2);
    base.delta.add(1, 76, 2);
    base.delta.add(1, 77, 2);
    base.delta.add(1, 78, 2);
    base.delta.add(1, 79, 2);
    base.delta.add(1, 80, 2);
    base.delta.add(1, 81, 2);
    base.delta.add(1, 82, 2);
    base.delta.add(1, 83, 2);
    base.delta.add(1, 84, 2);
    base.delta.add(1, 85, 2);
    base.delta.add(1, 86, 2);
    base.delta.add(1, 87, 2);
    base.delta.add(1, 88, 2);
    base.delta.add(1, 89, 2);
    base.delta.add(1, 90, 2);
    base.delta.add(1, 95, 2);
    base.delta.add(1, 97, 2);
    base.delta.add(1, 98, 2);
    base.delta.add(1, 99, 2);
    base.delta.add(1, 100, 2);
    base.delta.add(1, 101, 2);
    base.delta.add(1, 102, 2);
    base.delta.add(1, 103, 2);
    base.delta.add(1, 104, 2);
    base.delta.add(1, 105, 2);
    base.delta.add(1, 106, 2);
    base.delta.add(1, 107, 2);
    base.delta.add(1, 108, 2);
    base.delta.add(1, 109, 2);
    base.delta.add(1, 110, 2);
    base.delta.add(1, 111, 2);
    base.delta.add(1, 112, 2);
    base.delta.add(1, 113, 2);
    base.delta.add(1, 114, 2);
    base.delta.add(1, 115, 2);
    base.delta.add(1, 116, 2);
    base.delta.add(1, 117, 2);
    base.delta.add(1, 118, 2);
    base.delta.add(1, 119, 2);
    base.delta.add(1, 120, 2);
    base.delta.add(1, 121, 2);
    base.delta.add(1, 122, 2);
    base.delta.add(1, 124, 2);
    base.delta.add(2, 45, 3);
    base.delta.add(2, 46, 3);
    base.delta.add(2, 48, 3);
    base.delta.add(2, 49, 3);
    base.delta.add(2, 50, 3);
    base.delta.add(2, 51, 3);
    base.delta.add(2, 52, 3);
    base.delta.add(2, 53, 3);
    base.delta.add(2, 54, 3);
    base.delta.add(2, 55, 3);
    base.delta.add(2, 56, 3);
    base.delta.add(2, 57, 3);
    base.delta.add(2, 65, 3);
    base.delta.add(2, 66, 3);
    base.delta.add(2, 67, 3);
    base.delta.add(2, 68, 3);
    base.delta.add(2, 69, 3);
    base.delta.add(2, 70, 3);
    base.delta.add(2, 71, 3);
    base.delta.add(2, 72, 3);
    base.delta.add(2, 73, 3);
    base.delta.add(2, 74, 3);
    base.delta.add(2, 75, 3);
    base.delta.add(2, 76, 3);
    base.delta.add(2, 77, 3);
    base.delta.add(2, 78, 3);
    base.delta.add(2, 79, 3);
    base.delta.add(2, 80, 3);
    base.delta.add(2, 81, 3);
    base.delta.add(2, 82, 3);
    base.delta.add(2, 83, 3);
    base.delta.add(2, 84, 3);
    base.delta.add(2, 85, 3);
    base.delta.add(2, 86, 3);
    base.delta.add(2, 87, 3);
    base.delta.add(2, 88, 3);
    base.delta.add(2, 89, 3);
    base.delta.add(2, 90, 3);
    base.delta.add(2, 95, 3);
    base.delta.add(2, 97, 3);
    base.delta.add(2, 98, 3);
    base.delta.add(2, 99, 3);
    base.delta.add(2, 100, 3);
    base.delta.add(2, 101, 3);
    base.delta.add(2, 102, 3);
    base.delta.add(2, 103, 3);
    base.delta.add(2, 104, 3);
    base.delta.add(2, 105, 3);
    base.delta.add(2, 106, 3);
    base.delta.add(2, 107, 3);
    base.delta.add(2, 108, 3);
    base.delta.add(2, 109, 3);
    base.delta.add(2, 110, 3);
    base.delta.add(2, 111, 3);
    base.delta.add(2, 112, 3);
    base.delta.add(2, 113, 3);
    base.delta.add(2, 114, 3);
    base.delta.add(2, 115, 3);
    base.delta.add(2, 116, 3);
    base.delta.add(2, 117, 3);
    base.delta.add(2, 118, 3);
    base.delta.add(2, 119, 3);
    base.delta.add(2, 120, 3);
    base.delta.add(2, 121, 3);
    base.delta.add(2, 122, 3);
    base.delta.add(2, 124, 3);
    base.delta.add(3, 45, 4);
    base.delta.add(3, 46, 4);
    base.delta.add(3, 48, 4);
    base.delta.add(3, 49, 4);
    base.delta.add(3, 50, 4);
    base.delta.add(3, 51, 4);
    base.delta.add(3, 52, 4);
    base.delta.add(3, 53, 4);
    base.delta.add(3, 54, 4);
    base.delta.add(3, 55, 4);
    base.delta.add(3, 56, 4);
    base.delta.add(3, 57, 4);
    base.delta.add(3, 65, 4);
    base.delta.add(3, 66, 4);
    base.delta.add(3, 67, 4);
    base.delta.add(3, 68, 4);
    base.delta.add(3, 69, 4);
    base.delta.add(3, 70, 4);
    base.delta.add(3, 71, 4);
    base.delta.add(3, 72, 4);
    base.delta.add(3, 73, 4);
    base.delta.add(3, 74, 4);
    base.delta.add(3, 75, 4);
    base.delta.add(3, 76, 4);
    base.delta.add(3, 77, 4);
    base.delta.add(3, 78, 4);
    base.delta.add(3, 79, 4);
    base.delta.add(3, 80, 4);
    base.delta.add(3, 81, 4);
    base.delta.add(3, 82, 4);
    base.delta.add(3, 83, 4);
    base.delta.add(3, 84, 4);
    base.delta.add(3, 85, 4);
    base.delta.add(3, 86, 4);
    base.delta.add(3, 87, 4);
    base.delta.add(3, 88, 4);
    base.delta.add(3, 89, 4);
    base.delta.add(3, 90, 4);
    base.delta.add(3, 95, 4);
    base.delta.add(3, 97, 4);
    base.delta.add(3, 98, 4);
    base.delta.add(3, 99, 4);
    base.delta.add(3, 100, 4);
    base.delta.add(3, 101, 4);
    base.delta.add(3, 102, 4);
    base.delta.add(3, 103, 4);
    base.delta.add(3, 104, 4);
    base.delta.add(3, 105, 4);
    base.delta.add(3, 106, 4);
    base.delta.add(3, 107, 4);
    base.delta.add(3, 108, 4);
    base.delta.add(3, 109, 4);
    base.delta.add(3, 110, 4);
    base.delta.add(3, 111, 4);
    base.delta.add(3, 112, 4);
    base.delta.add(3, 113, 4);
    base.delta.add(3, 114, 4);
    base.delta.add(3, 115, 4);
    base.delta.add(3, 116, 4);
    base.delta.add(3, 117, 4);
    base.delta.add(3, 118, 4);
    base.delta.add(3, 119, 4);
    base.delta.add(3, 120, 4);
    base.delta.add(3, 121, 4);
    base.delta.add(3, 122, 4);
    base.delta.add(3, 124, 4);

    Nfa concat;
    concat.initial.insert(1);
    concat.final.insert(0);
    concat.final.insert(1);
    concat.delta.add(1, 45, 0);
    concat.delta.add(1, 46, 0);
    concat.delta.add(1, 48, 0);
    concat.delta.add(1, 49, 0);
    concat.delta.add(1, 50, 0);
    concat.delta.add(1, 51, 0);
    concat.delta.add(1, 52, 0);
    concat.delta.add(1, 53, 0);
    concat.delta.add(1, 54, 0);
    concat.delta.add(1, 55, 0);
    concat.delta.add(1, 56, 0);
    concat.delta.add(1, 57, 0);
    concat.delta.add(1, 65, 0);
    concat.delta.add(1, 66, 0);
    concat.delta.add(1, 67, 0);
    concat.delta.add(1, 68, 0);
    concat.delta.add(1, 69, 0);
    concat.delta.add(1, 70, 0);
    concat.delta.add(1, 71, 0);
    concat.delta.add(1, 72, 0);
    concat.delta.add(1, 73, 0);
    concat.delta.add(1, 74, 0);
    concat.delta.add(1, 75, 0);
    concat.delta.add(1, 76, 0);
    concat.delta.add(1, 77, 0);
    concat.delta.add(1, 78, 0);
    concat.delta.add(1, 79, 0);
    concat.delta.add(1, 80, 0);
    concat.delta.add(1, 81, 0);
    concat.delta.add(1, 82, 0);
    concat.delta.add(1, 83, 0);
    concat.delta.add(1, 84, 0);
    concat.delta.add(1, 85, 0);
    concat.delta.add(1, 86, 0);
    concat.delta.add(1, 87, 0);
    concat.delta.add(1, 88, 0);
    concat.delta.add(1, 89, 0);
    concat.delta.add(1, 90, 0);
    concat.delta.add(1, 95, 0);
    concat.delta.add(1, 97, 0);
    concat.delta.add(1, 98, 0);
    concat.delta.add(1, 99, 0);
    concat.delta.add(1, 100, 0);
    concat.delta.add(1, 101, 0);
    concat.delta.add(1, 102, 0);
    concat.delta.add(1, 103, 0);
    concat.delta.add(1, 104, 0);
    concat.delta.add(1, 105, 0);
    concat.delta.add(1, 106, 0);
    concat.delta.add(1, 107, 0);
    concat.delta.add(1, 108, 0);
    concat.delta.add(1, 109, 0);
    concat.delta.add(1, 110, 0);
    concat.delta.add(1, 111, 0);
    concat.delta.add(1, 112, 0);
    concat.delta.add(1, 113, 0);
    concat.delta.add(1, 114, 0);
    concat.delta.add(1, 115, 0);
    concat.delta.add(1, 116, 0);
    concat.delta.add(1, 117, 0);
    concat.delta.add(1, 118, 0);
    concat.delta.add(1, 119, 0);
    concat.delta.add(1, 120, 0);
    concat.delta.add(1, 121, 0);
    concat.delta.add(1, 122, 0);
    concat.delta.add(1, 124, 0);

    for (auto i=0;i<1000;i++) {
        base.concatenate(concat);
    }
}
