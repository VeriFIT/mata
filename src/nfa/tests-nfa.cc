// TODO: some header

#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>

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

/*
TEST_CASE("Mata::Nfa::Trans::operator<<")
{ // {{{
	Trans trans(1, 2, 3);

	REQUIRE(std::to_string(trans) == "(1, 2, 3)");
} // }}}
*/

TEST_CASE("Mata::Nfa::Nfa::add_trans()/has_trans()")
{ // {{{
	Nfa a(3);

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

TEST_CASE("Mata::Nfa::Nfa iteration")
{ // {{{
	Nfa aut;

	SECTION("empty automaton")
	{
		auto it = aut.begin();
		REQUIRE(it == aut.end());
	}

    const size_t state_num = 'r'+1;
    aut.increase_size(state_num);

	SECTION("a non-empty automaton")
	{
		aut.add_trans('q', 'a', 'r');
		aut.add_trans('q', 'b', 'r');
		auto it = aut.transitionrelation.begin();
		auto jt = aut.transitionrelation.begin();
		REQUIRE(it == jt);
		++it;
		REQUIRE(it != jt);
		REQUIRE((it != aut.transitionrelation.begin() && it != aut.transitionrelation.end()));
		REQUIRE(jt == aut.transitionrelation.begin());

		++jt;
		REQUIRE(it == jt);
		REQUIRE((jt != aut.transitionrelation.begin() && jt != aut.transitionrelation.end()));

        jt = aut.transitionrelation.begin() + state_num - 1;
		++jt;
		REQUIRE(it != jt);
		REQUIRE((jt != aut.transitionrelation.begin() && jt == aut.transitionrelation.end()));

        it = aut.transitionrelation.begin() + state_num - 1;
		++it;
		REQUIRE(it == jt);
		REQUIRE((it != aut.transitionrelation.begin() && it == aut.transitionrelation.end()));
	}
} // }}}

/*
TEST_CASE("Mata::Nfa::are_state_disjoint()")
{ // {{{
	Nfa a(50), b(50);

	SECTION("Empty automata are state disjoint")
	{
		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Left-hand side empty automaton is state disjoint with anything")
	{
		b.initialstates = {1, 4, 6};
		b.finalstates = {4, 7, 9, 0};
		b.add_trans(1, 'a', 1);
		b.add_trans(2, 'a', 8);
		b.add_trans(0, 'c', 49);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Right-hand side empty automaton is state disjoint with anything")
	{
		a.initialstates = {1, 4, 6};
		a.finalstates = {4, 7, 9, 0};
		a.add_trans(1, 'a', 1);
		a.add_trans(2, 'a', 8);
		a.add_trans(0, 'c', 49);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Automata with intersecting initial states are not state disjoint")
	{
		a.initialstates = {1, 4, 6};
		b.initialstates = {3, 9, 6, 8};

		REQUIRE(!are_state_disjoint(a, b));
	}

	SECTION("Automata with intersecting final states are not state disjoint")
	{
		a.finalstates = {1, 4, 6};
		b.finalstates = {3, 9, 6, 8};

		REQUIRE(!are_state_disjoint(a, b));
	}

	SECTION("Automata with disjoint sets of states are state disjoint")
	{
		a.initialstates = {0, 5, 16};
		a.finalstates = {1, 4, 6};

		b.initialstates = {11, 3};
		b.finalstates = {3, 9, 8};

		a.add_trans(1, 'a', 7);
		a.add_trans(1, 'b', 7);
		b.add_trans(3, 'b', 11);
		b.add_trans(3, 'b', 9);

		REQUIRE(are_state_disjoint(a, b));
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

		REQUIRE(!are_state_disjoint(a, b));
	}
} // }}}

TEST_CASE("Mata::Nfa::union_norename()")
{ // {{{
	Nfa a(7), b(7), res;

	SECTION("Union of empty automata")
	{
		union_norename(&res, a, b);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.trans_empty());
	}

	SECTION("Union of automata with no transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {3, 5};

		b.initialstates = {4, 6};
		b.finalstates = {4, 2};

		union_norename(&res, a, b);

		REQUIRE(!res.initialstates.empty());
		REQUIRE(!res.finalstates.empty());

		REQUIRE(res.has_initial(1));
		REQUIRE(res.has_initial(3));
		REQUIRE(res.has_initial(4));
		REQUIRE(res.has_initial(6));
		REQUIRE(res.has_final(3));
		REQUIRE(res.has_final(5));
		REQUIRE(res.has_final(4));
		REQUIRE(res.has_final(2));
	}

	SECTION("Union of automata with some transitions")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		union_norename(&res, a, b);

		EnumAlphabet alph = {"a", "b"};
		StringDict params;
		params["algo"] = "antichains";

		REQUIRE(is_incl(a, res, alph, params));
		REQUIRE(is_incl(b, res, alph, params));
	}

	SECTION("Union of automata with some transitions but without a final state")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);
		b.finalstates = {};

		union_norename(&res, a, b);

		EnumAlphabet alph = {"a", "b"};
		StringDict params;
		params["algo"] = "antichains";

		REQUIRE(is_incl(a, res, alph, params));
		REQUIRE(is_incl(res, a, alph, params));

		WARN_PRINT("Insufficient testing of Mata::Nfa::union_norename()");
	}
} // }}}
*/

TEST_CASE("Mata::Nfa::intersection()")
{ // {{{
	Nfa a, b, res;
	ProductMap prod_map;

	SECTION("Intersection of empty automata")
	{
		intersection(&res, a, b, &prod_map);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.trans_empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of empty automata 2")
	{
		intersection(&res, a, b);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.trans_empty());
	}

    a.increase_size(6);
    b.increase_size(7);

	SECTION("Intersection of automata with no transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {3, 5};

		b.initialstates = {4, 6};
		b.finalstates = {4, 2};

        REQUIRE(!a.initialstates.empty());
        REQUIRE(!b.initialstates.empty());
        REQUIRE(!a.finalstates.empty());
        REQUIRE(!b.finalstates.empty());

		intersection(&res, a, b, &prod_map);

		REQUIRE(!res.initialstates.empty());
		REQUIRE(!res.finalstates.empty());

		State init_fin_st = prod_map[{3, 4}];

		REQUIRE(res.has_initial(init_fin_st));
		REQUIRE(res.has_final(init_fin_st));
	}

    a.increase_size(11);
    b.increase_size(15);

	SECTION("Intersection of automata with some transitions")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		intersection(&res, a, b, &prod_map);

		REQUIRE(res.has_initial(prod_map[{1, 4}]));
		REQUIRE(res.has_initial(prod_map[{3, 4}]));
		REQUIRE(res.has_final(prod_map[{5, 2}]));

        //for (const auto& c : prod_map) std::cout << c.first.first << "," << c.first.second << " -> " << c.second << "\n";
        //std::cout << prod_map[{7, 2}] << " " <<  prod_map[{1, 2}] << '\n';
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 8}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'a', prod_map[{10, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 4}], 'b', prod_map[{7, 6}]));
		REQUIRE(res.has_trans(prod_map[{3, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{3, 0}]));
		REQUIRE(res.has_trans(prod_map[{7, 2}], 'a', prod_map[{5, 0}]));
		// REQUIRE(res.has_trans(prod_map[{7, 2}], 'b', prod_map[{1, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{10, 0}]));
		REQUIRE(res.has_trans(prod_map[{1, 2}], 'a', prod_map[{3, 0}]));
		// REQUIRE(res.has_trans(prod_map[{1, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 0}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{5, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{5, 2}], 'a', prod_map[{5, 0}]));
		REQUIRE(res.has_trans(prod_map[{10, 6}], 'a', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 8}], 'b', prod_map[{7, 4}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 6}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{3, 8}]));
		// REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.has_trans(prod_map[{7, 4}], 'a', prod_map[{5, 6}]));
		// REQUIRE(res.has_trans(prod_map[{7, 4}], 'b', prod_map[{1, 6}]));
		REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{1, 6}], 'a', prod_map[{10, 2}]));
		// REQUIRE(res.has_trans(prod_map[{10, 2}], 'b', prod_map[{7, 2}]));
		REQUIRE(res.has_trans(prod_map[{10, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{7, 0}], 'a', prod_map[{3, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 2}], 'a', prod_map[{7, 0}]));
		REQUIRE(res.has_trans(prod_map[{5, 6}], 'a', prod_map[{5, 2}]));
		REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 6}]));
		REQUIRE(res.has_trans(prod_map[{3, 4}], 'a', prod_map[{7, 8}]));
		REQUIRE(res.has_trans(prod_map[{7, 8}], 'b', prod_map[{1, 4}]));
	}

	SECTION("Intersection of automata with some transitions but without a final state")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);
		b.finalstates = {12};

		intersection(&res, a, b, &prod_map);

		REQUIRE(res.has_initial(prod_map[{1, 4}]));
		REQUIRE(res.has_initial(prod_map[{3, 4}]));
		REQUIRE(is_lang_empty(res));
	}
} // }}}

