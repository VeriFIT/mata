// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata-ng/nfa.hh>
using namespace VataNG::Nfa;

TEST_CASE("VataNG::Nfa::Nfa::add_trans()/has_trans()")
{ // {{{
	Nfa a;

	SECTION("Empty automata have now transitions")
	{
		REQUIRE(!a.has_trans(1, 'a', 1));
	}

	SECTION("If I add a transition, it is in the automaton")
	{
		a.add_trans(1, 'a', 1);

		REQUIRE(a.has_trans(1, 'a', 1));
	}

	SECTION("If I add a transition, only it is added")
	{
		a.add_trans(1, 'a', 1);

		REQUIRE(a.has_trans(1, 'a', 1));
		REQUIRE(!a.has_trans(1, 'a', 2));
		REQUIRE(!a.has_trans(1, 'b', 2));
		REQUIRE(!a.has_trans(2, 'a', 1));
	}
} // }}}

TEST_CASE("VataNG::Nfa::are_disjoint()")
{ // {{{
	Nfa a, b;

	SECTION("Empty automata are disjoint")
	{
		REQUIRE(are_disjoint(&a, &b));
	}

	SECTION("Left-hand side empty automaton is disjoint with anything")
	{
		b.initialstates = {1, 4, 6};
		b.finalstates = {4, 7, 9, 0};
		b.add_trans(1, 'a', 1);
		b.add_trans(2, 'a', 8);
		b.add_trans(0, 'c', 394093820488);

		REQUIRE(are_disjoint(&a, &b));
	}

	SECTION("Right-hand side empty automaton is disjoint with anything")
	{
		a.initialstates = {1, 4, 6};
		a.finalstates = {4, 7, 9, 0};
		a.add_trans(1, 'a', 1);
		a.add_trans(2, 'a', 8);
		a.add_trans(0, 'c', 394093820488);

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

		a.add_trans(1, 'a', 7);
		a.add_trans(1, 'b', 7);
		b.add_trans(3, 'b', 11);
		b.add_trans(3, 'b', 6);

		REQUIRE(!are_disjoint(&a, &b));
	}

	SECTION("Automata with intersecting states are not disjoint")
	{
		a.initialstates = {0, 5, 16};
		a.finalstates = {1, 4};

		b.initialstates = {11, 3};
		b.finalstates = {3, 9, 6, 8};

		a.add_trans(1, 'a', 7);
		a.add_trans(1, 'b', 7);
		a.add_trans(1, 'c', 7);
		b.add_trans(3, 'c', 11);
		b.add_trans(3, 'c', 5);
		b.add_trans(11, 'a', 3);

		REQUIRE(!are_disjoint(&a, &b));
	}
} // }}}


TEST_CASE("VataNG::Nfa::intersection()")
{ // {{{
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

		State init_fin_st = prod_map[{3, 4}];

		REQUIRE(res.has_initial(init_fin_st));
		REQUIRE(res.has_final(init_fin_st));
	}

	SECTION("Intersection of automata with some transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {5};
		a.add_trans(1, 'a', 3);
		a.add_trans(1, 'a', 10);
		a.add_trans(1, 'b', 7);
		a.add_trans(3, 'a', 7);
		a.add_trans(3, 'b', 9);
		a.add_trans(9, 'a', 9);
		a.add_trans(7, 'b', 1);
		a.add_trans(7, 'a', 3);
		a.add_trans(7, 'c', 3);
		a.add_trans(10, 'a', 7);
		a.add_trans(10, 'b', 7);
		a.add_trans(10, 'c', 7);
		a.add_trans(7, 'a', 5);
		a.add_trans(5, 'a', 5);
		a.add_trans(5, 'c', 9);

		b.initialstates = {4};
		b.finalstates = {2, 12};
		b.add_trans(4, 'c', 8);
		b.add_trans(4, 'a', 8);
		b.add_trans(8, 'b', 4);
		b.add_trans(4, 'a', 6);
		b.add_trans(4, 'b', 6);
		b.add_trans(6, 'a', 2);
		b.add_trans(2, 'b', 2);
		b.add_trans(2, 'a', 0);
		b.add_trans(0, 'a', 2);
		b.add_trans(2, 'c', 12);
		b.add_trans(12, 'a', 14);
		b.add_trans(14, 'b', 12);

		intersection(&res, &a, &b, &prod_map);

		REQUIRE(res.has_initial(prod_map[{1, 4}]));
		REQUIRE(res.has_initial(prod_map[{3, 4}]));
		REQUIRE(res.has_final(prod_map[{5, 2}]));

		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
		REQUIRE(res.has_trans(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
		REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
		REQUIRE(res.has_trans(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
		REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
		REQUIRE(res.has_trans(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
		REQUIRE(res.has_trans(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.has_trans(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
		REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
		REQUIRE(res.has_trans(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
	}
} // }}}
