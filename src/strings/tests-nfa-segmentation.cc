/* tests-nfa-segmentation.cc -- Tests for segmentation of NFAs
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
	x.initialstates = {4}; \
	x.finalstates = {2, 12}; \
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

TEST_CASE("Mata::Nfa::Segmentation::get_epsilon_depths()")
{
    Nfa aut('q' + 1);
    constexpr Symbol epsilon{'c'};
    const std::set<Symbol> epsilons({epsilon});

    SECTION("Automaton A")
    {
        FILL_WITH_AUT_A(aut);
        auto segmentation{SegNfa::Segmentation{aut, epsilons } };
        const auto& epsilon_depth_transitions{ segmentation.get_epsilon_depths() };
        REQUIRE(epsilon_depth_transitions == SegNfa::Segmentation::EpsilonDepthTransitions{{0, std::vector<Trans>{
                {10, epsilon, 7}, {7, epsilon, 3}, {5, epsilon, 9}}
       }});
    }

    SECTION("Small automaton with depths")
    {
        aut.initial.add(1);
        aut.final.add(8);
        aut.delta.add(1, epsilon, 2);
        aut.delta.add(2, 'a', 3);
        aut.delta.add(2, 'b', 4);
        aut.delta.add(3, 'b', 6);
        aut.delta.add(4, 'a', 6);
        aut.delta.add(6, epsilon, 7);
        aut.delta.add(7, epsilon, 8);

        auto segmentation{SegNfa::Segmentation{aut, epsilons } };
        const auto& epsilon_depth_transitions{ segmentation.get_epsilon_depths() };

        REQUIRE(epsilon_depth_transitions == SegNfa::Segmentation::EpsilonDepthTransitions{
                {0, TransSequence{{1, epsilon, 2}}},
                {1, TransSequence{{6, epsilon, 7}}},
                {2, TransSequence{{7, epsilon, 8}}},
        });
    }

}

TEST_CASE("Mata::Nfa::Segmentation::split_segment_automaton()") {
    Symbol epsilon{ 'c' };
    const std::set<Symbol> epsilons({epsilon}); 
    SECTION("Large automaton") {
        Nfa aut(100);
        aut.initial.add(1);
        aut.final.add(11);
        aut.delta.add(1, 'a', 2);
        aut.delta.add(1, 'b', 3);
        aut.delta.add(3, 'c', 4);
        aut.delta.add(4, 'a', 7);
        aut.delta.add(7, 'b', 8);
        aut.delta.add(8, 'a', 7);
        aut.delta.add(8, 'b', 4);
        aut.delta.add(4, 'c', 5);
        aut.delta.add(5, 'a', 6);
        aut.delta.add(5, 'b', 6);
        aut.delta.add(6, 'c', 10);
        aut.delta.add(9, 'a', 11);
        aut.delta.add(10, 'b', 11);

        auto segmentation{SegNfa::Segmentation{aut, epsilons}};
        auto segments{ segmentation.get_segments() };
        REQUIRE(segments.size() == 4);

        REQUIRE(segments[0].initial[0]);
        REQUIRE(segments[0].final[1]);
        REQUIRE(segments[0].delta.contains(0, 'b', 1));
        REQUIRE(!segments[0].delta.contains(0, 'a', 2));

        REQUIRE(segments[1].initial[0]);
        REQUIRE(segments[1].final[0]);
        REQUIRE(segments[1].delta.contains(0, 'a', 1));
        REQUIRE(!segments[1].delta.contains(0, 'a', 2));
        REQUIRE(!segments[1].delta.contains(0, 'c', 3));
        REQUIRE(segments[1].delta.contains(1, 'b', 2));
        REQUIRE(segments[1].delta.contains(2, 'b', 0));
        REQUIRE(segments[1].delta.contains(2, 'a', 1));

        REQUIRE(segments[2].initial[0]);
        REQUIRE(segments[2].final[1]);
        REQUIRE(segments[2].delta.contains(0, 'a', 1));
        REQUIRE(segments[2].delta.contains(0, 'b', 1));

        REQUIRE(segments[3].initial[0]);
        REQUIRE(segments[3].final[1]);
        REQUIRE(segments[3].delta.contains(0, 'b', 1));
    }

    SECTION("Correctly make states final and initial") {
        Nfa aut(100);
        aut.initial.add(0);
        aut.final.add({4, 6});
        aut.delta.add(0, epsilon, 2);
        aut.delta.add(0, 'a', 1);
        aut.delta.add(1, epsilon, 3);
        aut.delta.add(3, 'b', 5);
        aut.delta.add(2, epsilon, 4);
        aut.delta.add(5, epsilon, 6);

        auto segmentation{SegNfa::Segmentation{aut, epsilons}};
        auto segments{ segmentation.get_segments() };
        CHECK(segments.size() == 3);

        CHECK(segments[0].initial.size() == 1);
        CHECK(segments[0].initial[0]);
        CHECK(segments[0].final.size() == 2);
        CHECK(segments[0].final[0]);
        CHECK(segments[0].final[1]);
        CHECK(segments[0].get_num_of_trans() == 1);
        CHECK(segments[0].delta.contains(0, 'a', 1));

        CHECK(segments[1].initial.size() == 2);
        CHECK(segments[1].initial[0]);
        CHECK(segments[1].initial[1]);
        CHECK(segments[1].final.size() == 2);
        CHECK(segments[1].final[0]);
        CHECK(segments[1].final[2]);
        CHECK(segments[1].get_num_of_trans() == 1);
        CHECK(segments[1].delta.contains(1, 'b', 2));

        CHECK(segments[2].initial.size() == 2);
        CHECK(segments[2].initial[0]);
        CHECK(segments[2].initial[1]);
        CHECK(segments[2].final.size() == 2);
        CHECK(segments[2].final[0]);
        CHECK(segments[2].final[1]);
        CHECK(segments[2].get_num_of_trans() == 0);
    }
}
