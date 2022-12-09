// TODO: some header

#include <unordered_set>

#include "catch.hpp"

#include "mata/nfa.hh"
#include "mata/nfa-strings.hh"

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::util;
using namespace Mata::Parser;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x) \
	x.initialstates = {1, 3}; \
	x.finalstates = {5}; \
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
	x.initialstates = {4}; \
	x.finalstates = {2, 12}; \
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

template<class T> void unused(const T &) {}

TEST_CASE("Mata::Nfa::SegNfa::noodlify()")
{
    Nfa aut{20};

    SECTION("Small automaton") {
        aut.initial.add(0);
        aut.final.add(1);
        aut.add_trans(0, 'a', 1);

        Nfa noodle{2 };
        noodle.initial.add(0);
        noodle.add_trans(0, 'a', 1);
        noodle.final.add(1);
        auto result{ SegNfa::noodlify(aut, 'c') };
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 1);
        CHECK(are_equivalent(*result[0][0], noodle));

        auto result_segments{ SegNfa::noodlify_for_equation({ aut } , aut) };
        REQUIRE(result_segments.size() == 1);
        REQUIRE(result_segments[0].size() == 1);
        CHECK(are_equivalent(*result_segments[0][0], noodle));
    }

    SECTION("1-2-3 epsilon transitions")
    {
        aut.initial.add(0);
        aut.final.add({4, 5, 6, 7});
        aut.add_trans(0, 'e', 1);
        aut.add_trans(1, 'e', 2);
        aut.add_trans(1, 'e', 3);
        aut.add_trans(2, 'e', 4);
        aut.add_trans(2, 'e', 5);
        aut.add_trans(2, 'e', 6);
        aut.add_trans(3, 'e', 7);

        auto noodles{ SegNfa::noodlify(aut, 'e') };

        CHECK(noodles.size() == 4);
    }

    SECTION("6-5-6 epsilon transitions")
    {
        aut.initial.add({0, 1, 2});
        aut.final.add({11, 12, 13, 14, 15, 16});
        aut.add_trans(0, 'e', 3);
        aut.add_trans(0, 'e', 4);
        aut.add_trans(0, 'e', 5);
        aut.add_trans(1, 'e', 3);
        aut.add_trans(1, 'e', 4);
        aut.add_trans(2, 'e', 5);

        aut.add_trans(3, 'e', 6);
        aut.add_trans(3, 'e', 7);
        aut.add_trans(4, 'e', 8);
        aut.add_trans(4, 'e', 9);
        aut.add_trans(5, 'e', 10);

        aut.add_trans(6, 'e', 11);
        aut.add_trans(7, 'e', 12);
        aut.add_trans(8, 'e', 13);
        aut.add_trans(8, 'e', 14);
        aut.add_trans(9, 'e', 15);
        aut.add_trans(10, 'e', 16);

        auto noodles{ SegNfa::noodlify(aut, 'e') };

        CHECK(noodles.size() == 12);
    }

    SECTION("1-2-3-3 epsilon transitions")
    {
        aut.initial.add(0);
        aut.final.add(7);
        aut.add_trans(0, 'e', 1);

        aut.add_trans(1, 'e', 2);
        aut.add_trans(1, 'e', 3);

        aut.add_trans(2, 'e', 4);
        aut.add_trans(3, 'e', 5);
        aut.add_trans(3, 'e', 6);

        aut.add_trans(4, 'e', 7);
        aut.add_trans(5, 'e', 7);
        aut.add_trans(6, 'e', 7);

        auto noodles{ SegNfa::noodlify(aut, 'e') };

        CHECK(noodles.size() == 3);
    }
}