TEST_CASE("Mata::Nfa::intersection() with preserving epsilon transitions")
{
    constexpr Symbol epsilon{'e'};
    ProductMap prod_map;

    Nfa a{6};
    a.make_initial(0);
    a.make_final({1, 4, 5});
    a.add_trans(0, epsilon, 1);
    a.add_trans(1, 'a', 1);
    a.add_trans(1, 'b', 1);
    a.add_trans(1, 'c', 2);
    a.add_trans(2, 'b', 4);
    a.add_trans(2, epsilon, 3);
    a.add_trans(3, 'a', 5);

    Nfa b{10};
    b.make_initial(0);
    b.make_final({2, 4, 8, 7});
    b.add_trans(0, 'b', 1);
    b.add_trans(0, 'a', 2);
    b.add_trans(2, 'a', 4);
    b.add_trans(2, epsilon, 3);
    b.add_trans(3, 'b', 4);
    b.add_trans(0, 'c', 5);
    b.add_trans(5, 'a', 8);
    b.add_trans(5, epsilon, 6);
    b.add_trans(6, 'a', 9);
    b.add_trans(6, 'b', 7);

    Nfa result{ intersection(a, b, epsilon, &prod_map) };

    // Check states.
    CHECK(result.is_state(prod_map[{0, 0}]));
    CHECK(result.is_state(prod_map[{1, 0}]));
    CHECK(result.is_state(prod_map[{1, 1}]));
    CHECK(result.is_state(prod_map[{1, 2}]));
    CHECK(result.is_state(prod_map[{1, 3}]));
    CHECK(result.is_state(prod_map[{1, 4}]));
    CHECK(result.is_state(prod_map[{2, 5}]));
    CHECK(result.is_state(prod_map[{3, 5}]));
    CHECK(result.is_state(prod_map[{2, 6}]));
    CHECK(result.is_state(prod_map[{3, 6}]));
    CHECK(result.is_state(prod_map[{4, 7}]));
    CHECK(result.is_state(prod_map[{5, 9}]));
    CHECK(result.is_state(prod_map[{5, 8}]));
    CHECK(result.get_num_of_states() == 13);

    CHECK(result.has_initial(prod_map[{0, 0}]));
    CHECK(result.initialstates.size() == 1);

    CHECK(result.has_final(prod_map[{1, 2}]));
    CHECK(result.has_final(prod_map[{1, 4}]));
    CHECK(result.has_final(prod_map[{4, 7}]));
    CHECK(result.has_final(prod_map[{5, 8}]));
    CHECK(result.finalstates.size() == 4);

    // Check transitions.
    CHECK(result.get_num_of_trans() == 15);

    CHECK(result.has_trans(prod_map[{0, 0}], epsilon, prod_map[{1, 0}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{0, 0}]).size() == 1);

    CHECK(result.has_trans(prod_map[{1, 0}], 'b', prod_map[{1, 1}]));
    CHECK(result.has_trans(prod_map[{1, 0}], 'a', prod_map[{1, 2}]));
    CHECK(result.has_trans(prod_map[{1, 0}], 'c', prod_map[{2, 5}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{1, 0}]).size() == 3);

    CHECK(result.get_trans_from_state_as_sequence(prod_map[{1, 1}]).empty());

    CHECK(result.has_trans(prod_map[{1, 2}], epsilon, prod_map[{1, 3}]));
    CHECK(result.has_trans(prod_map[{1, 2}], 'a', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{1, 2}]).size() == 2);

    CHECK(result.has_trans(prod_map[{1, 3}], 'b', prod_map[{1, 4}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{1, 3}]).size() == 1);

    CHECK(result.get_trans_from_state_as_sequence(prod_map[{1, 4}]).empty());

    CHECK(result.has_trans(prod_map[{2, 5}], epsilon, prod_map[{3, 5}]));
    CHECK(result.has_trans(prod_map[{2, 5}], epsilon, prod_map[{2, 6}]));
    CHECK(result.has_trans(prod_map[{2, 5}], epsilon, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{2, 5}]).size() == 3);

    CHECK(result.has_trans(prod_map[{3, 5}], 'a', prod_map[{5, 8}]));
    CHECK(result.has_trans(prod_map[{3, 5}], epsilon, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{3, 5}]).size() == 2);

    CHECK(result.has_trans(prod_map[{2, 6}], 'b', prod_map[{4, 7}]));
    CHECK(result.has_trans(prod_map[{2, 6}], epsilon, prod_map[{3, 6}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{2, 6}]).size() == 2);

    CHECK(result.has_trans(prod_map[{3, 6}], 'a', prod_map[{5, 9}]));
    CHECK(result.get_trans_from_state_as_sequence(prod_map[{3, 6}]).size() == 1);

    CHECK(result.get_trans_from_state_as_sequence(prod_map[{4, 7}]).empty());

    CHECK(result.get_trans_from_state_as_sequence(prod_map[{5, 9}]).empty());

    CHECK(result.get_trans_from_state_as_sequence(prod_map[{5, 8}]).empty());
}

TEST_CASE("Mata::Nfa::is_lang_empty()")
{ // {{{
	Nfa aut(14);
	Path cex;

	SECTION("An empty automaton has an empty language")
	{
		REQUIRE(is_lang_empty(aut));
	}

	SECTION("An automaton with a state that is both initial and final does not have an empty language")
	{
		aut.initialstates = {1, 2};
		aut.finalstates = {2, 3};

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);
	}

	SECTION("More complicated automaton")
	{
		aut.initialstates = {1,2};
		aut.add_trans(1, 'a', 2);
		aut.add_trans(1, 'a', 3);
		aut.add_trans(1, 'b', 4);
		aut.add_trans(2, 'a', 2);
		aut.add_trans(2, 'a', 3);
		aut.add_trans(2, 'b', 4);
		aut.add_trans(3, 'b', 4);
		aut.add_trans(3, 'c', 7);
		aut.add_trans(3, 'b', 2);
		aut.add_trans(7, 'a', 8);

		SECTION("with final states")
		{
			aut.finalstates = {7};
			REQUIRE(!is_lang_empty(aut));
		}

		SECTION("without final states")
		{
			REQUIRE(is_lang_empty(aut));
		}

		SECTION("another complicated automaton")
		{
			FILL_WITH_AUT_A(aut);

			REQUIRE(!is_lang_empty(aut));
		}

		SECTION("a complicated automaton with unreachable final states")
		{
			FILL_WITH_AUT_A(aut);
			aut.finalstates = {13};

			REQUIRE(is_lang_empty(aut));
		}
	}

	SECTION("An automaton with a state that is both initial and final does not have an empty language")
	{
		aut.initialstates = {1, 2};
		aut.finalstates = {2, 3};

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.size() == 1);
		REQUIRE(cex[0] == 2);
	}

	SECTION("Counterexample of an automaton with non-empty language")
	{
		aut.initialstates = {1, 2};
		aut.finalstates = {8, 9};
		aut.add_trans(1, 'c', 2);
		aut.add_trans(2, 'a', 4);
		aut.add_trans(2, 'c', 1);
		aut.add_trans(2, 'c', 3);
		aut.add_trans(3, 'e', 5);
		aut.add_trans(4, 'c', 8);

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.size() == 3);
		REQUIRE(cex[0] == 2);
		REQUIRE(cex[1] == 4);
		REQUIRE(cex[2] == 8);
	}
} // }}}

TEST_CASE("Mata::Nfa::get_word_for_path()")
{ // {{{
	Nfa aut(5);
	Path path;
	Word word;

	SECTION("empty word")
	{
		path = { };

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.empty());
	}

	SECTION("empty word 2")
	{
		aut.initialstates = {1};
		path = {1};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.empty());
	}

	SECTION("nonempty word")
	{
		aut.initialstates = {1};
		aut.add_trans(1, 'c', 2);
		aut.add_trans(2, 'a', 4);
		aut.add_trans(2, 'c', 1);
		aut.add_trans(2, 'b', 3);

		path = {1,2,3};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first == Word({'c', 'b'}));
	}

	SECTION("longer word")
	{
		aut.initialstates = {1};
		aut.add_trans(1, 'a', 2);
		aut.add_trans(1, 'c', 2);
		aut.add_trans(2, 'a', 4);
		aut.add_trans(2, 'c', 1);
		aut.add_trans(2, 'b', 3);
		aut.add_trans(3, 'd', 2);

		path = {1,2,3,2,4};

		auto word_bool_pair = get_word_for_path(aut, path);
		std::set<Word> possible({
			Word({'c', 'b', 'd', 'a'}),
			Word({'a', 'b', 'd', 'a'})});
		REQUIRE(word_bool_pair.second);
		REQUIRE(haskey(possible, word_bool_pair.first));
	}

	SECTION("invalid path")
	{
		aut.initialstates = {1};
		aut.add_trans(1, 'a', 2);
		aut.add_trans(1, 'c', 2);
		aut.add_trans(2, 'a', 4);
		aut.add_trans(2, 'c', 1);
		aut.add_trans(2, 'b', 3);
		aut.add_trans(3, 'd', 2);

		path = {1,2,3,1,2};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(!word_bool_pair.second);
	}
}


TEST_CASE("Mata::Nfa::is_lang_empty_cex()")
{
	Nfa aut(10);
	Word cex;

	SECTION("Counterexample of an automaton with non-empty language")
	{
		aut.initialstates = {1, 2};
		aut.finalstates = {8, 9};
		aut.add_trans(1, 'c', 2);
		aut.add_trans(2, 'a', 4);
		aut.add_trans(2, 'c', 1);
		aut.add_trans(2, 'c', 3);
		aut.add_trans(3, 'e', 5);
		aut.add_trans(4, 'c', 8);

		bool is_empty = is_lang_empty_cex(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.size() == 2);
		REQUIRE(cex[0] == 'a');
		REQUIRE(cex[1] == 'c');
	}
}


TEST_CASE("Mata::Nfa::determinize()")
{
	Nfa aut(3);
	Nfa result;
	SubsetMap subset_map;

	SECTION("empty automaton")
	{
		determinize(&result, aut);

		REQUIRE(result.has_initial(subset_map[{}]));
		REQUIRE(result.finalstates.empty());
		REQUIRE(result.nothing_in_trans());
	}

	SECTION("simple automaton 1")
	{
		aut.initialstates = { 1 };
		aut.finalstates = { 1 };
		determinize(&result, aut, &subset_map);

		REQUIRE(result.has_initial(subset_map[{1}]));
		REQUIRE(result.has_final(subset_map[{1}]));
		REQUIRE(result.nothing_in_trans());
	}

	SECTION("simple automaton 2")
	{
		aut.initialstates = { 1 };
		aut.finalstates = { 2 };
		aut.add_trans(1, 'a', 2);
		determinize(&result, aut, &subset_map);

		REQUIRE(result.has_initial(subset_map[{1}]));
        REQUIRE(result.has_final(subset_map[{2}]));
		REQUIRE(result.has_trans(subset_map[{1}], 'a', subset_map[{2}]));
	}
} // }}}

