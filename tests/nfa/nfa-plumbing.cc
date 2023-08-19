/* tests-nfa-plumbing.cc -- Tests plumbing versions of functions
 *
 * Copyright (c) 2023 VeriFIT & Friends
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
#include "mata/nfa/plumbing.hh"

using Symbol = Mata::Symbol;
using OnTheFlyAlphabet = Mata::OnTheFlyAlphabet;

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

TEST_CASE("Mata::Nfa::Plumbing") {
    Mata::Nfa::Nfa lhs{};
    Mata::Nfa::Nfa rhs{};
    Mata::Nfa::Nfa result{};
    OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b", "c" } };

    SECTION("Mata::Nfa::Plumbing::concatenate") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        Mata::Nfa::Plumbing::concatenate(&result, lhs, rhs);
        CHECK(is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::intersection") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        Mata::Nfa::Plumbing::intersection(&result, lhs, rhs);
        CHECK(is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::union") {
        FILL_WITH_AUT_A(lhs);
        FILL_WITH_AUT_B(lhs);
        Mata::Nfa::Plumbing::uni(&result, lhs, rhs);
        CHECK(!is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::remove_epsilon") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::remove_epsilon(&result, lhs);
        CHECK(!is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::revert") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::revert(&result, lhs);
        CHECK(!is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::reduce") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::reduce(&result, lhs);
        CHECK(!is_lang_empty(result));
        CHECK(result.size() <= lhs.size());
    }

    SECTION("Mata::Nfa::Plumbing::determinize") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::determinize(&result, lhs);
        CHECK(!is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::minimize") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::minimize(&result, lhs);
        CHECK(!is_lang_empty(result));
    }

    SECTION("Mata::Nfa::Plumbing::complement") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::complement(&result, lhs, alph);
        CHECK(!is_lang_empty(result));
    }
    SECTION("Mata::Nfa::Plumbing::make_complete") {
        FILL_WITH_AUT_A(lhs);
        Mata::Nfa::Plumbing::make_complete(&lhs, alph, lhs.size() + 1);
        CHECK(!is_lang_empty(lhs));
    }
}






