// TODO: some header

#include "../3rdparty/catch.hpp"

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
	b.initialstates = {4}; \
	b.finalstates = {2, 12}; \
	b.add_trans(4, 'c', 8); \
	b.add_trans(4, 'a', 8); \
	b.add_trans(8, 'b', 4); \
	b.add_trans(4, 'a', 6); \
	b.add_trans(4, 'b', 6); \
	b.add_trans(6, 'a', 2); \
	b.add_trans(2, 'b', 2); \
	b.add_trans(2, 'a', 0); \
	b.add_trans(0, 'a', 2); \
	b.add_trans(2, 'c', 12); \
	b.add_trans(12, 'a', 14); \
	b.add_trans(14, 'b', 12); \

// }}}

#include <vata2/nfa.hh>
using namespace Vata2::Nfa;

TEST_CASE("Vata2::Nfa::Nfa::add_trans()/has_trans()")
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

TEST_CASE("Vata2::Nfa::are_state_disjoint()")
{ // {{{
	Nfa a, b;

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
		b.add_trans(0, 'c', 394093820488);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Right-hand side empty automaton is state disjoint with anything")
	{
		a.initialstates = {1, 4, 6};
		a.finalstates = {4, 7, 9, 0};
		a.add_trans(1, 'a', 1);
		a.add_trans(2, 'a', 8);
		a.add_trans(0, 'c', 394093820488);

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


TEST_CASE("Vata2::Nfa::intersection()")
{ // {{{
	Nfa a, b, res;
	ProductMap prod_map;

	SECTION("Intersection of empty automata")
	{
		intersection(&res, a, b, &prod_map);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.transitions.empty());
		REQUIRE(prod_map.empty());
	}

	SECTION("Intersection of empty automata 2")
	{
		intersection(&res, a, b);

		REQUIRE(res.initialstates.empty());
		REQUIRE(res.finalstates.empty());
		REQUIRE(res.transitions.empty());
	}

	SECTION("Intersection of automata with no transitions")
	{
		a.initialstates = {1, 3};
		a.finalstates = {3, 5};

		b.initialstates = {4, 6};
		b.finalstates = {4, 2};

		intersection(&res, a, b, &prod_map);

		REQUIRE(!res.initialstates.empty());
		REQUIRE(!res.finalstates.empty());

		State init_fin_st = prod_map[{3, 4}];

		REQUIRE(res.has_initial(init_fin_st));
		REQUIRE(res.has_final(init_fin_st));
	}

	SECTION("Intersection of automata with some transitions")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		intersection(&res, a, b, &prod_map);

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

TEST_CASE("Vata2::Nfa::is_lang_empty()")
{
	Nfa aut;
	Word cex;

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
}

TEST_CASE("Vata2::Nfa::get_word_for_path()")
{ // {{{
	Nfa aut;
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
		REQUIRE(possible.find(word_bool_pair.first) != possible.end());
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

TEST_CASE("Vata2::Nfa::determinize()")
{
	Nfa aut;
	Nfa result;
	SubsetMap subset_map;

	SECTION("empty automaton")
	{
		determinize(&result, aut);

		REQUIRE(result.has_initial(subset_map[{}]));
		REQUIRE(result.finalstates.empty());
		REQUIRE(result.transitions.empty());
	}

	SECTION("simple automaton 1")
	{
		aut.initialstates = { 1 };
		aut.finalstates = { 1 };
		determinize(&result, aut, &subset_map);

		REQUIRE(result.has_initial(subset_map[{1}]));
		REQUIRE(result.has_final(subset_map[{1}]));
		REQUIRE(result.transitions.empty());
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
}


TEST_CASE("Vata2::Nfa::construct() correct calls")
{
	Nfa aut;
	Vata2::Parser::Parsed parsed;
	StringToSymbolMap symbol_map;

	SECTION("construct an empty automaton")
	{
		parsed.type = "NFA";

		construct(&aut, parsed);

		REQUIRE(is_lang_empty(aut));
	}

	SECTION("construct a simple non-empty automaton accepting the empty word")
	{
		parsed.type = "NFA";
		parsed.dict.insert({"Initial", {"q1"}});
		parsed.dict.insert({"Final", {"q1"}});

		construct(&aut, parsed);

		REQUIRE(!is_lang_empty(aut));
	}

	SECTION("construct an automaton with more than one initial/final states")
	{
		parsed.type = "NFA";
		parsed.dict.insert({"Initial", {"q1", "q2"}});
		parsed.dict.insert({"Final", {"q1", "q2", "q3"}});

		construct(&aut, parsed);

		REQUIRE(aut.initialstates.size() == 2);
		REQUIRE(aut.finalstates.size() == 3);
	}

	SECTION("construct a simple non-empty automaton accepting only the word 'a'")
	{
		parsed.type = "NFA";
		parsed.dict.insert({"Initial", {"q1"}});
		parsed.dict.insert({"Final", {"q2"}});
		parsed.trans_list = { {"q1", "a", "q2"} };

		construct(&aut, parsed, &symbol_map);

		Path cex;
		REQUIRE(!is_lang_empty(aut, &cex));
		auto word_bool_pair = get_word_for_path(aut, cex);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first == encode_word(symbol_map, {"a"}));

		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a"})));
	}

	SECTION("construct a more complicated non-empty automaton")
	{
		parsed.type = "NFA";
		parsed.dict.insert({"Initial", {"q1", "q3"}});
		parsed.dict.insert({"Final", {"q5"}});
		parsed.trans_list.push_back({"q1", "a", "q3"});
		parsed.trans_list.push_back({"q1", "a", "q10"});
		parsed.trans_list.push_back({"q1", "b", "q7"});
		parsed.trans_list.push_back({"q3", "a", "q7"});
		parsed.trans_list.push_back({"q3", "b", "q9"});
		parsed.trans_list.push_back({"q9", "a", "q9"});
		parsed.trans_list.push_back({"q7", "b", "q1"});
		parsed.trans_list.push_back({"q7", "a", "q3"});
		parsed.trans_list.push_back({"q7", "c", "q3"});
		parsed.trans_list.push_back({"q10", "a", "q7"});
		parsed.trans_list.push_back({"q10", "b", "q7"});
		parsed.trans_list.push_back({"q10", "c", "q7"});
		parsed.trans_list.push_back({"q7", "a", "q5"});
		parsed.trans_list.push_back({"q5", "a", "q5"});
		parsed.trans_list.push_back({"q5", "c", "q9"});

		construct(&aut, parsed, &symbol_map);

		// some samples
		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"b", "a"})));
		REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a", "c", "a", "a"})));
		REQUIRE(is_in_lang(aut, encode_word(symbol_map,
			{"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a"})));
		// some wrong samples
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"b", "c"})));
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"a", "c", "c", "a"})));
		REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"b", "a", "c", "b"})));

		// assert(false);
	}
} // }}}


TEST_CASE("Vata2::Nfa::construct() invalid calls")
{ // {{{
	Nfa aut;
	Vata2::Parser::Parsed parsed;

	SECTION("construct() call with invalid Parsed object")
	{
		parsed.type = "FA";

		CHECK_THROWS_WITH(construct(&aut, parsed),
			Catch::Contains("expecting type"));
	}

	SECTION("construct() call with an epsilon transition")
	{
		parsed.type = "NFA";
		parsed.trans_list = { {"q1", "q2"}};

		CHECK_THROWS_WITH(construct(&aut, parsed),
			Catch::Contains("Epsilon transition"));
	}

	SECTION("construct() call with a nonsense transition")
	{
		parsed.type = "NFA";
		parsed.trans_list = { {"q1", "a", "q2", "q3"}};

		CHECK_THROWS_WITH(construct(&aut, parsed),
			Catch::Contains("Invalid transition"));
	}
} // }}}