TEST_CASE("Mata::Nfa::construct() correct calls")
{ // {{{
	Nfa aut(10);
	Mata::Parser::ParsedSection parsec;
	StringToSymbolMap symbol_map;

	SECTION("construct an empty automaton")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;

		construct(&aut, parsec);

		REQUIRE(is_lang_empty(aut));
	}

	SECTION("construct a simple non-empty automaton accepting the empty word")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q1"}});

		construct(&aut, parsec);

		REQUIRE(!is_lang_empty(aut));
	}

	SECTION("construct an automaton with more than one initial/final states")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1", "q2"}});
		parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

		construct(&aut, parsec);

		REQUIRE(aut.initialstates.size() == 2);
		REQUIRE(aut.finalstates.size() == 3);
	}

	SECTION("construct a simple non-empty automaton accepting only the word 'a'")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q2"}});
		parsec.body = { {"q1", "a", "q2"} };

		construct(&aut, parsec, &symbol_map);

		Path cex;
		REQUIRE(!is_lang_empty(aut, &cex));
		auto word_bool_pair = get_word_for_path(aut, cex);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first == encode_word(symbol_map, {"a"}));

		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a"})));
	}

	SECTION("construct a more complicated non-empty automaton")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1", "q3"}});
		parsec.dict.insert({"Final", {"q5"}});
		parsec.body.push_back({"q1", "a", "q3"});
		parsec.body.push_back({"q1", "a", "q10"});
		parsec.body.push_back({"q1", "b", "q7"});
		parsec.body.push_back({"q3", "a", "q7"});
		parsec.body.push_back({"q3", "b", "q9"});
		parsec.body.push_back({"q9", "a", "q9"});
		parsec.body.push_back({"q7", "b", "q1"});
		parsec.body.push_back({"q7", "a", "q3"});
		parsec.body.push_back({"q7", "c", "q3"});
		parsec.body.push_back({"q10", "a", "q7"});
		parsec.body.push_back({"q10", "b", "q7"});
		parsec.body.push_back({"q10", "c", "q7"});
		parsec.body.push_back({"q7", "a", "q5"});
		parsec.body.push_back({"q5", "a", "q5"});
		parsec.body.push_back({"q5", "c", "q9"});

		construct(&aut, parsec, &symbol_map);

		// some samples
		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"b", "a"})));
		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a", "c", "a", "a"})));
		REQUIRE(is_in_lang(aut, encode_word(symbol_map,
			{"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a"})));
		// some wrong samples
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"b", "c"})));
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"a", "c", "c", "a"})));
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"b", "a", "c", "b"})));
	}
} // }}}

TEST_CASE("Mata::Nfa::construct() invalid calls")
{ // {{{
	Nfa aut;
	Mata::Parser::ParsedSection parsec;

	SECTION("construct() call with invalid ParsedSection object")
	{
		parsec.type = "FA";

		CHECK_THROWS_WITH(construct(&aut, parsec),
			Catch::Contains("expecting type"));
	}

	SECTION("construct() call with an epsilon transition")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.body = { {"q1", "q2"} };

		CHECK_THROWS_WITH(construct(&aut, parsec),
			Catch::Contains("Epsilon transition"));
	}

	SECTION("construct() call with a nonsense transition")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.body = { {"q1", "a", "q2", "q3"} };

		CHECK_THROWS_WITH(construct(&aut, parsec),
			Catch::Contains("Invalid transition"));
	}
} // }}}

/*
TEST_CASE("Mata::Nfa::serialize() and operator<<()")
{ // {{{
	Nfa aut;

	SECTION("empty automaton")
	{
		std::string str;

		SECTION("serialize()")
		{
			str = std::to_string(serialize(aut));
		}

		SECTION("operator<<")
		{
			std::ostringstream os;
			os << aut;
			str = os.str();
		}

		Mata::Parser::ParsedSection parsec = Mata::Parser::parse_vtf_section(str);
		Nfa res = construct(parsec);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.trans_empty());
	}

	SECTION("small automaton")
	{
		aut.initialstates = { 'q', 'r', 's' };
		aut.finalstates = { 'r', 's', 't' };

		aut.add_trans('q', 'a', 'r');
		aut.add_trans('r', 'b', 'q');
		aut.add_trans('s', 'c', 'q');
		aut.add_trans('s', 'd', 'q');
		aut.add_trans('q', 'a', 'q');

		Mata::Nfa::StateToStringMap state_dict =
			{{'q', "q"}, {'r', "r"}, {'s', "s"}, {'t', "t"}};
		Mata::Nfa::SymbolToStringMap symb_dict =
			{{'a', "a"}, {'b', "b"}, {'c', "c"}, {'d', "d"}};
		std::string str = std::to_string(serialize(aut, &symb_dict, &state_dict));

		ParsedSection parsec = Mata::Parser::parse_vtf_section(str);

		Mata::Nfa::StringToStateMap inv_state_dict =
			Mata::util::invert_map(state_dict);
		Mata::Nfa::StringToSymbolMap inv_symb_dict =
			Mata::util::invert_map(symb_dict);
		Nfa res = construct(parsec, &inv_symb_dict, &inv_state_dict);

		REQUIRE(res.initialstates == aut.initialstates);
		REQUIRE(res.finalstates == aut.finalstates);
		REQUIRE(res.trans_size() == aut.trans_size());
		REQUIRE(res.has_trans('q', 'a', 'r'));
		REQUIRE(res.has_trans('r', 'b', 'q'));
		REQUIRE(res.has_trans('s', 'c', 'q'));
		REQUIRE(res.has_trans('s', 'd', 'q'));
		REQUIRE(res.has_trans('q', 'a', 'q'));
	}

	SECTION("implicit state and symbol mapper")
	{
		aut.add_trans(1, 2, 3);

		ParsedSection parsec = serialize(aut);

		REQUIRE(parsec.body.size() == 1);
		REQUIRE(*parsec.body.cbegin() == BodyLine({"q1", "a2", "q3"}));
	}

	SECTION("incorrect state mapper")
	{
		Mata::Nfa::StateToStringMap state_dict = {{'q', "q"}};
		Mata::Nfa::SymbolToStringMap symb_dict = {{'a', "a"}};
		aut.add_trans('q', 'a', 'r');

		CHECK_THROWS_WITH(serialize(aut, &symb_dict, &state_dict),
			Catch::Contains("cannot translate state"));
	}

	SECTION("incorrect symbol mapper")
	{
		Mata::Nfa::StateToStringMap state_dict = {{'q', "q"}, {'r', "r"}};
		Mata::Nfa::SymbolToStringMap symb_dict = {{'a', "a"}};
		aut.add_trans('q', 'b', 'r');

		CHECK_THROWS_WITH(serialize(aut, &symb_dict, &state_dict),
			Catch::Contains("cannot translate symbol"));
	}
} // }}}
*/

TEST_CASE("Mata::Nfa::make_complete()")
{ // {{{
	Nfa aut(11);

	SECTION("empty automaton, empty alphabet")
	{
		EnumAlphabet alph = { };

		make_complete(&aut, alph, 0);

		REQUIRE(aut.initialstates.empty());
		REQUIRE(aut.finalstates.empty());
		REQUIRE(aut.nothing_in_trans());
	}

	SECTION("empty automaton")
	{
		EnumAlphabet alph = {"a", "b"};

		make_complete(&aut, alph, 0);

		REQUIRE(aut.initialstates.empty());
		REQUIRE(aut.finalstates.empty());
		REQUIRE(aut.has_trans(0, alph["a"], 0));
		REQUIRE(aut.has_trans(0, alph["b"], 0));
	}

	SECTION("non-empty automaton, empty alphabet")
	{
		EnumAlphabet alphabet{};

		aut.initialstates = {1};

		make_complete(&aut, alphabet, 0);

		REQUIRE(aut.initialstates.size() == 1);
		REQUIRE(*aut.initialstates.begin() == 1);
		REQUIRE(aut.finalstates.empty());
		REQUIRE(aut.nothing_in_trans());
	}

	SECTION("one-state automaton")
	{
		EnumAlphabet alph = {"a", "b"};
		const State SINK = 10;

		aut.initialstates = {1};

		make_complete(&aut, alph, SINK);

		REQUIRE(aut.initialstates.size() == 1);
		REQUIRE(*aut.initialstates.begin() == 1);
		REQUIRE(aut.finalstates.empty());
		REQUIRE(aut.has_trans(1, alph["a"], SINK));
		REQUIRE(aut.has_trans(1, alph["b"], SINK));
		REQUIRE(aut.has_trans(SINK, alph["a"], SINK));
		REQUIRE(aut.has_trans(SINK, alph["b"], SINK));
	}

	SECTION("bigger automaton")
	{
		EnumAlphabet alph = {"a", "b", "c"};
		const State SINK = 9;

		aut.initialstates = {1, 2};
		aut.finalstates = {8};
		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(2, alph["a"], 4);
		aut.add_trans(2, alph["c"], 1);
		aut.add_trans(2, alph["c"], 3);
		aut.add_trans(3, alph["b"], 5);
		aut.add_trans(4, alph["c"], 8);

		make_complete(&aut, alph, SINK);

		REQUIRE(aut.has_trans(1, alph["a"], 2));
		REQUIRE(aut.has_trans(1, alph["b"], SINK));
		REQUIRE(aut.has_trans(1, alph["c"], SINK));
		REQUIRE(aut.has_trans(2, alph["a"], 4));
		REQUIRE(aut.has_trans(2, alph["c"], 1));
		REQUIRE(aut.has_trans(2, alph["c"], 3));
		REQUIRE(aut.has_trans(2, alph["b"], SINK));
		REQUIRE(aut.has_trans(3, alph["b"], 5));
		REQUIRE(aut.has_trans(3, alph["a"], SINK));
		REQUIRE(aut.has_trans(3, alph["c"], SINK));
		REQUIRE(aut.has_trans(4, alph["c"], 8));
		REQUIRE(aut.has_trans(4, alph["a"], SINK));
		REQUIRE(aut.has_trans(4, alph["b"], SINK));
		REQUIRE(aut.has_trans(5, alph["a"], SINK));
		REQUIRE(aut.has_trans(5, alph["b"], SINK));
		REQUIRE(aut.has_trans(5, alph["c"], SINK));
		REQUIRE(aut.has_trans(8, alph["a"], SINK));
		REQUIRE(aut.has_trans(8, alph["b"], SINK));
		REQUIRE(aut.has_trans(8, alph["c"], SINK));
		REQUIRE(aut.has_trans(SINK, alph["a"], SINK));
		REQUIRE(aut.has_trans(SINK, alph["b"], SINK));
		REQUIRE(aut.has_trans(SINK, alph["c"], SINK));
	}
} // }}}

