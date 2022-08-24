// TODO: some header

#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/noodlify.hh>

using namespace Mata::Nfa;
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

    SECTION("1-2-3 epsilon transitions")
    {
        aut.make_initial(0);
        aut.make_final({ 4, 5, 6, 7 });
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
        aut.make_initial({ 0, 1, 2 });
        aut.make_final({ 11, 12, 13, 14, 15, 16 });
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
        aut.make_initial(0);
        aut.make_final(7);
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
        CHECK(SegNfa::noodlify_for_equation(std::vector<std::reference_wrapper<const Nfa>>{}, Nfa{}).empty());
    }

    SECTION("Empty left side") {
        Nfa right{1};
        right.make_initial(0);
        right.make_final(0);
        CHECK(SegNfa::noodlify_for_equation(std::vector<std::reference_wrapper<const Nfa>>{}, right).empty());
    }

    SECTION("Empty right side") {
        Nfa left{ 1};
        left.make_initial(0);
        left.make_final(0);
        CHECK(SegNfa::noodlify_for_equation({ left }, Nfa{}).empty());
    }

    SECTION("Small automata without initial and final states") {
        Nfa left{ 1};
        Nfa right{ 2};
        CHECK(SegNfa::noodlify_for_equation({ left }, right).empty());
    }

    SECTION("Small automata") {
        Nfa left1{ 1};
        left1.make_initial(0);
        left1.make_final(0);
        Nfa left2{ 1};
        left2.make_initial(0);
        left2.make_final(0);
        Nfa right{ 2};
        right.make_initial(0);
        right.make_final(0);

        Nfa noodle{2};
        noodle.make_initial(0);
        noodle.add_trans(0, 0, 1);
        noodle.make_final(1);
        auto result{ SegNfa::noodlify_for_equation({ left1, left2 }, right) };
        REQUIRE(result.size() == 1);
        CHECK(equivalence_check(result[0], noodle));
    }

    SECTION("Larger automata") {
        Nfa left1{ 2};
        left1.make_initial(0);
        left1.make_final(1);
        left1.add_trans(0, 'a', 1);
        Nfa left2{ 2};
        left2.make_initial(0);
        left2.make_final(1);
        left2.add_trans(0, 'b', 1);
        Nfa right_side{ 3};
        right_side.make_initial(0);
        right_side.add_trans(0, 'a', 1);
        right_side.add_trans(1, 'b', 2);
        right_side.make_final(2);

        Nfa noodle{ 4 };
        noodle.make_initial(0);
        noodle.make_final(3);
        noodle.add_trans(0, 'a', 1);
        noodle.add_trans(1, 'c', 2); // The automatically chosen epsilon symbol (one larger than 'b').
        noodle.add_trans(2, 'b', 3);
        auto result{ SegNfa::noodlify_for_equation({ left1, left2 }, right_side) };
        REQUIRE(result.size() == 1);
        CHECK(equivalence_check(result[0], noodle));
    }

    SECTION("Larger automata with separate noodles") {
        Nfa left1{ 3};
        left1.make_initial(0);
        left1.make_final({ 1, 2 });
        left1.add_trans(0, 'a', 1);
        left1.add_trans(0, 'b', 2);
        Nfa left2{ 2};
        left2.make_initial(0);
        left2.make_final(1);
        left2.add_trans(0, 'a', 1);
        Nfa left3{ 2};
        left3.make_initial(0);
        left3.make_final(1);
        left3.add_trans(0, 'b', 1);

        Nfa noodle1{ 6 };
        noodle1.make_initial(0);
        noodle1.make_final(5);
        noodle1.add_trans(0, 'a', 1);
        noodle1.add_trans(1, 'c', 2); // The automatically chosen epsilon symbol (one larger than 'b').
        noodle1.add_trans(2, 'a', 3);
        noodle1.add_trans(3, 'c', 4);
        noodle1.add_trans(4, 'b', 5);

        SECTION("Full intersection") {
            Nfa right_side{ 7 };
            right_side.make_initial(0);
            right_side.add_trans(0, 'a', 1);
            right_side.add_trans(1, 'a', 2);
            right_side.add_trans(2, 'b', 3);
            right_side.add_trans(0, 'b', 4);
            right_side.add_trans(4, 'a', 5);
            right_side.add_trans(5, 'b', 6);
            right_side.make_final({ 3, 6 });

            Nfa noodle2{ 6 };
            noodle2.make_initial(0);
            noodle2.make_final(5);
            noodle2.add_trans(0, 'b', 1);
            noodle2.add_trans(1, 'c', 2);
            noodle2.add_trans(2, 'a', 3);
            noodle2.add_trans(3, 'c', 4);
            noodle2.add_trans(4, 'b', 5);

            auto result{ SegNfa::noodlify_for_equation({ left1, left2, left3 }, right_side) };
            REQUIRE(result.size() == 2);
            CHECK(equivalence_check(result[0], noodle1));
            CHECK(equivalence_check(result[1], noodle2));
        }

        SECTION("Partial intersection") {
            Nfa right_side{ 7 };
            right_side.make_initial(0);
            right_side.add_trans(0, 'a', 1);
            right_side.add_trans(1, 'a', 2);
            right_side.add_trans(2, 'b', 3);
            right_side.add_trans(0, 'b', 4);
            right_side.add_trans(4, 'a', 5);
            right_side.add_trans(5, 'b', 6);
            right_side.make_final(3);

            auto result{ SegNfa::noodlify_for_equation({ left1, left2, left3 }, right_side) };
            REQUIRE(result.size() == 1);
            CHECK(equivalence_check(result[0], noodle1));
        }
    }
}