TEST_CASE("Mata::Nfa::SegNfa::noodlify_for_equation()") {
    SECTION("Empty input") {
        CHECK(SegNfa::noodlify_for_equation(std::vector<std::reference_wrapper<Nfa>>{}, Nfa{}).empty());
    }

    SECTION("Empty left side") {
        Nfa right{1};
        right.initial.add(0);
        right.final.add(0);
        CHECK(SegNfa::noodlify_for_equation(std::vector<std::reference_wrapper<Nfa>>{}, right).empty());
    }

    SECTION("Empty right side") {
        Nfa left{ 1};
        left.initial.add(0);
        left.final.add(0);
        CHECK(SegNfa::noodlify_for_equation({ left }, Nfa{}).empty());
    }

    SECTION("Small automata without initial and final states") {
        Nfa left{ 1};
        Nfa right{ 2};
        CHECK(SegNfa::noodlify_for_equation({ left }, right).empty());
    }

    SECTION("Small automata") {
        Nfa left1{ 1};
        left1.initial.add(0);
        left1.final.add(0);
        Nfa left2{ 1};
        left2.initial.add(0);
        left2.final.add(0);
        Nfa right{ 2};
        right.initial.add(0);
        right.final.add(0);

        Nfa noodle{2};
        noodle.initial.add(0);
        noodle.add_trans(0, 0, 1);
        noodle.final.add(1);
        auto result{ SegNfa::noodlify_for_equation({ left1, left2 }, right) };
        REQUIRE(result.size() == 1);
        //CHECK(are_equivalent(result[0], noodle));
    }

    SECTION("Larger automata") {
        Nfa left1{ 2};
        left1.initial.add(0);
        left1.final.add(1);
        left1.add_trans(0, 'a', 1);
        Nfa left2{ 2};
        left2.initial.add(0);
        left2.final.add(1);
        left2.add_trans(0, 'b', 1);
        Nfa right_side{ 3};
        right_side.initial.add(0);
        right_side.add_trans(0, 'a', 1);
        right_side.add_trans(1, 'b', 2);
        right_side.final.add(2);

        Nfa noodle{ 4 };
        noodle.initial.add(0);
        noodle.final.add(3);
        noodle.add_trans(0, 'a', 1);
        noodle.add_trans(1, 'c', 2); // The automatically chosen epsilon symbol (one larger than 'b').
        noodle.add_trans(2, 'b', 3);
        auto result{ SegNfa::noodlify_for_equation({ left1, left2 }, right_side) };
        REQUIRE(result.size() == 1);
        //CHECK(are_equivalent(result[0], noodle));
    }

    SECTION("Single noodle") {
        Nfa left{ 10};
        left.initial.add(0);
        left.final.add(9);
        left.add_trans(0, 108, 1);
        left.add_trans(1, 111, 2);
        left.add_trans(2, 99, 3);
        left.add_trans(3, 97, 4);
        left.add_trans(4, 108, 5);
        left.add_trans(5, 104, 6);
        left.add_trans(6, 111, 7);
        left.add_trans(7, 115, 8);
        left.add_trans(8, 116, 9);

        Nfa right_side{ 1 };
        right_side.initial.add(0);
        right_side.final.add(0);
        right_side.add_trans(0, 44, 0);
        right_side.add_trans(0, 47, 0);
        right_side.add_trans(0, 58, 0);
        right_side.add_trans(0, 85, 0);
        right_side.add_trans(0, 90, 0);
        right_side.add_trans(0, 97, 0);
        right_side.add_trans(0, 99, 0);
        right_side.add_trans(0, 104, 0);
        right_side.add_trans(0, 108, 0);
        right_side.add_trans(0, 111, 0);
        right_side.add_trans(0, 115, 0);
        right_side.add_trans(0, 116, 0);
        right_side.add_trans(0, 117, 0);
        right_side.add_trans(0, 122, 0);

        auto result{ SegNfa::noodlify_for_equation({ left }, right_side) };
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].size() == 1);
        CHECK(are_equivalent(*result[0][0], left));
    }

    SECTION("Larger automata with separate noodles") {
        Nfa left1{ 3};
        left1.initial.add(0);
        left1.final.add({1, 2});
        left1.add_trans(0, 'a', 1);
        left1.add_trans(0, 'b', 2);
        Nfa left2{ 2};
        left2.initial.add(0);
        left2.final.add(1);
        left2.add_trans(0, 'a', 1);
        Nfa left3{ 2};
        left3.initial.add(0);
        left3.final.add(1);
        left3.add_trans(0, 'b', 1);

        Nfa noodle1_segment1{ 2 };
        noodle1_segment1.initial.add(0);
        noodle1_segment1.final.add(1);
        noodle1_segment1.add_trans(0, 'a', 1);

        Nfa noodle1_segment2{ 2 };
        noodle1_segment2.initial.add(0);
        noodle1_segment2.final.add(1);
        noodle1_segment2.add_trans(0, 'a', 1);

        Nfa noodle1_segment3{ 2 };
        noodle1_segment3.initial.add(0);
        noodle1_segment3.final.add(1);
        noodle1_segment3.add_trans(0, 'b', 1);

        std::vector<std::shared_ptr<Nfa>> noodle1_segments{ std::make_shared<Nfa>(noodle1_segment1),
                std::make_shared<Nfa>(noodle1_segment2), std::make_shared<Nfa>(noodle1_segment3) };

        SECTION("Full intersection") {
            Nfa right_side{ 7 };
            right_side.initial.add(0);
            right_side.add_trans(0, 'a', 1);
            right_side.add_trans(1, 'a', 2);
            right_side.add_trans(2, 'b', 3);
            right_side.add_trans(0, 'b', 4);
            right_side.add_trans(4, 'a', 5);
            right_side.add_trans(5, 'b', 6);
            right_side.final.add({3, 6});

            Nfa noodle2_segment1{ 2 };
            noodle2_segment1.initial.add(0);
            noodle2_segment1.final.add(1);
            noodle2_segment1.add_trans(0, 'b', 1);

            Nfa noodle2_segment2{ 2 };
            noodle2_segment2.initial.add(0);
            noodle2_segment2.final.add(1);
            noodle2_segment2.add_trans(0, 'a', 1);

            Nfa noodle2_segment3{ 2 };
            noodle2_segment3.initial.add(0);
            noodle2_segment3.final.add(1);
            noodle2_segment3.add_trans(0, 'b', 1);

            std::vector<std::shared_ptr<Nfa>> noodle2_segments{ std::make_shared<Nfa>(noodle2_segment1),
                    std::make_shared<Nfa>(noodle2_segment2), std::make_shared<Nfa>(noodle2_segment3) };

            SegNfa::NoodleSequence expected{ noodle1_segments, noodle2_segments };

            auto result{ SegNfa::noodlify_for_equation({ left1, left2, left3 }, right_side) };
            REQUIRE(result.size() == 2);

            CHECK(are_equivalent(*result[0][0], *expected[0][0]));
            CHECK(are_equivalent(*result[0][1], *expected[0][1]));
            CHECK(are_equivalent(*result[0][2], *expected[0][2]));

            CHECK(are_equivalent(*result[1][0], *expected[1][0]));
            CHECK(are_equivalent(*result[1][1], *expected[1][1]));
            CHECK(are_equivalent(*result[1][2], *expected[1][2]));
        }

        SECTION("Partial intersection") {
            Nfa right_side{ 7 };
            right_side.initial.add(0);
            right_side.add_trans(0, 'a', 1);
            right_side.add_trans(1, 'a', 2);
            right_side.add_trans(2, 'b', 3);
            right_side.add_trans(0, 'b', 4);
            right_side.add_trans(4, 'a', 5);
            right_side.add_trans(5, 'b', 6);
            right_side.final.add(3);

            auto result{ SegNfa::noodlify_for_equation({ left1, left2, left3 }, right_side) };
            REQUIRE(result.size() == 1);
            CHECK(are_equivalent(*result[0][0], *noodle1_segments[0]));
            CHECK(are_equivalent(*result[0][1], *noodle1_segments[1]));
            CHECK(are_equivalent(*result[0][2], *noodle1_segments[2]));
        }
    }
}

TEST_CASE("Mata::Nfa::SegNfa::noodlify_for_equation() for profiling", "[.profiling][noodlify]") {
    Nfa left1{ 3};
    left1.initial.add(0);
    left1.final.add({1, 2});
    left1.add_trans(0, 'a', 1);
    left1.add_trans(0, 'b', 2);
    Nfa left2{ 2};
    left2.initial.add(0);
    left2.final.add(1);
    left2.add_trans(0, 'a', 1);
    Nfa left3{ 2};
    left3.initial.add(0);
    left3.final.add(1);
    left3.add_trans(0, 'b', 1);

    Nfa right_side{ 7 };
    right_side.initial.add(0);
    right_side.add_trans(0, 'a', 1);
    right_side.add_trans(1, 'a', 2);
    right_side.add_trans(2, 'b', 3);
    right_side.add_trans(0, 'b', 4);
    right_side.add_trans(4, 'a', 5);
    right_side.add_trans(5, 'b', 6);
    right_side.final.add({3, 6});

    AutPtrSequence left_side{ &left1, &left2, &left3 };
    for (size_t i{}; i < 10000; ++i) {
        SegNfa::noodlify_for_equation(left_side, right_side);
    }
}
