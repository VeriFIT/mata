/* tests-nfa-strings.cc -- Tests for string solving operations.
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

#include "catch.hpp"

#include "mata/nfa.hh"
#include "mata/nfa-strings.hh"

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::Strings::SegNfa;
using namespace Mata::util;
using namespace Mata::Parser;

using Word = std::vector<Symbol>;

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


TEST_CASE("Mata::Nfa::get_shortest_words()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        Word word{};
        word.push_back('b');
        word.push_back('a');
        WordSet expected{word};
        Word word2{};
        word2.push_back('a');
        word2.push_back('a');
        expected.insert(expected.begin(), word2);
        REQUIRE(get_shortest_words(aut) == expected);

        SECTION("Additional initial state with longer words")
        {
            aut.initial.add(8);
            REQUIRE(get_shortest_words(aut) == expected);
        }

        SECTION("Change initial state")
        {
            aut.initial.clear();
            aut.initial.add(8);

            word.clear();
            word.push_back('b');
            word.push_back('b');
            word.push_back('a');
            expected = WordSet{word};
            word2.clear();
            word2.push_back('b');
            word2.push_back('a');
            word2.push_back('a');
            expected.insert(expected.begin(), word2);

            REQUIRE(get_shortest_words(aut) == expected);
        }
    }

    SECTION("Empty automaton")
    {
        REQUIRE(get_shortest_words(aut).empty());
    }

    SECTION("One-state automaton accepting an empty language")
    {
        aut.initial.add(0);
        REQUIRE(get_shortest_words(aut).empty());
        aut.final.add(1);
        REQUIRE(get_shortest_words(aut).empty());
        aut.final.add(0);
        REQUIRE(get_shortest_words(aut) == WordSet{Word{}});
    }

    SECTION("Automaton A")
    {
        FILL_WITH_AUT_A(aut);
        Word word{};
        word.push_back('b');
        word.push_back('a');
        std::set<Word> expected{word};
        Word word2{};
        word2.push_back('a');
        word2.push_back('a');
        expected.insert(expected.begin(), word2);
        REQUIRE(get_shortest_words(aut) == expected);
    }

    SECTION("Single transition automaton")
    {
        aut.initial = {1 };
        aut.final = {2 };
        aut.add_trans(1, 'a', 2);

        REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{'a'}});
    }

    SECTION("Single state automaton")
    {
        aut.initial = {1 };
        aut.final = {1 };
        aut.add_trans(1, 'a', 1);

        REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{}});
    }

    SECTION("Require FIFO queue")
    {
        aut.initial = {1 };
        aut.final = {4 };
        aut.add_trans(1, 'a', 5);
        aut.add_trans(5, 'c', 4);
        aut.add_trans(1, 'a', 2);
        aut.add_trans(2, 'b', 3);
        aut.add_trans(3, 'b', 4);

        Word word{};
        word.push_back('a');
        word.push_back('c');
        std::set<Word> expected{word};

        // LIFO queue would return as shortest words string "abb", which would be incorrect.
        REQUIRE(get_shortest_words(aut) == expected);
    }
}

TEST_CASE("Mata::Nfa::get_shortest_words() for profiling", "[.profiling][shortest_words]") {
    Nfa aut('q' + 1);
    FILL_WITH_AUT_B(aut);
    aut.initial.clear();
    aut.initial.add(8);
    Word word{};
    word.push_back('b');
    word.push_back('b');
    word.push_back('a');
    WordSet expected{ word };
    Word word2{};
    word2.push_back('b');
    word2.push_back('a');
    word2.push_back('a');
    expected.insert(expected.begin(), word2);

    for (size_t n{}; n < 100000; ++n) {
        get_shortest_words(aut);
    }
}
