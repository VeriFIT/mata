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
#include "mata/re2parser.hh"

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::Strings::SegNfa;
using namespace Mata::util;
using namespace Mata::Parser;
using namespace Mata::RE2Parser;

using Symbol = Mata::Symbol;
using Word = std::vector<Symbol>;

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
        aut.delta.add(1, 'a', 2);

        REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{'a'}});
    }

    SECTION("Single state automaton")
    {
        aut.initial = {1 };
        aut.final = {1 };
        aut.delta.add(1, 'a', 1);

        REQUIRE(get_shortest_words(aut) == std::set<Word>{Word{}});
    }

    SECTION("Require FIFO queue")
    {
        aut.initial = {1 };
        aut.final = {4 };
        aut.delta.add(1, 'a', 5);
        aut.delta.add(5, 'c', 4);
        aut.delta.add(1, 'a', 2);
        aut.delta.add(2, 'b', 3);
        aut.delta.add(3, 'b', 4);

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

TEST_CASE("Mata::Strings::get_lengths()") {
     
    SECTION("basic") {
        Nfa x;
        create_nfa(&x, "(abcde)*");
        x.trim();
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0,5}}));
    }

    SECTION("basic2") {
        Nfa x;
        create_nfa(&x, "a+");
        x.trim();
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{1,1}}));
    }

    SECTION("basic3") {
        Nfa x;
        create_nfa(&x, "a*");
        x.trim();
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0,1}}));
    }

    SECTION("empty") {
        Nfa x;
        create_nfa(&x, "");
        x.trim();
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{0,0}}));
    }

    SECTION("finite") {
        Nfa x;
        create_nfa(&x, "abcd");
        x.trim();
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({{4,0}}));
    }

    SECTION("advanced 1") {
        Nfa x;
        create_nfa(&x, "(cd(abcde)*)|(a(aaa)*)");
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({
            {1,0}, {2,15}, {4,15}, {7,15}, {10,15}, {12,15}, {13,15}, {16,15}
        }));
    }

    SECTION("advanced 2") {
        Nfa x;
        create_nfa(&x, "a(aaaa|aaaaaaa)*");
        CHECK(get_word_lengths(x) == std::set<std::pair<int, int>>({
            {1,0}, {5,0}, {8,0}, {9,0}, {12,0}, {13,0}, {15,0}, {16,0}, 
            {17,0}, {19,0}, {20,0}, {21,0}, {22,0}, {23,0}, {24,0}, {25,0}, 
            {26,1}
        }));
    }
}