TEST_CASE("Mata::Nfa::complement()")
{ // {{{
	Nfa aut(3);
	Nfa cmpl;

	SECTION("empty automaton, empty alphabet")
	{
		EnumAlphabet alph = {};

		cmpl = complement(aut, alph);

		REQUIRE(is_in_lang(cmpl, { }));
		REQUIRE(cmpl.initialstates.size() == 1);
		REQUIRE(cmpl.finalstates.size() == 1);
		REQUIRE(cmpl.nothing_in_trans());
		REQUIRE(*cmpl.initialstates.begin() == *cmpl.finalstates.begin());
	}

	SECTION("empty automaton")
	{
		EnumAlphabet alph = {"a", "b"};

		cmpl = complement(aut, alph);

		REQUIRE(is_in_lang(cmpl, { }));
		REQUIRE(is_in_lang(cmpl, { alph["a"] }));
		REQUIRE(is_in_lang(cmpl, { alph["b"] }));
		REQUIRE(is_in_lang(cmpl, { alph["a"], alph["a"] }));
		REQUIRE(is_in_lang(cmpl, { alph["a"], alph["b"], alph["b"], alph["a"] }));

		// TODO: consider removing the structural tests (in case a more
		// sophisticated complementation algorithm is used)
		REQUIRE(cmpl.initialstates.size() == 1);
		REQUIRE(cmpl.finalstates.size() == 1);

		State init_state = *cmpl.initialstates.begin();
		State fin_state = *cmpl.finalstates.begin();
		REQUIRE(init_state == fin_state);
		REQUIRE(cmpl.get_transitions_from_state(init_state).size() == 2);
		REQUIRE(cmpl.has_trans(init_state, alph["a"], init_state));
		REQUIRE(cmpl.has_trans(init_state, alph["b"], init_state));
	}

	SECTION("empty automaton accepting epsilon, empty alphabet")
	{
		EnumAlphabet alph = {};
		aut.initialstates = {1};
		aut.finalstates = {1};

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(cmpl.initialstates.size() == 1);
		REQUIRE(cmpl.finalstates.size() == 0);
		REQUIRE(cmpl.nothing_in_trans());
	}

	SECTION("empty automaton accepting epsilon")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1};
		aut.finalstates = {1};

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(is_in_lang(cmpl, { alph["a"] }));
		REQUIRE(is_in_lang(cmpl, { alph["b"] }));
		REQUIRE(is_in_lang(cmpl, { alph["a"], alph["a"] }));
		REQUIRE(is_in_lang(cmpl, { alph["a"], alph["b"], alph["b"], alph["a"] }));
		REQUIRE(cmpl.initialstates.size() == 1);
		REQUIRE(cmpl.finalstates.size() == 1);
		size_t sum = 0;
		for (const auto& x : cmpl) {
            unused(x);
            sum++;
		}
		REQUIRE(sum == 4);
	}

	SECTION("non-empty automaton accepting a*b*")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1,2};
		aut.finalstates = {1,2};

		aut.add_trans(1, alph["a"], 1);
		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(2, alph["b"], 2);

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(!is_in_lang(cmpl, { alph["a"] }));
		REQUIRE(!is_in_lang(cmpl, { alph["b"] }));
		REQUIRE(!is_in_lang(cmpl, { alph["a"], alph["a"] }));
		REQUIRE(is_in_lang(cmpl, { alph["a"], alph["b"], alph["b"], alph["a"] }));
		REQUIRE(!is_in_lang(cmpl, { alph["a"], alph["a"], alph["b"], alph["b"] }));
		REQUIRE(is_in_lang(cmpl, { alph["b"], alph["a"], alph["a"], alph["a"] }));

		REQUIRE(cmpl.initialstates.size() == 1);
		REQUIRE(cmpl.finalstates.size() == 1);
		size_t sum = 0;
		for (const auto& x : cmpl) {
            unused(x);
            sum++;
		}
		REQUIRE(sum == 6);
	}

} // }}}

