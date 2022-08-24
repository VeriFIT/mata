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


