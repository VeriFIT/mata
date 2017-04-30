// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata-ng/nfa.hh>
using namespace VataNG::Nfa;

TEST_CASE("VataNG::Nfa::are_disjoint() is correct")
{
	Nfa a, b;

	SECTION("Empty automata are disjoint")
	{
		REQUIRE(are_disjoint(&a, &b));
	}
}


