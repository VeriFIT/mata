// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata-ng/nfa.hh>
using namespace VataNG::Nfa;

TEST_CASE("VataNG::Nfa::are_disjoint()")
{
	Nfa a, b;

	SECTION("Empty automata are disjoint")
	{
		REQUIRE(are_disjoint(&a, &b));
	}

	SECTION("Left-hand side empty automaton is disjoint with anything")
	{
		b.initialstates = {1, 4, 6};
		b.finalstates = {4, 7, 9, 0};
		add_trans(&b, 1, 1, 1);
		add_trans(&b, 2, 1, 8);
		add_trans(&b, 0, 3, 394093820488);

		REQUIRE(are_disjoint(&a, &b));
	}

	SECTION("Right-hand side empty automaton is disjoint with anything")
	{
		a.initialstates = {1, 4, 6};
		a.finalstates = {4, 7, 9, 0};
		add_trans(&a, 1, 1, 1);
		add_trans(&a, 2, 1, 8);
		add_trans(&a, 0, 3, 394093820488);

		REQUIRE(are_disjoint(&a, &b));
	}

	SECTION("Automata with intersecting initial states are not disjoint")
	{
		a.initialstates = {1, 4, 6};
		b.initialstates = {3, 9, 6, 8};

		REQUIRE(!are_disjoint(&a, &b));
	}

	SECTION("Automata with intersecting final states are not disjoint")
	{
		a.finalstates = {1, 4, 6};
		b.finalstates = {3, 9, 6, 8};

		REQUIRE(!are_disjoint(&a, &b));
	}

	SECTION("Automata with disjoint states are disjoint")
	{
		a.initialstates = {0, 5, 16};
		a.finalstates = {1, 4, 6};

		b.initialstates = {11, 3};
		b.finalstates = {3, 9, 8};

		add_trans(&a, 1, 4, 7);
		add_trans(&a, 1, 2, 7);
		add_trans(&b, 3, 2, 11);
		add_trans(&b, 3, 2, 6);

		REQUIRE(!are_disjoint(&a, &b));
	}

	SECTION("Automata with intersecting states are not disjoint")
	{
		a.initialstates = {0, 5, 16};
		a.finalstates = {1, 4};

		b.initialstates = {11, 3};
		b.finalstates = {3, 9, 6, 8};

		add_trans(&a, 1, 4, 7);
		add_trans(&a, 1, 3, 7);
		add_trans(&b, 3, 2, 11);
		add_trans(&b, 3, 2, 5);

		REQUIRE(!are_disjoint(&a, &b));
	}
}


TEST_CASE("VataNG::Nfa::intersection()")
{
	Nfa a, b, res;
	ProductMap prod_map;

	SECTION("Intersection of empty automata")
	{
		intersection(&res, &a, &b, &prod_map);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.transitions.empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of automata with no transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {3, 5};

		b.initialstates = {4, 6};
		b.finalstates = {4, 2};

		intersection(&res, &a, &b, &prod_map);

		REQUIRE(!res.initialstates.empty());
		REQUIRE(!res.finalstates.empty());

		State init_fin_st = prod_map[std::make_pair(3, 4)];

		REQUIRE(res.initialstates.count(init_fin_st) == 1);
		REQUIRE(res.finalstates.count(init_fin_st) == 1);
	}

	SECTION("Intersection of automata with some transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {3, 5};

		b.initialstates = {4, 6};
		b.finalstates = {4, 2};

		intersection(&res, &a, &b, &prod_map);

		REQUIRE(false);
	}
}