TEST_CASE("Mata::Nfa::is_universal()")
{ // {{{
	Nfa aut(6);
	Word cex;
	StringDict params;

	const std::unordered_set<std::string> ALGORITHMS = {
		"naive",
		"antichains",
	};

	SECTION("empty automaton, empty alphabet")
	{
		EnumAlphabet alph = { };

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("empty automaton accepting epsilon, empty alphabet")
	{
		EnumAlphabet alph = { };
		aut.initialstates = {1};
		aut.finalstates = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
			REQUIRE(Word{ } == cex);
		}
	}

	SECTION("empty automaton accepting epsilon")
	{
		EnumAlphabet alph = {"a"};
		aut.initialstates = {1};
		aut.finalstates = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(!is_univ);
			REQUIRE(((cex == Word{alph["a"]}) || (cex == Word{alph["b"]})));
		}
	}

	SECTION("automaton for a*b*")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1,2};
		aut.finalstates = {1,2};

		aut.add_trans(1, alph["a"], 1);
		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(2, alph["b"], 2);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("automaton for a* + b*")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1,2};
		aut.finalstates = {1,2};

		aut.add_trans(1, alph["a"], 1);
		aut.add_trans(2, alph["b"], 2);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("automaton for (a + b)*")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1};
		aut.finalstates = {1};

		aut.add_trans(1, alph["a"], 1);
		aut.add_trans(1, alph["b"], 1);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("automaton for eps + (a+b) + (a+b)(a+b)(a* + b*)")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1};
		aut.finalstates = {1, 2, 3, 4, 5};

		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(1, alph["b"], 2);
		aut.add_trans(2, alph["a"], 3);
		aut.add_trans(2, alph["b"], 3);

		aut.add_trans(3, alph["a"], 4);
		aut.add_trans(4, alph["a"], 4);

		aut.add_trans(3, alph["b"], 5);
		aut.add_trans(5, alph["b"], 5);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(!is_univ);

			REQUIRE(cex.size() == 4);
			REQUIRE((cex[0] == alph["a"] || cex[0] == alph["b"]));
			REQUIRE((cex[1] == alph["a"] || cex[1] == alph["b"]));
			REQUIRE((cex[2] == alph["a"] || cex[2] == alph["b"]));
			REQUIRE((cex[3] == alph["a"] || cex[3] == alph["b"]));
			REQUIRE(cex[2] != cex[3]);
		}
	}

	SECTION("automaton for epsilon + a(a + b)* + b(a + b)*")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1,3};
		aut.finalstates = {1,2,4};

		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(2, alph["a"], 2);
		aut.add_trans(2, alph["b"], 2);
		aut.add_trans(3, alph["b"], 4);
		aut.add_trans(4, alph["a"], 4);
		aut.add_trans(4, alph["b"], 4);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("example from Abdulla et al. TACAS'10")
	{
		EnumAlphabet alph = {"a", "b"};
		aut.initialstates = {1,2};
		aut.finalstates = {1,2,3};

		aut.add_trans(1, alph["b"], 1);
		aut.add_trans(1, alph["a"], 2);
		aut.add_trans(1, alph["b"], 4);
		aut.add_trans(2, alph["b"], 2);
		aut.add_trans(2, alph["a"], 3);
		aut.add_trans(3, alph["b"], 3);
		aut.add_trans(3, alph["a"], 1);
		aut.add_trans(4, alph["b"], 2);
		aut.add_trans(4, alph["b"], 3);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("subsumption-pruning in processed")
	{
		EnumAlphabet alph = {"a"};
		aut.initialstates = {1,2};
		aut.finalstates = {1};

		aut.add_trans(1, alph["a"], 1);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("wrong parameters 1")
	{
		EnumAlphabet alph = { };

		CHECK_THROWS_WITH(is_universal(aut, alph, params),
			Catch::Contains("requires setting the \"algo\" key"));
	}

	SECTION("wrong parameters 2")
	{
		EnumAlphabet alph = { };
		params["algo"] = "foo";

		CHECK_THROWS_WITH(is_universal(aut, alph, params),
			Catch::Contains("received an unknown value"));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_incl()")
{ // {{{
	Nfa smaller(10);
	Nfa bigger(16);
	Word cex;
	StringDict params;

	const std::unordered_set<std::string> ALGORITHMS = {
		"naive",
		"antichains",
	};

	SECTION("{} <= {}, empty alphabet")
	{
		EnumAlphabet alph = { };

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, params);
			CHECK(is_included);

            is_included = is_incl(bigger, smaller, alph, params);
            CHECK(is_included);
		}
	}

	SECTION("{} <= {epsilon}, empty alphabet")
	{
		EnumAlphabet alph = { };
		bigger.initialstates = {1};
		bigger.finalstates = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, &cex, params);
            CHECK(is_included);

            is_included = is_incl(bigger, smaller, alph, &cex, params);
            CHECK(!is_included);
		}
	}

	SECTION("{epsilon} <= {epsilon}, empty alphabet")
	{
		EnumAlphabet alph = { };
		smaller.initialstates = {1};
		smaller.finalstates = {1};
		bigger.initialstates = {11};
		bigger.finalstates = {11};

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, &cex, params);
            CHECK(is_included);

            is_included = is_incl(bigger, smaller, alph, &cex, params);
            CHECK(is_included);
		}
	}

	SECTION("{epsilon} !<= {}, empty alphabet")
	{
		EnumAlphabet alph = { };
		smaller.initialstates = {1};
		smaller.finalstates = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, &cex, params);

			REQUIRE(!is_included);
			REQUIRE(cex == Word{});

            is_included = is_incl(bigger, smaller, alph, &cex, params);
            REQUIRE(cex == Word{});
            REQUIRE(is_included);
		}
	}

	SECTION("a* + b* <= (a+b)*")
	{
		EnumAlphabet alph = {"a", "b"};
		smaller.initialstates = {1,2};
		smaller.finalstates = {1,2};
		smaller.add_trans(1, alph["a"], 1);
		smaller.add_trans(2, alph["b"], 2);

		bigger.initialstates = {11};
		bigger.finalstates = {11};
		bigger.add_trans(11, alph["a"], 11);
		bigger.add_trans(11, alph["b"], 11);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, params);
			REQUIRE(is_included);

            is_included = is_incl(bigger, smaller, alph, params);
            REQUIRE(!is_included);
		}
	}

	SECTION("(a+b)* !<= a* + b*")
	{
		EnumAlphabet alph = {"a", "b"};
		smaller.initialstates = {1};
		smaller.finalstates = {1};
		smaller.add_trans(1, alph["a"], 1);
		smaller.add_trans(1, alph["b"], 1);

		bigger.initialstates = {11, 12};
		bigger.finalstates = {11, 12};
		bigger.add_trans(11, alph["a"], 11);
		bigger.add_trans(12, alph["b"], 12);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;

			bool is_included = is_incl(smaller, bigger, alph, &cex, params);

			REQUIRE(!is_included);
			REQUIRE((
				cex == Word{alph["a"], alph["b"]} ||
				cex == Word{alph["b"], alph["a"]}));

            is_included = is_incl(bigger, smaller, alph, &cex, params);
            REQUIRE(is_included);
            REQUIRE((
                cex == Word{alph["a"], alph["b"]} ||
                cex == Word{alph["b"], alph["a"]}));
		}
	}

	SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
	{
		EnumAlphabet alph = {"a", "b"};
		smaller.initialstates = {1};
		smaller.finalstates = {1};
		smaller.add_trans(1, alph["a"], 1);
		smaller.add_trans(1, alph["b"], 1);

		bigger.initialstates = {11};
		bigger.finalstates = {11, 12, 13, 14, 15};

		bigger.add_trans(11, alph["a"], 12);
		bigger.add_trans(11, alph["b"], 12);
		bigger.add_trans(12, alph["a"], 13);
		bigger.add_trans(12, alph["b"], 13);

		bigger.add_trans(13, alph["a"], 14);
		bigger.add_trans(14, alph["a"], 14);

		bigger.add_trans(13, alph["b"], 15);
		bigger.add_trans(15, alph["b"], 15);

		for (const auto& algo : ALGORITHMS) {
			params["algo"] = algo;
			bool is_included = is_incl(smaller, bigger, alph, &cex, params);
			REQUIRE(!is_included);

			REQUIRE(cex.size() == 4);
			REQUIRE((cex[0] == alph["a"] || cex[0] == alph["b"]));
			REQUIRE((cex[1] == alph["a"] || cex[1] == alph["b"]));
			REQUIRE((cex[2] == alph["a"] || cex[2] == alph["b"]));
			REQUIRE((cex[3] == alph["a"] || cex[3] == alph["b"]));
			REQUIRE(cex[2] != cex[3]);

            is_included = is_incl(bigger, smaller, alph, &cex, params);
            REQUIRE(is_included);

            REQUIRE(cex.size() == 4);
            REQUIRE((cex[0] == alph["a"] || cex[0] == alph["b"]));
            REQUIRE((cex[1] == alph["a"] || cex[1] == alph["b"]));
            REQUIRE((cex[2] == alph["a"] || cex[2] == alph["b"]));
            REQUIRE((cex[3] == alph["a"] || cex[3] == alph["b"]));
            REQUIRE(cex[2] != cex[3]);
		}
	}

	SECTION("wrong parameters 1")
	{
		EnumAlphabet alph = { };

		CHECK_THROWS_WITH(is_incl(smaller, bigger, alph, params),
			Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(is_incl(smaller, bigger, alph));
	}

	SECTION("wrong parameters 2")
	{
		EnumAlphabet alph = { };
		params["algo"] = "foo";

		CHECK_THROWS_WITH(is_incl(smaller, bigger, alph, params),
			Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(is_incl(smaller, bigger, alph));
	}
} // }}}

TEST_CASE("Mata::Nfa::equivalence_check")
{
    Nfa smaller(10);
    Nfa bigger(16);
    Word cex;
    StringDict params;

    const std::unordered_set<std::string> ALGORITHMS = {
            "naive",
            "antichains",
    };

    SECTION("{} == {}, empty alphabet")
    {
        EnumAlphabet alph{};

        for (const auto& algo : ALGORITHMS) {
            params["algo"] = algo;

            CHECK(equivalence_check(smaller, bigger, alph, params));
            CHECK(equivalence_check(smaller, bigger, params));
            CHECK(equivalence_check(smaller, bigger));

            CHECK(equivalence_check(bigger, smaller , alph, params));
            CHECK(equivalence_check(bigger, smaller, params));
            CHECK(equivalence_check(bigger, smaller));
        }
    }

    SECTION("{} == {epsilon}, empty alphabet")
    {
        EnumAlphabet alph = { };
        bigger.initialstates = {1};
        bigger.finalstates = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algo"] = algo;

            CHECK(!equivalence_check(smaller, bigger, alph, params));
            CHECK(!equivalence_check(smaller, bigger, params));
            CHECK(!equivalence_check(smaller, bigger));

            CHECK(!equivalence_check(bigger, smaller , alph, params));
            CHECK(!equivalence_check(bigger, smaller, params));
            CHECK(!equivalence_check(bigger, smaller));
        }
    }

    SECTION("{epsilon} == {epsilon}, empty alphabet")
    {
        EnumAlphabet alph = { };
        smaller.initialstates = {1};
        smaller.finalstates = {1};
        bigger.initialstates = {11};
        bigger.finalstates = {11};

        for (const auto& algo : ALGORITHMS) {
            params["algo"] = algo;

            CHECK(equivalence_check(smaller, bigger, alph, params));
            CHECK(equivalence_check(smaller, bigger, params));
            CHECK(equivalence_check(smaller, bigger));

            CHECK(equivalence_check(bigger, smaller , alph, params));
            CHECK(equivalence_check(bigger, smaller, params));
            CHECK(equivalence_check(bigger, smaller));
        }
    }

    SECTION("a* + b* == (a+b)*")
    {
        EnumAlphabet alph = {"a", "b"};
        smaller.initialstates = {1,2};
        smaller.finalstates = {1,2};
        smaller.add_trans(1, alph["a"], 1);
        smaller.add_trans(2, alph["b"], 2);

        bigger.initialstates = {11};
        bigger.finalstates = {11};
        bigger.add_trans(11, alph["a"], 11);
        bigger.add_trans(11, alph["b"], 11);

        for (const auto& algo : ALGORITHMS) {
            params["algo"] = algo;

            CHECK(!equivalence_check(smaller, bigger, alph, params));
            CHECK(!equivalence_check(smaller, bigger, params));
            CHECK(!equivalence_check(smaller, bigger));

            CHECK(!equivalence_check(bigger, smaller , alph, params));
            CHECK(!equivalence_check(bigger, smaller, params));
            CHECK(!equivalence_check(bigger, smaller));
        }
    }

    SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
    {
        EnumAlphabet alph = {"a", "b"};
        smaller.initialstates = {1};
        smaller.finalstates = {1};
        smaller.add_trans(1, alph["a"], 1);
        smaller.add_trans(1, alph["b"], 1);

        bigger.initialstates = {11};
        bigger.finalstates = {11, 12, 13, 14, 15};

        bigger.add_trans(11, alph["a"], 12);
        bigger.add_trans(11, alph["b"], 12);
        bigger.add_trans(12, alph["a"], 13);
        bigger.add_trans(12, alph["b"], 13);

        bigger.add_trans(13, alph["a"], 14);
        bigger.add_trans(14, alph["a"], 14);

        bigger.add_trans(13, alph["b"], 15);
        bigger.add_trans(15, alph["b"], 15);

        for (const auto& algo : ALGORITHMS) {
            params["algo"] = algo;

            CHECK(!equivalence_check(smaller, bigger, alph, params));
            CHECK(!equivalence_check(smaller, bigger, params));
            CHECK(!equivalence_check(smaller, bigger));

            CHECK(!equivalence_check(bigger, smaller , alph, params));
            CHECK(!equivalence_check(bigger, smaller, params));
            CHECK(!equivalence_check(bigger, smaller));
        }
    }

    SECTION("wrong parameters 1")
    {
        EnumAlphabet alph = { };

        CHECK_THROWS_WITH(equivalence_check(smaller, bigger, alph, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_THROWS_WITH(equivalence_check(smaller, bigger, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(equivalence_check(smaller, bigger));
    }

    SECTION("wrong parameters 2")
    {
        EnumAlphabet alph = { };
        params["algo"] = "foo";

        CHECK_THROWS_WITH(equivalence_check(smaller, bigger, alph, params),
                          Catch::Contains("received an unknown value"));
        CHECK_THROWS_WITH(equivalence_check(smaller, bigger, params),
                          Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(equivalence_check(smaller, bigger));
    }
}

TEST_CASE("Mata::Nfa::revert()")
{ // {{{
	Nfa aut(9);

	SECTION("empty automaton")
	{
		Nfa result = revert(aut);

		REQUIRE(result.nothing_in_trans());
		REQUIRE(result.initialstates.size() == 0);
		REQUIRE(result.finalstates.size() == 0);
	}

	SECTION("no-transition automaton")
	{
		aut.make_initial(1);
		aut.make_initial(3);

		aut.make_final(2);
		aut.make_final(5);

		Nfa result = revert(aut);

		REQUIRE(result.nothing_in_trans());
		REQUIRE(result.has_initial(2));
		REQUIRE(result.has_initial(5));
		REQUIRE(result.has_final(1));
		REQUIRE(result.has_final(3));
	}

	SECTION("one-transition automaton")
	{
		aut.make_initial(1);
		aut.make_final(2);
		aut.add_trans(1, 'a', 2);

		Nfa result = revert(aut);

		REQUIRE(result.has_initial(2));
		REQUIRE(result.has_final(1));
		REQUIRE(result.has_trans(2, 'a', 1));
		REQUIRE(result.get_num_of_states() == aut.get_num_of_states());
	}

	SECTION("bigger automaton")
	{
		aut.initialstates = {1,2};
		aut.add_trans(1, 'a', 2);
		aut.add_trans(1, 'a', 3);
		aut.add_trans(1, 'b', 4);
		aut.add_trans(2, 'a', 2);
		aut.add_trans(2, 'a', 3);
		aut.add_trans(2, 'b', 4);
		aut.add_trans(3, 'b', 4);
		aut.add_trans(3, 'c', 7);
		aut.add_trans(3, 'b', 2);
		aut.add_trans(7, 'a', 8);
		aut.finalstates = {3};

		Nfa result = revert(aut);
		REQUIRE(result.finalstates == StateSet({1,2}));
		REQUIRE(result.has_trans(2, 'a', 1));
		REQUIRE(result.has_trans(3, 'a', 1));
		REQUIRE(result.has_trans(4, 'b', 1));
		REQUIRE(result.has_trans(2, 'a', 2));
		REQUIRE(result.has_trans(3, 'a', 2));
		REQUIRE(result.has_trans(4, 'b', 2));
		REQUIRE(result.has_trans(4, 'b', 3));
		REQUIRE(result.has_trans(7, 'c', 3));
		REQUIRE(result.has_trans(2, 'b', 3));
		REQUIRE(result.has_trans(8, 'a', 7));
		REQUIRE(result.initialstates == StateSet({3}));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_deterministic()")
{ // {{{
	Nfa aut('s'+1);

	SECTION("(almost) empty automaton")
	{
		// no initial states
		REQUIRE(!is_deterministic(aut));

		// add an initial state
		aut.make_initial('q');
		REQUIRE(is_deterministic(aut));

		// add the same initial state
		aut.make_initial('q');
		REQUIRE(is_deterministic(aut));

		// add another initial state
		aut.make_initial('r');
		REQUIRE(!is_deterministic(aut));

		// add a final state
		aut.make_final('q');
		REQUIRE(!is_deterministic(aut));
	}

	SECTION("trivial automata")
	{
		aut.make_initial('q');
		aut.add_trans('q', 'a', 'r');
		REQUIRE(is_deterministic(aut));

		// unreachable states
		aut.add_trans('s', 'a', 'r');
		REQUIRE(is_deterministic(aut));

		// transitions over a different symbol
		aut.add_trans('q', 'b', 'h');
		REQUIRE(is_deterministic(aut));

		// nondeterminism
		aut.add_trans('q', 'a', 's');
		REQUIRE(!is_deterministic(aut));
	}

	SECTION("larger automaton 1")
	{
		FILL_WITH_AUT_A(aut);
		REQUIRE(!is_deterministic(aut));
	}

	SECTION("larger automaton 2")
	{
		FILL_WITH_AUT_B(aut);
		REQUIRE(!is_deterministic(aut));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_complete()")
{ // {{{
	Nfa aut('q'+1);

	SECTION("empty automaton")
	{
		StringToSymbolMap ssmap;
		OnTheFlyAlphabet alph(&ssmap);

		// is complete for the empty alphabet
		REQUIRE(is_complete(aut, alph));

		alph.translate_symb("a1");
		alph.translate_symb("a2");

		// the empty automaton is complete even for a non-empty alphabet
		REQUIRE(is_complete(aut, alph));

		// add a non-reachable state (the automaton should still be complete)
		aut.add_trans('q', alph["a1"], 'q');
		REQUIRE(is_complete(aut, alph));
	}

	SECTION("small automaton")
	{
		StringToSymbolMap ssmap;
		OnTheFlyAlphabet alph(&ssmap);

		aut.make_initial(4);
		aut.add_trans(4, alph["a"], 8);
		aut.add_trans(4, alph["c"], 8);
		aut.add_trans(4, alph["a"], 6);
		aut.add_trans(4, alph["b"], 6);
		aut.add_trans(8, alph["b"], 4);
		aut.add_trans(6, alph["a"], 2);
		aut.add_trans(2, alph["b"], 2);
		aut.add_trans(2, alph["a"], 0);
		aut.add_trans(2, alph["c"], 12);
		aut.add_trans(0, alph["a"], 2);
		aut.add_trans(12, alph["a"], 14);
		aut.add_trans(14, alph["b"], 12);
		aut.make_final({2, 12});

		REQUIRE(!is_complete(aut, alph));

		make_complete(&aut, alph, 100);
		REQUIRE(is_complete(aut, alph));
	}

	SECTION("using a non-alphabet symbol")
	{
		StringToSymbolMap ssmap;
		OnTheFlyAlphabet alph(&ssmap);

		aut.make_initial(4);
		aut.add_trans(4, alph["a"], 8);
		aut.add_trans(4, alph["c"], 8);
		aut.add_trans(4, alph["a"], 6);
		aut.add_trans(4, alph["b"], 6);
		aut.add_trans(6, 100, 4);

		CHECK_THROWS_WITH(is_complete(aut, alph),
			Catch::Contains("symbol that is not in the provided alphabet"));
	}

	SECTION("a small automaton using CharAlphabet")
	{
		CharAlphabet alph;

        aut.make_initial(4);
		aut.add_trans(4, 'a', 8);
		aut.add_trans(4, 'c', 8);
		aut.add_trans(4, 'a', 6);
		aut.add_trans(4, 'b', 6);
		aut.add_trans(8, 'b', 4);
		aut.add_trans(6, 'a', 2);
		aut.add_trans(2, 'b', 2);
		aut.add_trans(2, 'a', 0);
		aut.add_trans(2, 'c', 12);
		aut.add_trans(0, 'a', 2);
		aut.add_trans(12, 'a', 14);
		aut.add_trans(14, 'b', 12);
		aut.make_final({2, 12});

		REQUIRE(!is_complete(aut, alph));

		make_complete(&aut, alph, 100);
		REQUIRE(is_complete(aut, alph));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_prfx_in_lang()")
{ // {{{
	Nfa aut('q'+1);

	SECTION("empty automaton")
	{
		Word w;
		w = {'a', 'b', 'd'};
		REQUIRE(!is_prfx_in_lang(aut, w));

		w = { };
		REQUIRE(!is_prfx_in_lang(aut, w));
	}

	SECTION("automaton accepting only epsilon")
	{
		aut.make_initial('q');
		aut.make_final('q');

		Word w;
		w = { };
		REQUIRE(is_prfx_in_lang(aut, w));

		w = {'a', 'b'};
		REQUIRE(is_prfx_in_lang(aut, w));
	}

	SECTION("small automaton")
	{
		FILL_WITH_AUT_B(aut);

		Word w;
		w = {'b', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w = { };
		REQUIRE(!is_prfx_in_lang(aut, w));

		w = {'c', 'b', 'a'};
		REQUIRE(!is_prfx_in_lang(aut, w));

		w = {'c', 'b', 'a', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w = {'a', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w = {'c', 'b', 'b', 'a', 'c', 'b'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w = Word(100000, 'a');
		REQUIRE(is_prfx_in_lang(aut, w));

		w = Word(100000, 'b');
		REQUIRE(!is_prfx_in_lang(aut, w));
	}
} // }}}

TEST_CASE("Mata::Nfa::fw-direct-simulation()")
{ // {{{
    Nfa aut;

    SECTION("empty automaton")
    {
        Simlib::Util::BinaryRelation result = compute_relation(aut);

        REQUIRE(result.size() == 0);
    }

    aut.increase_size(9);
    SECTION("no-transition automaton")
    {
        aut.make_initial(1);
        aut.make_initial(3);

        aut.make_final(2);
        aut.make_final(5);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(1,3));
        REQUIRE(result.get(2,5));
        REQUIRE(!result.get(5,1));
        REQUIRE(!result.get(2,3));
    }

    SECTION("small automaton")
    {
        aut.make_initial(1);
        aut.make_final(2);
        aut.add_trans(1, 'a', 4);
        aut.add_trans(4, 'b', 5);
        aut.add_trans(2, 'b', 5);
        aut.add_trans(1, 'b', 4);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(4,1));
        REQUIRE(!result.get(2,5));

    }

    Nfa aut_big(9);

    SECTION("bigger automaton")
    {
        aut_big.initialstates = {1,2};
        aut_big.add_trans(1, 'a', 2);
        aut_big.add_trans(1, 'a', 3);
        aut_big.add_trans(1, 'b', 4);
        aut_big.add_trans(2, 'a', 2);
        aut_big.add_trans(2, 'b', 2);
        aut_big.add_trans(2, 'a', 3);
        aut_big.add_trans(2, 'b', 4);
        aut_big.add_trans(3, 'b', 4);
        aut_big.add_trans(3, 'c', 7);
        aut_big.add_trans(3, 'b', 2);
        aut_big.add_trans(5, 'c', 3);
        aut_big.add_trans(7, 'a', 8);
        aut_big.finalstates = {3};

        Simlib::Util::BinaryRelation result = compute_relation(aut_big);
        REQUIRE(result.get(1,2));
        REQUIRE(!result.get(2,1));
        REQUIRE(!result.get(3,1));
        REQUIRE(!result.get(3,2));
        REQUIRE(result.get(4,1));
        REQUIRE(result.get(4,2));
        REQUIRE(result.get(4,5));
        REQUIRE(!result.get(5,2));
        REQUIRE(!result.get(5,1));
        REQUIRE(result.get(7,1));
        REQUIRE(result.get(7,2));
        REQUIRE(result.get(8,1));
        REQUIRE(result.get(8,2));
        REQUIRE(result.get(8,5));
    }
} // }}

TEST_CASE("Mata::Nfa::reduce_size_by_simulation()")
{
	Nfa aut;
	StateMap<State> state_map;

	SECTION("empty automaton")
	{
		Nfa result = reduce(aut, &state_map);

		REQUIRE(result.nothing_in_trans());
		REQUIRE(result.initialstates.size() == 0);
		REQUIRE(result.finalstates.size() == 0);
	}

	SECTION("simple automaton")
	{
		aut.increase_size(3);
		aut.make_initial(1);

		aut.make_final(2);
		Nfa result = reduce(aut, &state_map);

		REQUIRE(result.nothing_in_trans());
		REQUIRE(result.has_initial(state_map[1]));
		REQUIRE(result.has_final(state_map[2]));
		REQUIRE(result.get_num_of_states() == 2);
		REQUIRE(state_map[1] == state_map[0]);
		REQUIRE(state_map[2] != state_map[0]);
	}

	SECTION("big automaton")
	{
		aut.increase_size(10);
		aut.initialstates = {1,2};
		aut.add_trans(1, 'a', 2);
		aut.add_trans(1, 'a', 3);
		aut.add_trans(1, 'b', 4);
		aut.add_trans(2, 'a', 2);
		aut.add_trans(2, 'b', 2);
		aut.add_trans(2, 'a', 3);
		aut.add_trans(2, 'b', 4);
		aut.add_trans(3, 'b', 4);
		aut.add_trans(3, 'c', 7);
		aut.add_trans(3, 'b', 2);
		aut.add_trans(5, 'c', 3);
		aut.add_trans(7, 'a', 8);
		aut.add_trans(9, 'b', 2);
		aut.add_trans(9, 'c', 0);
		aut.add_trans(0, 'a', 4);
		aut.finalstates = {3, 9};


		Nfa result = reduce(aut, &state_map);

		REQUIRE(result.get_num_of_states() == 6);
		REQUIRE(result.has_initial(state_map[1]));
		REQUIRE(result.has_initial(state_map[2]));
		REQUIRE(result.has_trans(state_map[9], 'c', state_map[0]));
		REQUIRE(result.has_trans(state_map[9], 'c', state_map[7]));
		REQUIRE(result.has_trans(state_map[3], 'c', state_map[0]));
		REQUIRE(result.has_trans(state_map[0], 'a', state_map[8]));
		REQUIRE(result.has_trans(state_map[7], 'a', state_map[4]));
		REQUIRE(result.has_trans(state_map[1], 'a', state_map[3]));
		REQUIRE(!result.has_trans(state_map[3], 'b', state_map[4]));
		REQUIRE(result.has_trans(state_map[2], 'a', state_map[2]));
		REQUIRE(result.has_final(state_map[9]));
		REQUIRE(result.has_final(state_map[3]));
	}
}

TEST_CASE("Mata::Nfa::union_norename()") {
    Word one{1};
    Word zero{0};

    Nfa lhs(2);
    lhs.make_initial(0);
    lhs.add_trans(0, 0, 1);
    lhs.make_final(1);
    REQUIRE(!is_in_lang(lhs, one));
    REQUIRE(is_in_lang(lhs, zero));

    Nfa rhs(2);
    rhs.make_initial(0);
    rhs.add_trans(0, 1, 1);
    rhs.make_final(1);
    REQUIRE(is_in_lang(rhs, one));
    REQUIRE(!is_in_lang(rhs, zero));

    SECTION("failing minimal scenario") {
        Nfa result;
        uni(&result, lhs, rhs);
        REQUIRE(is_in_lang(result, one));
        REQUIRE(is_in_lang(result, zero));
    }
}

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
        REQUIRE(aut.get_shortest_words() == expected);

        SECTION("Additional initial state with longer words")
        {
            aut.initialstates.push_back(8);
            REQUIRE(aut.get_shortest_words() == expected);
        }

        SECTION("Change initial state")
        {
			aut.initialstates.clear();
            aut.initialstates.push_back(8);

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

            REQUIRE(aut.get_shortest_words() == expected);
        }
    }

    SECTION("Empty automaton")
    {
        REQUIRE(aut.get_shortest_words().empty());
    }

    SECTION("One-state automaton accepting an empty language")
    {
        aut.make_initial(0);
        REQUIRE(aut.get_shortest_words().empty());
        aut.make_final(1);
        REQUIRE(aut.get_shortest_words().empty());
        aut.make_final(0);
        REQUIRE(aut.get_shortest_words() == WordSet{Word{}});
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
        REQUIRE(aut.get_shortest_words() == expected);
    }

    SECTION("Single transition automaton")
    {
        aut.initialstates = { 1 };
        aut.finalstates = { 2 };
        aut.add_trans(1, 'a', 2);

        REQUIRE(aut.get_shortest_words() == std::set<Word>{Word{'a'}});
    }

    SECTION("Single state automaton")
    {
        aut.initialstates = { 1 };
        aut.finalstates = { 1 };
        aut.add_trans(1, 'a', 1);

        REQUIRE(aut.get_shortest_words() == std::set<Word>{Word{}});
    }

    SECTION("Require FIFO queue")
    {
        aut.initialstates = { 1 };
        aut.finalstates = { 4 };
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
        REQUIRE(aut.get_shortest_words() == expected);
    }
}

TEST_CASE("Mata::Nfa::remove_final()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        REQUIRE(aut.has_final(2));
        REQUIRE(aut.has_final(12));
        aut.remove_final(12);
        REQUIRE(aut.has_final(2));
        REQUIRE(!aut.has_final(12));
    }
}

TEST_CASE("Mata::Nfa::remove_trans()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.add_trans(1, 3, 4);
        aut.add_trans(1, 3, 5);

        SECTION("Simple remove")
        {
            REQUIRE(aut.has_trans(1, 3, 4));
            REQUIRE(aut.has_trans(1, 3, 5));
            aut.remove_trans(1, 3, 5);
            REQUIRE(aut.has_trans(1, 3, 4));
            REQUIRE(!aut.has_trans(1, 3, 5));
        }

        SECTION("Remove missing transition")
        {
            REQUIRE_THROWS_AS(aut.remove_trans(1, 1, 5), std::invalid_argument);
        }

        SECTION("Remove the last state_to from states_to")
        {
            REQUIRE(aut.has_trans(6, 'a', 2));
            aut.remove_trans(6, 'a', 2);
            REQUIRE(!aut.has_trans(6, 'a', 2));
            REQUIRE(aut.transitionrelation[6].empty());

            REQUIRE(aut.has_trans(4, 'a', 8));
            REQUIRE(aut.has_trans(4, 'c', 8));
            REQUIRE(aut.has_trans(4, 'a', 6));
            REQUIRE(aut.has_trans(4, 'b', 6));
            REQUIRE(aut.transitionrelation[4].size() == 3);
            aut.remove_trans(4, 'a', 6);
            REQUIRE(!aut.has_trans(4, 'a', 6));
            REQUIRE(aut.has_trans(4, 'b', 6));
            REQUIRE(aut.transitionrelation[4].size() == 3);

            aut.remove_trans(4, 'a', 8);
            REQUIRE(!aut.has_trans(4, 'a', 8));
            REQUIRE(aut.has_trans(4, 'c', 8));
            REQUIRE(aut.transitionrelation[4].size() == 2);

            aut.remove_trans(4, 'c', 8);
            REQUIRE(!aut.has_trans(4, 'a', 8));
            REQUIRE(!aut.has_trans(4, 'c', 8));
            REQUIRE(aut.transitionrelation[4].size() == 1);
        }
    }
}

TEST_CASE("Mafa::Nfa::get_transitions_from_state()")
{
    Nfa aut{};

    SECTION("Add new states within the limit")
    {
        aut.increase_size(20);
        aut.make_initial(0);
        aut.make_initial(1);
        aut.make_initial(2);
        REQUIRE_NOTHROW(aut.get_transitions_from_state(0));
        REQUIRE_NOTHROW(aut.get_transitions_from_state(1));
        REQUIRE_NOTHROW(aut.get_transitions_from_state(2));
        REQUIRE(aut.get_transitions_from_state(0).empty());
        REQUIRE(aut.get_transitions_from_state(1).empty());
        REQUIRE(aut.get_transitions_from_state(2).empty());
    }

    SECTION("Add new states over the limit")
    {
        aut.increase_size(2);
        REQUIRE_NOTHROW(aut.make_initial(0));
        REQUIRE_NOTHROW(aut.make_initial(1));
        REQUIRE_THROWS_AS(aut.make_initial(2), std::runtime_error);
        REQUIRE_NOTHROW(aut.get_transitions_from_state(0));
        REQUIRE_NOTHROW(aut.get_transitions_from_state(1));
        //REQUIRE_THROWS(aut.get_transitions_from_state(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
        REQUIRE(aut.get_transitions_from_state(0).empty());
        REQUIRE(aut.get_transitions_from_state(1).empty());
        //REQUIRE_THROWS(aut.get_transitions_from_state(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    }

    SECTION("Add new states without specifying the number of states")
    {
        REQUIRE_THROWS_AS(aut.make_initial(0), std::runtime_error);
        //REQUIRE_THROWS(aut.get_transitions_from_state(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    }

    SECTION("Add new initial without specifying the number of states with over +1 number")
    {
        REQUIRE_THROWS_AS(aut.make_initial(25), std::runtime_error);
        //REQUIRE_THROWS(aut.get_transitions_from_state(25)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    }
}


TEST_CASE("Mata::Nfa::get_trans_as_sequence(}")
{
    Nfa aut('q' + 1);
    TransSequence expected{};

    aut.add_trans(1, 2, 3);
    expected.push_back(Trans{1, 2, 3});
    aut.add_trans(1, 3, 4);
    expected.push_back(Trans{1, 3, 4});
    aut.add_trans(2, 3, 4);
    expected.push_back(Trans{2, 3, 4});


    REQUIRE(aut.get_trans_as_sequence() == expected);
}

TEST_CASE("Mata::Nfa::Segmentation::get_epsilon_depths()")
{
    Nfa aut('q' + 1);
    constexpr Symbol epsilon{'c'};

    SECTION("Automaton A")
    {
        FILL_WITH_AUT_A(aut);
        auto segmentation{SegNfa::Segmentation{aut, epsilon } };
        const auto& epsilon_depth_transitions{ segmentation.get_epsilon_depths() };
        REQUIRE(epsilon_depth_transitions == SegNfa::Segmentation::EpsilonDepthTransitions{{0, std::vector<Trans>{
                {10, epsilon, 7}, {7, epsilon, 3}, {5, epsilon, 9}}
       }});
    }

    SECTION("Small automaton with depths")
    {
        aut.make_initial(1);
        aut.make_final(8);
        aut.add_trans(1, epsilon, 2);
        aut.add_trans(2, 'a', 3);
        aut.add_trans(2, 'b', 4);
        aut.add_trans(3, 'b', 6);
        aut.add_trans(4, 'a', 6);
        aut.add_trans(6, epsilon, 7);
        aut.add_trans(7, epsilon, 8);

        auto segmentation{SegNfa::Segmentation{aut, epsilon } };
        const auto& epsilon_depth_transitions{ segmentation.get_epsilon_depths() };

        REQUIRE(epsilon_depth_transitions == SegNfa::Segmentation::EpsilonDepthTransitions{
                {0, TransSequence{{1, epsilon, 2}}},
                {1, TransSequence{{6, epsilon, 7}}},
                {2, TransSequence{{7, epsilon, 8}}},
        });
    }

}

TEST_CASE("Mata::Nfa::remove_epsilon()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.remove_epsilon('c');
    REQUIRE(aut.has_trans(10, 'a', 7));
    REQUIRE(aut.has_trans(10, 'b', 7));
    REQUIRE(!aut.has_trans(10, 'c', 7));
    REQUIRE(aut.has_trans(7, 'a', 5));
    REQUIRE(aut.has_trans(7, 'a', 3));
    REQUIRE(!aut.has_trans(7, 'c', 3));
    REQUIRE(aut.has_trans(7, 'b', 9));
    REQUIRE(aut.has_trans(7, 'a', 7));
    REQUIRE(aut.has_trans(5, 'a', 5));
    REQUIRE(!aut.has_trans(5, 'c', 9));
    REQUIRE(aut.has_trans(5, 'a', 9));
}

TEST_CASE("Mata::Nfa::get_num_of_trans()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.get_num_of_trans() == 15);
}

TEST_CASE("Mata::Nfa::Segmentation::split_segment_automaton()")
{
    Nfa aut(100);
    aut.make_initial(1);
    aut.make_final(11);
    aut.add_trans(1, 'a', 2);
    aut.add_trans(1, 'b', 3);
    aut.add_trans(3, 'c', 4);
    aut.add_trans(4, 'a', 7);
    aut.add_trans(7, 'b', 8);
    aut.add_trans(8, 'a', 7);
    aut.add_trans(8, 'b', 4);
    aut.add_trans(4, 'c', 5);
    aut.add_trans(5, 'a', 6);
    aut.add_trans(5, 'b', 6);
    aut.add_trans(6, 'c', 10);
    aut.add_trans(9, 'a', 11);
    aut.add_trans(10, 'b', 11);

    auto segmentation{SegNfa::Segmentation{aut, 'c'}};
    auto segments{ segmentation.get_segments() };
    REQUIRE(segments.size() == 4);

    REQUIRE(segments[0].has_initial(0));
    REQUIRE(segments[0].has_final(1));
    REQUIRE(segments[0].has_trans(0, 'b', 1));
    REQUIRE(!segments[0].has_trans(0, 'a', 2));

    REQUIRE(segments[1].has_initial(0));
    REQUIRE(segments[1].has_final(0));
    REQUIRE(segments[1].has_trans(0, 'a', 1));
    REQUIRE(!segments[1].has_trans(0, 'a', 2));
    REQUIRE(!segments[1].has_trans(0, 'c', 3));
    REQUIRE(segments[1].has_trans(1, 'b', 2));
    REQUIRE(segments[1].has_trans(2, 'b', 0));
    REQUIRE(segments[1].has_trans(2, 'a', 1));

    REQUIRE(segments[2].has_initial(0));
    REQUIRE(segments[2].has_final(1));
    REQUIRE(segments[2].has_trans(0, 'a', 1));
    REQUIRE(segments[2].has_trans(0, 'b', 1));

    REQUIRE(segments[3].has_initial(0));
    REQUIRE(segments[3].has_final(1));
    REQUIRE(segments[3].has_trans(0, 'b', 1));
}

TEST_CASE("Mata::Nfa::get_digraph()")
{
    Nfa aut(100);
    Symbol abstract_symbol{'x'};
    FILL_WITH_AUT_A(aut);

    Nfa digraph{ aut.get_digraph() };

    REQUIRE(digraph.get_num_of_states() == aut.get_num_of_states());
    REQUIRE(digraph.get_num_of_trans() == 12);
    REQUIRE(digraph.has_trans(1, abstract_symbol, 10));
    REQUIRE(digraph.has_trans(10, abstract_symbol, 7));
    REQUIRE(!digraph.has_trans(10, 'a', 7));
    REQUIRE(!digraph.has_trans(10, 'b', 7));
    REQUIRE(!digraph.has_trans(10, 'c', 7));
}

TEST_CASE("Mata::Nfa::get_reachable_states()")
{
    Nfa aut{20};

    SECTION("Automaton A")
    {
        FILL_WITH_AUT_A(aut);
        aut.remove_trans(3, 'b', 9);
        aut.remove_trans(5, 'c', 9);
        aut.remove_trans(1, 'a', 10);

        auto reachable{aut.get_reachable_states()};
        CHECK(reachable.find(0) == reachable.end());
        CHECK(reachable.find(1) != reachable.end());
        CHECK(reachable.find(2) == reachable.end());
        CHECK(reachable.find(3) != reachable.end());
        CHECK(reachable.find(4) == reachable.end());
        CHECK(reachable.find(5) != reachable.end());
        CHECK(reachable.find(6) == reachable.end());
        CHECK(reachable.find(7) != reachable.end());
        CHECK(reachable.find(8) == reachable.end());
        CHECK(reachable.find(9) == reachable.end());
        CHECK(reachable.find(10) == reachable.end());

        aut.remove_initial(1);
        aut.remove_initial(3);

        reachable = aut.get_reachable_states();
        CHECK(reachable.empty());
    }

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.remove_trans(2, 'c', 12);
        aut.remove_trans(4, 'c', 8);
        aut.remove_trans(4, 'a', 8);

        auto reachable{aut.get_reachable_states()};
        CHECK(reachable.find(0) != reachable.end());
        CHECK(reachable.find(1) == reachable.end());
        CHECK(reachable.find(2) != reachable.end());
        CHECK(reachable.find(3) == reachable.end());
        CHECK(reachable.find(4) != reachable.end());
        CHECK(reachable.find(5) == reachable.end());
        CHECK(reachable.find(6) != reachable.end());
        CHECK(reachable.find(7) == reachable.end());
        CHECK(reachable.find(8) == reachable.end());
        CHECK(reachable.find(9) == reachable.end());
        CHECK(reachable.find(10) == reachable.end());
        CHECK(reachable.find(11) == reachable.end());
        CHECK(reachable.find(12) == reachable.end());
        CHECK(reachable.find(13) == reachable.end());
        CHECK(reachable.find(14) == reachable.end());

        aut.remove_final(2);
        reachable = aut.get_reachable_states();
        CHECK(reachable.size() == 4);
        CHECK(reachable.find(0) != reachable.end());
        CHECK(reachable.find(2) != reachable.end());
        CHECK(reachable.find(4) != reachable.end());
        CHECK(reachable.find(6) != reachable.end());
        CHECK(aut.get_useful_states().empty());

        aut.make_final(4);
        reachable = aut.get_reachable_states();
        CHECK(reachable.find(4) != reachable.end());
    }
}

TEST_CASE("Mata::Nfa::trim()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.remove_trans(1, 'a', 10);

    Nfa old_aut{aut};

    aut.trim();
    CHECK(aut.initialstates.size() == old_aut.initialstates.size());
    CHECK(aut.finalstates.size() == old_aut.finalstates.size());
    CHECK(aut.get_num_of_states() == 4);
    for (const Word& word: old_aut.get_shortest_words())
    {
        CHECK(is_in_lang(aut, word));
    }

    aut.remove_final(2); // '2' is the new final state in the earlier trimmed automaton.
    aut.trim();
    CHECK(aut.trans_empty());
    CHECK(aut.get_num_of_states() == 0);
}

TEST_CASE("Mata::Nfa::Nfa::trans_empty()")
{
    Nfa aut{};

    SECTION("Empty automaton")
    {
        CHECK(aut.trans_empty());
    }

    SECTION("No transitions automaton")
    {
        aut.increase_size(1);
        CHECK(aut.trans_empty());
    }

    SECTION("Single state automaton with no transitions")
    {
        aut.increase_size(1);
        aut.make_initial(0);
        aut.make_final(0);
        CHECK(aut.trans_empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.increase_size(1);
        aut.make_initial(0);
        aut.make_final(0);
        aut.add_trans(0, 'a', 0);
        CHECK(!aut.trans_empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.increase_size(2);
        aut.make_initial(0);
        aut.make_final(1);
        CHECK(aut.trans_empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.increase_size(2);
        aut.make_initial(0);
        aut.make_final(1);
        aut.add_trans(0, 'a', 1);
        CHECK(!aut.trans_empty());
    }
}

TEST_CASE("Mata::Nfa::concatenate()")
{
    Nfa lhs{};
    Nfa rhs{};
    Nfa result{};

    SECTION("Empty automaton")
    {
        lhs.increase_size(1);
        rhs.increase_size(1);
        result = concatenate(lhs, rhs);

        CHECK(result.get_num_of_states() == 2);
        CHECK(result.initialstates.empty());
        CHECK(result.finalstates.empty());
        CHECK(result.trans_empty());
        CHECK(is_lang_empty(result));
    }

    SECTION("Empty language")
    {
        lhs.increase_size(1);
        lhs.make_initial(0);
        rhs.increase_size(1);
        rhs.make_initial(0);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.finalstates.empty());
        CHECK(result.get_num_of_states() == 2);
        CHECK(result.trans_empty());
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.increase_size(1);
        lhs.make_initial(0);
        lhs.make_final(0);
        rhs.increase_size(1);
        rhs.make_initial(0);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.finalstates.empty());
        CHECK(result.get_num_of_states() == 1);
        CHECK(result.trans_empty());
    }

    SECTION("Single state automata accepting an empty string")
    {
        lhs.increase_size(1);
        lhs.make_initial(0);
        lhs.make_final(0);
        rhs.increase_size(1);
        rhs.make_initial(0);
        rhs.make_final(0);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(0));
        CHECK(result.get_num_of_states() == 1);
        CHECK(result.trans_empty());
    }

    SECTION("Empty language rhs automaton")
    {
        lhs.increase_size(1);
        lhs.make_initial(0);
        lhs.make_final(0);
        rhs.increase_size(2);
        rhs.make_initial(0);
        rhs.make_final(1);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(1));
        CHECK(result.get_num_of_states() == 2);
        CHECK(result.trans_empty());
    }

    SECTION("Simple two state rhs automaton")
    {
        lhs.increase_size(1);
        lhs.make_initial(0);
        lhs.make_final(0);
        rhs.increase_size(2);
        rhs.make_initial(0);
        rhs.make_final(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(1));
        CHECK(result.get_num_of_states() == 2);
        CHECK(result.has_trans(0, 'a', 1));
    }

    SECTION("Simple two state automata")
    {
        lhs.increase_size(2);
        lhs.make_initial(0);
        lhs.make_final(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(2);
        rhs.make_initial(0);
        rhs.make_final(1);
        rhs.add_trans(0, 'a', 1);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(2));
        CHECK(result.get_num_of_states() == 3);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 2));

        auto shortest_words{result.get_shortest_words()};
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state automata with higher state num for non-final state")
    {
        lhs.increase_size(2);
        lhs.make_initial(0);
        lhs.make_final(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(4);
        rhs.make_initial(0);
        rhs.make_final(1);
        rhs.add_trans(0, 'a', 1);
        rhs.add_trans(0, 'c', 3);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(2));
        CHECK(result.get_num_of_states() == 5);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 2));

        auto shortest_words{result.get_shortest_words()};
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Simple two state lhs automaton")
    {
        lhs.increase_size(2);
        lhs.make_initial(0);
        lhs.make_final(1);
        lhs.add_trans(0, 'b', 1);
        rhs.increase_size(1);
        rhs.make_initial(0);
        rhs.make_final(0);
        rhs.add_trans(0, 'a', 0);

        result = concatenate(lhs, rhs);

        CHECK(result.has_initial(0));
        CHECK(result.has_final(1));
        CHECK(result.get_num_of_states() == 2);
        CHECK(result.has_trans(0, 'b', 1));
        CHECK(result.has_trans(1, 'a', 1));

        auto shortest_words{result.get_shortest_words()};
        CHECK(shortest_words.size() == 1);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b' }) != shortest_words.end());
    }

    SECTION("Automaton A concatenate automaton B")
    {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(lhs, rhs);

        CHECK(result.initialstates.size() == 2);
        CHECK(result.has_initial(1));
        CHECK(result.has_initial(3));

        CHECK(result.get_num_of_states() == 25);

        auto shortest_words{result.get_shortest_words()};
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }

    SECTION("Automaton B concatenate automaton A")
    {
        lhs.increase_size_for_state(10);
        FILL_WITH_AUT_A(lhs);
        rhs.increase_size_for_state(14);
        FILL_WITH_AUT_B(rhs);

        result = concatenate(rhs, lhs);

        CHECK(result.get_num_of_states() == 24);

        CHECK(result.initialstates.size() == 1);
        // Final state 2 in automaton B will not stay in the result automaton.
        // Hence, initial state 4 in aut B will be initial state 3 in the result.
        CHECK(result.has_initial(3));

        auto shortest_words{result.get_shortest_words()};
        CHECK(shortest_words.size() == 4);
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'b', 'a', 'b', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'a', 'a' }) != shortest_words.end());
        CHECK(shortest_words.find(std::vector<Symbol>{ 'a', 'a', 'b', 'a' }) != shortest_words.end());
    }
}
