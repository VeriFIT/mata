// TODO: some header

#include <unordered_set>

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/nfa-plumbing.hh>
#include <mata/nfa-strings.hh>
#include <mata/nfa-algorithms.hh>
#include <mata/re2parser.hh>

using namespace Mata::Nfa::Algorithms;
using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::Nfa::Plumbing;
using namespace Mata::Util;
using namespace Mata::Parser;
using Symbol = Mata::Symbol;
using IntAlphabet = Mata::IntAlphabet;
using OnTheFlyAlphabet = Mata::OnTheFlyAlphabet;
using StringToSymbolMap = Mata::StringToSymbolMap;

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

template<class T> void unused(const T &) {}

//TODO: we have already a method for this in nfa.hh - has_not_transitions, right?
bool nothing_in_trans(const Nfa& nfa)
{
    bool all_empty = true;
    for (size_t i = 0; i < nfa.size(); ++i) {
        all_empty &= nfa.delta[i].empty();
    }

    return all_empty;
}

TEST_CASE("Mata::Nfa::size()") {
    Nfa nfa{};
    CHECK(nfa.size() == 0);

    nfa.add_state(3);
    CHECK(nfa.size() == 4);

    nfa.clear();
    nfa.add_state();
    CHECK(nfa.size() == 1);

    nfa.clear();
    FILL_WITH_AUT_A(nfa);
    CHECK(nfa.size() == 11);

    nfa.clear();
    FILL_WITH_AUT_B(nfa);
    CHECK(nfa.size() == 15);

    nfa = Nfa{ 0, {}, {} };
    CHECK(nfa.size() == 0);
}

/*
TEST_CASE("Mata::Nfa::Trans::operator<<")
{ // {{{
	Trans trans(1, 2, 3);

	REQUIRE(std::to_string(trans) == "(1, 2, 3)");
} // }}}
*/

TEST_CASE("Mata::Nfa::IntAlphabet") {
    auto alphabet1 = IntAlphabet();
    auto alphabet2 = IntAlphabet();
    CHECK(alphabet1.is_equal(alphabet2));

    auto& alphabet3 = alphabet2;
    CHECK(alphabet3.is_equal(&alphabet1));
    const auto& alphabet4 = alphabet2;
    CHECK(alphabet4.is_equal(alphabet1));

    OnTheFlyAlphabet different_alphabet{};
    OnTheFlyAlphabet different_alphabet2{};
    CHECK(!alphabet1.is_equal(different_alphabet));
    CHECK(!different_alphabet.is_equal(different_alphabet2));
    CHECK(different_alphabet.is_equal(&different_alphabet));
}

TEST_CASE("Mata::Nfa::create_alphabet()") {
    Nfa a{1};
    a.delta.add(0, 'a', 0);

    Nfa b{1};
    b.delta.add(0, 'b', 0);
    b.delta.add(0, 'a', 0);
    Nfa c{1};
    b.delta.add(0, 'c', 0);

    auto alphabet{Mata::Nfa::create_alphabet(a, b, c) };

    auto symbols{alphabet.get_alphabet_symbols() };
    CHECK(symbols == Mata::Util::OrdVector<Symbol>{ 'c', 'b', 'a' });

    //Mata::Nfa::create_alphabet(1, 3, 4); // Will not compile: '1', '3', '4' are not of the required type.
    //Mata::Nfa::create_alphabet(a, b, 4); // Will not compile: '4' is not of the required type.
}

TEST_CASE("Mata::Nfa::OnTheFlyAlphabet::add_symbols_from()") {
    OnTheFlyAlphabet alphabet{};
    StringToSymbolMap symbol_map{ { "a", 4 }, { "b", 2 }, { "c", 10 } };
    alphabet.add_symbols_from(symbol_map);

    auto symbols{alphabet.get_alphabet_symbols() };
    Mata::Util::OrdVector<Symbol> expected{ 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == symbol_map);

    symbol_map["a"] = 6;
	symbol_map["e"] = 7;
    alphabet.add_symbols_from(symbol_map);

    symbols = alphabet.get_alphabet_symbols();
	expected = Mata::Util::OrdVector<Symbol>{ 7, 4, 2, 10 };
    CHECK(symbols == expected);
    CHECK(alphabet.get_next_value() == 11);
    CHECK(alphabet.get_symbol_map() == StringToSymbolMap{
		{ "a", 4 }, { "b", 2 }, { "c", 10 }, { "e", 7 }
	});
}

TEST_CASE("Mata::Nfa::Nfa::delta.add()/delta.contains()")
{ // {{{
	Nfa a(3);

	SECTION("Empty automata have now transitions")
	{
		REQUIRE(!a.delta.contains(1, 'a', 1));
	}

	SECTION("If I add a transition, it is in the automaton")
	{
		a.delta.add(1, 'a', 1);

		REQUIRE(a.delta.contains(1, 'a', 1));
	}

	SECTION("If I add a transition, only it is added")
	{
		a.delta.add(1, 'a', 1);

		REQUIRE(a.delta.contains(1, 'a', 1));
		REQUIRE(!a.delta.contains(1, 'a', 2));
		REQUIRE(!a.delta.contains(1, 'b', 2));
		REQUIRE(!a.delta.contains(2, 'a', 1));
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
    aut.add_state(state_num-1);

	SECTION("a non-empty automaton")
	{
		aut.delta.add('q', 'a', 'r');
		aut.delta.add('q', 'b', 'r');
		auto it = aut.delta.begin();
		auto jt = aut.delta.begin();
		REQUIRE(it == jt);
		++it;
		REQUIRE(it != jt);
		REQUIRE((it != aut.delta.begin() && it != aut.delta.end()));
		REQUIRE(jt == aut.delta.begin());

		++jt;
		REQUIRE(it == jt);
		REQUIRE((jt != aut.delta.begin() && jt != aut.delta.end()));

        jt = aut.delta.end();
		REQUIRE(it != jt);
		REQUIRE((jt != aut.delta.begin() && jt == aut.delta.end()));

        it = aut.delta.end();
		REQUIRE(it == jt);
		REQUIRE((it != aut.delta.begin() && it == aut.delta.end()));
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
		b.initial = {1, 4, 6};
		b.final = {4, 7, 9, 0};
		b.delta.add(1, 'a', 1);
		b.delta.add(2, 'a', 8);
		b.delta.add(0, 'c', 49);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Right-hand side empty automaton is state disjoint with anything")
	{
		a.initial = {1, 4, 6};
		a.final = {4, 7, 9, 0};
		a.delta.add(1, 'a', 1);
		a.delta.add(2, 'a', 8);
		a.delta.add(0, 'c', 49);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Automata with intersecting initial states are not state disjoint")
	{
		a.initial = {1, 4, 6};
		b.initial = {3, 9, 6, 8};

		REQUIRE(!are_state_disjoint(a, b));
	}

	SECTION("Automata with intersecting final states are not state disjoint")
	{
		a.final = {1, 4, 6};
		b.final = {3, 9, 6, 8};

		REQUIRE(!are_state_disjoint(a, b));
	}

	SECTION("Automata with disjoint sets of states are state disjoint")
	{
		a.initial = {0, 5, 16};
		a.final = {1, 4, 6};

		b.initial = {11, 3};
		b.final = {3, 9, 8};

		a.delta.add(1, 'a', 7);
		a.delta.add(1, 'b', 7);
		b.delta.add(3, 'b', 11);
		b.delta.add(3, 'b', 9);

		REQUIRE(are_state_disjoint(a, b));
	}

	SECTION("Automata with intersecting states are not disjoint")
	{
		a.initial = {0, 5, 16};
		a.final = {1, 4};

		b.initial = {11, 3};
		b.final = {3, 9, 6, 8};

		a.delta.add(1, 'a', 7);
		a.delta.add(1, 'b', 7);
		a.delta.add(1, 'c', 7);
		b.delta.add(3, 'c', 11);
		b.delta.add(3, 'c', 5);
		b.delta.add(11, 'a', 3);

		REQUIRE(!are_state_disjoint(a, b));
	}
} // }}}

TEST_CASE("Mata::Nfa::union_norename()")
{ // {{{
	Nfa a(7), b(7), res;

	SECTION("Union of empty automata")
	{
		union_norename(&res, a, b);

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
	}

	SECTION("Union of automata with no transitions")
	{
		a.initial = {1, 3};
		a.final = {3, 5};

		b.initial = {4, 6};
		b.final = {4, 2};

		union_norename(&res, a, b);

		REQUIRE(!res.initial.empty());
		REQUIRE(!res.final.empty());

		REQUIRE(res.initial[1]);
		REQUIRE(res.initial[3]);
		REQUIRE(res.initial[4]);
		REQUIRE(res.initial[6]);
		REQUIRE(res.final[3]);
		REQUIRE(res.final[5]);
		REQUIRE(res.final[4]);
		REQUIRE(res.final[2]);
	}

	SECTION("Union of automata with some transitions")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);

		union_norename(&res, a, b);

		OnTheFlyAlphabet alph{"a", "b"};
		StringMap params;
		params["algorithm"] = "antichains";

		REQUIRE(is_included(a, res, &alph, params));
		REQUIRE(is_included(b, res, &alph, params));
	}

	SECTION("Union of automata with some transitions but without a final state")
	{
		FILL_WITH_AUT_A(a);
		FILL_WITH_AUT_B(b);
		b.final = {};

		union_norename(&res, a, b);

		OnTheFlyAlphabet alph{"a", "b"};
		StringMap params;
		params["algorithm"] = "antichains";

		REQUIRE(is_included(a, res, &alph, params));
		REQUIRE(is_included(res, a, &alph, params));

		WARN_PRINT("Insufficient testing of Mata::Nfa::union_norename()");
	}
} // }}}
*/

TEST_CASE("Mata::Nfa::is_lang_empty()")
{ // {{{
	Nfa aut(14);
	Run cex;

	SECTION("An empty automaton has an empty language")
	{
		REQUIRE(is_lang_empty(aut));
	}

	SECTION("An automaton with a state that is both initial and final does not have an empty language")
	{
		aut.initial = {1, 2};
		aut.final = {2, 3};

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);
	}

	SECTION("More complicated automaton")
	{
		aut.initial = {1, 2};
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'a', 3);
		aut.delta.add(1, 'b', 4);
		aut.delta.add(2, 'a', 2);
		aut.delta.add(2, 'a', 3);
		aut.delta.add(2, 'b', 4);
		aut.delta.add(3, 'b', 4);
		aut.delta.add(3, 'c', 7);
		aut.delta.add(3, 'b', 2);
		aut.delta.add(7, 'a', 8);

		SECTION("with final states")
		{
			aut.final = {7};
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
			aut.final = {13};

			REQUIRE(is_lang_empty(aut));
		}
	}

	SECTION("An automaton with a state that is both initial and final does not have an empty language")
	{
		aut.initial = {1, 2};
		aut.final = {2, 3};

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.path.size() == 1);
		REQUIRE(cex.path[0] == 2);
	}

	SECTION("Counterexample of an automaton with non-empty language")
	{
		aut.initial = {1, 2};
		aut.final = {8, 9};
		aut.delta.add(1, 'c', 2);
		aut.delta.add(2, 'a', 4);
		aut.delta.add(2, 'c', 1);
		aut.delta.add(2, 'c', 3);
		aut.delta.add(3, 'e', 5);
		aut.delta.add(4, 'c', 8);

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.path.size() == 3);
		REQUIRE(cex.path[0] == 2);
		REQUIRE(cex.path[1] == 4);
		REQUIRE(cex.path[2] == 8);
	}
} // }}}

TEST_CASE("Mata::Nfa::get_word_for_path()")
{ // {{{
	Nfa aut(5);
	Run path;
	Word word;

	SECTION("empty word")
	{
		path = { };

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.word.empty());
	}

	SECTION("empty word 2")
	{
		aut.initial = {1};
		path.path = {1};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.word.empty());
	}

	SECTION("nonempty word")
	{
		aut.initial = {1};
		aut.delta.add(1, 'c', 2);
		aut.delta.add(2, 'a', 4);
		aut.delta.add(2, 'c', 1);
		aut.delta.add(2, 'b', 3);

        path.path = {1,2,3};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.word == Word({'c', 'b'}));
	}

	SECTION("longer word")
	{
		aut.initial = {1};
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'c', 2);
		aut.delta.add(2, 'a', 4);
		aut.delta.add(2, 'c', 1);
		aut.delta.add(2, 'b', 3);
		aut.delta.add(3, 'd', 2);

        path.path = {1,2,3,2,4};

		auto word_bool_pair = get_word_for_path(aut, path);
		std::set<Word> possible({
			Word({'c', 'b', 'd', 'a'}),
			Word({'a', 'b', 'd', 'a'})});
		REQUIRE(word_bool_pair.second);
		REQUIRE(haskey(possible, word_bool_pair.first.word));
	}

	SECTION("invalid path")
	{
		aut.initial = {1};
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'c', 2);
		aut.delta.add(2, 'a', 4);
		aut.delta.add(2, 'c', 1);
		aut.delta.add(2, 'b', 3);
		aut.delta.add(3, 'd', 2);

		path.path = {1,2,3,1,2};

		auto word_bool_pair = get_word_for_path(aut, path);
		REQUIRE(!word_bool_pair.second);
	}
}


TEST_CASE("Mata::Nfa::is_lang_empty_cex()")
{
	Nfa aut(10);
	Run cex;

	SECTION("Counterexample of an automaton with non-empty language")
	{
		aut.initial = {1, 2};
		aut.final = {8, 9};
		aut.delta.add(1, 'c', 2);
		aut.delta.add(2, 'a', 4);
		aut.delta.add(2, 'c', 1);
		aut.delta.add(2, 'c', 3);
		aut.delta.add(3, 'e', 5);
		aut.delta.add(4, 'c', 8);

		bool is_empty = is_lang_empty(aut, &cex);
		REQUIRE(!is_empty);

		// check the counterexample
		REQUIRE(cex.word.size() == 2);
		REQUIRE(cex.word[0] == 'a');
		REQUIRE(cex.word[1] == 'c');
	}
}


TEST_CASE("Mata::Nfa::determinize()")
{
	Nfa aut(3);
	Nfa result;
	std::unordered_map<StateSet, State> subset_map;

	SECTION("empty automaton")
	{
		result = determinize(aut);

		REQUIRE(result.final.empty());
		REQUIRE(nothing_in_trans(result));
        CHECK(is_lang_empty(result));
	}

	SECTION("simple automaton 1")
	{
		aut.initial = {1 };
		aut.final = {1 };
		result = determinize(aut, &subset_map);

		REQUIRE(result.initial[subset_map[{1}]]);
		REQUIRE(result.final[subset_map[{1}]]);
		REQUIRE(nothing_in_trans(result));
	}

	SECTION("simple automaton 2")
	{
		aut.initial = {1 };
		aut.final = {2 };
		aut.delta.add(1, 'a', 2);
		result = determinize(aut, &subset_map);

		REQUIRE(result.initial[subset_map[{1}]]);
        REQUIRE(result.final[subset_map[{2}]]);
		REQUIRE(result.delta.contains(subset_map[{1}], 'a', subset_map[{2}]));
	}

    SECTION("This broke Delta when delta[q] could cause re-allocation of post")
    {
        Nfa x{};
        x.initial.add(0);
        x.final.add(4);
        x.delta.add(0, 1, 3);
        x.delta.add(3, 1, 3);
        x.delta.add(3, 2, 3);
        x.delta.add(3, 0, 1);
        x.delta.add(1, 1, 1);
        x.delta.add(1, 2, 1);
        x.delta.add(1, 0, 2);
        x.delta.add(2, 0, 2);
        x.delta.add(2, 1, 2);
        x.delta.add(2, 2, 2);
        x.delta.add(2, 0, 4);
        OnTheFlyAlphabet alphabet{};
        auto complement_result{determinize(x)};
    }
} // }}}

TEST_CASE("Mata::Nfa::minimize() for profiling", "[.profiling],[minimize]") {
    Nfa aut(4);
    Nfa result;
    std::unordered_map<StateSet, State> subset_map;

    aut.initial.add(0);
    aut.final.add(3);
    aut.delta.add(0, 46, 0);
    aut.delta.add(0, 47, 0);
    aut.delta.add(0, 58, 0);
    aut.delta.add(0, 58, 1);
    aut.delta.add(0, 64, 0);
    aut.delta.add(0, 64, 0);
    aut.delta.add(0, 82, 0);
    aut.delta.add(0, 92, 0);
    aut.delta.add(0, 98, 0);
    aut.delta.add(0, 100, 0);
    aut.delta.add(0, 103, 0);
    aut.delta.add(0, 109, 0);
    aut.delta.add(0, 110, 0);
    aut.delta.add(0, 111, 0);
    aut.delta.add(0, 114, 0);
    aut.delta.add(1, 47, 2);
    aut.delta.add(2, 47, 3);
    aut.delta.add(3, 46, 3);
    aut.delta.add(3, 47, 3);
    aut.delta.add(3, 58, 3);
    aut.delta.add(3, 64, 3);
    aut.delta.add(3, 82, 3);
    aut.delta.add(3, 92, 3);
    aut.delta.add(3, 98, 3);
    aut.delta.add(3, 100, 3);
    aut.delta.add(3, 103, 3);
    aut.delta.add(3, 109, 3);
    aut.delta.add(3, 110, 3);
    aut.delta.add(3, 111, 3);
    aut.delta.add(3, 114, 3);
    minimize(&result, aut);
}

TEST_CASE("Mata::Nfa::construct() correct calls")
{ // {{{
	Nfa aut(10);
	Mata::Parser::ParsedSection parsec;
	StringToSymbolMap symbol_map;

	SECTION("construct an empty automaton")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;

		aut = construct(parsec);

		REQUIRE(is_lang_empty(aut));
	}

	SECTION("construct a simple non-empty automaton accepting the empty word")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q1"}});

		aut = construct(parsec);

		REQUIRE(!is_lang_empty(aut));
	}

	SECTION("construct an automaton with more than one initial/final states")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1", "q2"}});
		parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

		aut = construct(parsec);

		REQUIRE(aut.initial.size() == 2);
		REQUIRE(aut.final.size() == 3);
	}

	SECTION("construct a simple non-empty automaton accepting only the word 'a'")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q2"}});
		parsec.body = { {"q1", "a", "q2"} };

		aut = construct(parsec, &symbol_map);

		Run cex;
		REQUIRE(!is_lang_empty(aut, &cex));
		auto word_bool_pair = get_word_for_path(aut, cex);
		REQUIRE(word_bool_pair.second);
		REQUIRE(word_bool_pair.first.word == encode_word(symbol_map, {"a"}).word);

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

		aut = construct(parsec, &symbol_map);

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

		CHECK_THROWS_WITH(construct(parsec),
			Catch::Contains("expecting type"));
	}

	SECTION("construct() call with an epsilon transition")
	{
		parsec.type = Mata::Nfa::TYPE_NFA;
		parsec.body = { {"q1", "q2"} };

		CHECK_THROWS_WITH(construct(parsec),
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

TEST_CASE("Mata::Nfa::construct() from IntermediateAut correct calls")
{ // {{{
    Nfa aut;
    Mata::IntermediateAut inter_aut;
    StringToSymbolMap symbol_map;

    SECTION("construct an empty automaton")
    {
        inter_aut.automaton_type = Mata::IntermediateAut::NFA;
        REQUIRE(is_lang_empty(aut));
        aut = construct(inter_aut);
        REQUIRE(is_lang_empty(aut));
    }

    SECTION("construct a simple non-empty automaton accepting the empty word from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p | q\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        aut = construct(inter_aut);

        REQUIRE(!is_lang_empty(aut));
    }

    SECTION("construct an automaton with more than one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p | q | r\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p q\n"
                "%Final p q r\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 2);
        REQUIRE(aut.final.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q r m n\n"
                "%Alphabet-auto\n"
                "%Initial p q r\n"
                "%Final p q m n\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initial.size() == 3);
        REQUIRE(aut.final.size() == 4);
    }

    SECTION("construct a simple non-empty automaton accepting only the word 'a' from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1\n"
                "%Final q2\n"
                "q1 a q2\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];
        construct(&aut, inter_aut, &symbol_map);

        Run cex;
        REQUIRE(!is_lang_empty(aut, &cex));
        auto word_bool_pair = get_word_for_path(aut, cex);
        REQUIRE(word_bool_pair.second);
        REQUIRE(word_bool_pair.first.word == encode_word(symbol_map, {"a"}).word);

        REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a"})));
    }

    SECTION("construct a more complicated non-empty automaton from intermediate automaton")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1 | q3\n"
                "%Final q5\n"
                "q1 a q3\n"
                "q1 a q10\n"
                "q1 b q7\n"
                "q3 a q7\n"
                "q3 b q9\n"
                "q9 a q9\n"
                "q7 b q1\n"
                "q7 a q3\n"
                "q7 c q3\n"
                "q10 a q7\n"
                "q10 b q7\n"
                "q10 c q7\n"
                "q7 a q5\n"
                "q5 c q9\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut, &symbol_map);

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

    SECTION("construct - final states from negation")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final !q0 & !q1 & !q4 & !q5 & !q6\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut, &symbol_map);
        REQUIRE(aut.final.size() == 4);
        REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a1", "a2"})));
        REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a1", "a2", "a3"})));
        REQUIRE(!is_in_lang(aut, encode_word(symbol_map, {"a1", "a2", "a3", "a4"})));
        REQUIRE(is_in_lang(aut, encode_word(symbol_map, {"a1", "a2", "a3", "a5", "a7"})));
    }

    SECTION("construct - final states given as true")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final true\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

		Mata::Nfa::StringToStateMap state_map;
        construct(&aut, inter_aut, &symbol_map, &state_map);
        CHECK(aut.final.size() == 9);
		CHECK(aut.final[state_map.at("0")]);
		CHECK(aut.final[state_map.at("1")]);
		CHECK(aut.final[state_map.at("2")]);
		CHECK(aut.final[state_map.at("3")]);
		CHECK(aut.final[state_map.at("4")]);
		CHECK(aut.final[state_map.at("5")]);
		CHECK(aut.final[state_map.at("6")]);
		CHECK(aut.final[state_map.at("7")]);
		CHECK(aut.final[state_map.at("8")]);
    }

    SECTION("construct - final states given as false")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q0 q8\n"
                "%Final false\n"
                "q0 a1 q1\n"
                "q1 a2 q2\n"
                "q2 a3 q3\n"
                "q2 a4 q4\n"
                "q3 a5 q5\n"
                "q3 a6 q6\n"
                "q5 a7 q7\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

		Mata::Nfa::StringToStateMap state_map;
        construct(&aut, inter_aut, &symbol_map, &state_map);
        CHECK(aut.final.size() == 0);
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

		REQUIRE(res.initial.empty());
		REQUIRE(res.final.empty());
		REQUIRE(res.delta.empty());
	}

	SECTION("small automaton")
	{
		aut.initial = { 'q', 'r', 's' };
		aut.final = { 'r', 's', 't' };

		aut.delta.add('q', 'a', 'r');
		aut.delta.add('r', 'b', 'q');
		aut.delta.add('s', 'c', 'q');
		aut.delta.add('s', 'd', 'q');
		aut.delta.add('q', 'a', 'q');

		Mata::Nfa::StateToStringMap state_dict =
			{{'q', "q"}, {'r', "r"}, {'s', "s"}, {'t', "t"}};
		Mata::Nfa::SymbolToStringMap symb_dict =
			{{'a', "a"}, {'b', "b"}, {'c', "c"}, {'d', "d"}};
		std::string str = std::to_string(serialize(aut, &symb_dict, &state_dict));

		ParsedSection parsec = Mata::Parser::parse_mf_section(str);

		Mata::Nfa::StringToStateMap inv_state_dict =
			Mata::util::invert_map(state_dict);
		Mata::Nfa::StringToSymbolMap inv_symb_dict =
			Mata::util::invert_map(symb_dict);
		Nfa res = construct(parsec, &inv_symb_dict, &inv_state_dict);

		REQUIRE(res.initial == aut.initial);
		REQUIRE(res.final == aut.final);
		REQUIRE(res.trans_size() == aut.trans_size());
		REQUIRE(res.delta.has_trans('q', 'a', 'r'));
		REQUIRE(res.delta.has_trans('r', 'b', 'q'));
		REQUIRE(res.delta.has_trans('s', 'c', 'q'));
		REQUIRE(res.delta.has_trans('s', 'd', 'q'));
		REQUIRE(res.delta.contains('q', 'a', 'q'));
	}

	SECTION("implicit state and symbol mapper")
	{
		aut.delta.add(1, 2, 3);

		ParsedSection parsec = serialize(aut);

		REQUIRE(parsec.body.size() == 1);
		REQUIRE(*parsec.body.cbegin() == BodyLine({"q1", "a2", "q3"}));
	}

	SECTION("incorrect state mapper")
	{
		Mata::Nfa::StateToStringMap state_dict = {{'q', "q"}};
		Mata::Nfa::SymbolToStringMap symb_dict = {{'a', "a"}};
		aut.delta.add('q', 'a', 'r');

		CHECK_THROWS_WITH(serialize(aut, &symb_dict, &state_dict),
			Catch::Contains("cannot translate state"));
	}

	SECTION("incorrect symbol mapper")
	{
		Mata::Nfa::StateToStringMap state_dict = {{'q', "q"}, {'r', "r"}};
		Mata::Nfa::SymbolToStringMap symb_dict = {{'a', "a"}};
		aut.delta.add('q', 'b', 'r');

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
		OnTheFlyAlphabet alph{};

		make_complete(aut, alph, 0);

		REQUIRE(aut.initial.empty());
		REQUIRE(aut.final.empty());
		REQUIRE(nothing_in_trans(aut));
	}

	SECTION("empty automaton")
	{
		OnTheFlyAlphabet alph{"a", "b"};

		make_complete(aut, alph, 0);

		REQUIRE(aut.initial.empty());
		REQUIRE(aut.final.empty());
		REQUIRE(aut.delta.contains(0, alph["a"], 0));
		REQUIRE(aut.delta.contains(0, alph["b"], 0));
	}

	SECTION("non-empty automaton, empty alphabet")
	{
		OnTheFlyAlphabet alphabet{};

		aut.initial = {1};

		make_complete(aut, alphabet, 0);

		REQUIRE(aut.initial.size() == 1);
		REQUIRE(*aut.initial.begin() == 1);
		REQUIRE(aut.final.empty());
		REQUIRE(nothing_in_trans(aut));
	}

	SECTION("one-state automaton")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		const State SINK = 10;

		aut.initial = {1};

		make_complete(aut, alph, SINK);

		REQUIRE(aut.initial.size() == 1);
		REQUIRE(*aut.initial.begin() == 1);
		REQUIRE(aut.final.empty());
		REQUIRE(aut.delta.contains(1, alph["a"], SINK));
		REQUIRE(aut.delta.contains(1, alph["b"], SINK));
		REQUIRE(aut.delta.contains(SINK, alph["a"], SINK));
		REQUIRE(aut.delta.contains(SINK, alph["b"], SINK));
	}

	SECTION("bigger automaton")
	{
		OnTheFlyAlphabet alph{"a", "b", "c"};
		const State SINK = 9;

		aut.initial = {1, 2};
		aut.final = {8};
		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(2, alph["a"], 4);
		aut.delta.add(2, alph["c"], 1);
		aut.delta.add(2, alph["c"], 3);
		aut.delta.add(3, alph["b"], 5);
		aut.delta.add(4, alph["c"], 8);

		make_complete(aut, alph, SINK);

		REQUIRE(aut.delta.contains(1, alph["a"], 2));
		REQUIRE(aut.delta.contains(1, alph["b"], SINK));
		REQUIRE(aut.delta.contains(1, alph["c"], SINK));
		REQUIRE(aut.delta.contains(2, alph["a"], 4));
		REQUIRE(aut.delta.contains(2, alph["c"], 1));
		REQUIRE(aut.delta.contains(2, alph["c"], 3));
		REQUIRE(aut.delta.contains(2, alph["b"], SINK));
		REQUIRE(aut.delta.contains(3, alph["b"], 5));
		REQUIRE(aut.delta.contains(3, alph["a"], SINK));
		REQUIRE(aut.delta.contains(3, alph["c"], SINK));
		REQUIRE(aut.delta.contains(4, alph["c"], 8));
		REQUIRE(aut.delta.contains(4, alph["a"], SINK));
		REQUIRE(aut.delta.contains(4, alph["b"], SINK));
		REQUIRE(aut.delta.contains(5, alph["a"], SINK));
		REQUIRE(aut.delta.contains(5, alph["b"], SINK));
		REQUIRE(aut.delta.contains(5, alph["c"], SINK));
		REQUIRE(aut.delta.contains(8, alph["a"], SINK));
		REQUIRE(aut.delta.contains(8, alph["b"], SINK));
		REQUIRE(aut.delta.contains(8, alph["c"], SINK));
		REQUIRE(aut.delta.contains(SINK, alph["a"], SINK));
		REQUIRE(aut.delta.contains(SINK, alph["b"], SINK));
		REQUIRE(aut.delta.contains(SINK, alph["c"], SINK));
	}
} // }}}

TEST_CASE("Mata::Nfa::complement()")
{ // {{{
	Nfa aut(3);
	Nfa cmpl;

	SECTION("empty automaton, empty alphabet")
	{
		OnTheFlyAlphabet alph{};

		cmpl = complement(aut, alph);

		REQUIRE(is_in_lang(cmpl, { }));
		REQUIRE(cmpl.initial.size() == 1);
		REQUIRE(cmpl.final.size() == 1);
		REQUIRE(nothing_in_trans(cmpl));
		REQUIRE(*cmpl.initial.begin() == *cmpl.final.begin());
	}

	SECTION("empty automaton")
	{
		OnTheFlyAlphabet alph{"a", "b"};

		cmpl = complement(aut, alph);

		REQUIRE(is_in_lang(cmpl, {}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"] },{}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["b"] }, {}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"], alph["a"]}, {}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));

		// TODO: consider removing the structural tests (in case a more}
		// sophisticated complementation algorithm is used)
		REQUIRE(cmpl.initial.size() == 1);
		REQUIRE(cmpl.final.size() == 1);

		State init_state = *cmpl.initial.begin();
		State fin_state = *cmpl.final.begin();
		REQUIRE(init_state == fin_state);
		REQUIRE(cmpl.get_moves_from(init_state).size() == 2);
		REQUIRE(cmpl.delta.contains(init_state, alph["a"], init_state));
		REQUIRE(cmpl.delta.contains(init_state, alph["b"], init_state));
	}

	SECTION("empty automaton accepting epsilon, empty alphabet")
	{
		OnTheFlyAlphabet alph{};
		aut.initial = {1};
		aut.final = {1};

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(cmpl.initial.size() == 1);
		REQUIRE(cmpl.final.size() == 0);
		REQUIRE(nothing_in_trans(cmpl));
	}

	SECTION("empty automaton accepting epsilon")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1};
		aut.final = {1};

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"]}, {}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["b"]}, {}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"], alph["a"]}, {}}));
		REQUIRE(is_in_lang(cmpl, Mata::Nfa::Run{{ alph["a"], alph["b"], alph["b"], alph["a"]},{}}));
		REQUIRE(cmpl.initial.size() == 1);
		REQUIRE(cmpl.final.size() == 1);
		size_t sum = 0;
		for (const auto& x : cmpl) {
            unused(x);
            sum++;
		}
		REQUIRE(sum == 4);
	}

	SECTION("non-empty automaton accepting a*b*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1, 2};
		aut.final = {1, 2};

		aut.delta.add(1, alph["a"], 1);
		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(2, alph["b"], 2);

		cmpl = complement(aut, alph);

		REQUIRE(!is_in_lang(cmpl, { }));
		REQUIRE(!is_in_lang(cmpl, {{ alph["a"] }, {}}));
		REQUIRE(!is_in_lang(cmpl, {{ alph["b"] }, {}}));
		REQUIRE(!is_in_lang(cmpl, {{ alph["a"], alph["a"] }, {}}));
		REQUIRE(is_in_lang(cmpl, {{ alph["a"], alph["b"], alph["b"], alph["a"] }, {}}));
		REQUIRE(!is_in_lang(cmpl, {{ alph["a"], alph["a"], alph["b"], alph["b"] }, {}}));
		REQUIRE(is_in_lang(cmpl, {{ alph["b"], alph["a"], alph["a"], alph["a"] }, {}}));

		REQUIRE(cmpl.initial.size() == 1);
		REQUIRE(cmpl.final.size() == 1);
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
	Run cex;
	StringMap params;

	const std::unordered_set<std::string> ALGORITHMS = {
		"naive",
		"antichains",
	};

	SECTION("empty automaton, empty alphabet")
	{
		OnTheFlyAlphabet alph{};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("empty automaton accepting epsilon, empty alphabet")
	{
		OnTheFlyAlphabet alph{};
		aut.initial = {1};
		aut.final = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
			REQUIRE(Word{ } == cex.word);
		}
	}

	SECTION("empty automaton accepting epsilon")
	{
		OnTheFlyAlphabet alph{"a"};
		aut.initial = {1};
		aut.final = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(!is_univ);
			REQUIRE(((cex.word == Word{alph["a"]}) || (cex.word == Word{alph["b"]})));
		}
	}

	SECTION("automaton for a*b*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1, 2};
		aut.final = {1, 2};

		aut.delta.add(1, alph["a"], 1);
		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(2, alph["b"], 2);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("automaton for a* + b*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1, 2};
		aut.final = {1, 2};

		aut.delta.add(1, alph["a"], 1);
		aut.delta.add(2, alph["b"], 2);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(!is_univ);
		}
	}

	SECTION("automaton for (a + b)*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1};
		aut.final = {1};

		aut.delta.add(1, alph["a"], 1);
		aut.delta.add(1, alph["b"], 1);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("automaton for eps + (a+b) + (a+b)(a+b)(a* + b*)")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1};
		aut.final = {1, 2, 3, 4, 5};

		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(1, alph["b"], 2);
		aut.delta.add(2, alph["a"], 3);
		aut.delta.add(2, alph["b"], 3);

		aut.delta.add(3, alph["a"], 4);
		aut.delta.add(4, alph["a"], 4);

		aut.delta.add(3, alph["b"], 5);
		aut.delta.add(5, alph["b"], 5);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(!is_univ);

			REQUIRE(cex.word.size() == 4);
			REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
			REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
			REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
			REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
			REQUIRE(cex.word[2] != cex.word[3]);
		}
	}

	SECTION("automaton for epsilon + a(a + b)* + b(a + b)*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1, 3};
		aut.final = {1, 2, 4};

		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(2, alph["a"], 2);
		aut.delta.add(2, alph["b"], 2);
		aut.delta.add(3, alph["b"], 4);
		aut.delta.add(4, alph["a"], 4);
		aut.delta.add(4, alph["b"], 4);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("example from Abdulla et al. TACAS'10")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		aut.initial = {1, 2};
		aut.final = {1, 2, 3};

		aut.delta.add(1, alph["b"], 1);
		aut.delta.add(1, alph["a"], 2);
		aut.delta.add(1, alph["b"], 4);
		aut.delta.add(2, alph["b"], 2);
		aut.delta.add(2, alph["a"], 3);
		aut.delta.add(3, alph["b"], 3);
		aut.delta.add(3, alph["a"], 1);
		aut.delta.add(4, alph["b"], 2);
		aut.delta.add(4, alph["b"], 3);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("subsumption-pruning in processed")
	{
		OnTheFlyAlphabet alph{"a"};
		aut.initial = {1, 2};
		aut.final = {1};

		aut.delta.add(1, alph["a"], 1);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_univ = is_universal(aut, alph, &cex, params);

			REQUIRE(is_univ);
		}
	}

	SECTION("wrong parameters 1")
	{
		OnTheFlyAlphabet alph{};

		CHECK_THROWS_WITH(is_universal(aut, alph, params),
			Catch::Contains("requires setting the \"algo\" key"));
	}

	SECTION("wrong parameters 2")
	{
		OnTheFlyAlphabet alph{};
		params["algorithm"] = "foo";

		CHECK_THROWS_WITH(is_universal(aut, alph, params),
			Catch::Contains("received an unknown value"));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_included()")
{ // {{{
	Nfa smaller(10);
	Nfa bigger(16);
	Run cex;
	StringMap params;

	const std::unordered_set<std::string> ALGORITHMS = {
		"naive",
		"antichains",
	};

	SECTION("{} <= {}, empty alphabet")
	{
		OnTheFlyAlphabet alph{};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &alph, params);
			CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &alph, params);
            CHECK(is_incl);
		}
	}

	SECTION("{} <= {epsilon}, empty alphabet")
	{
		OnTheFlyAlphabet alph{};
		bigger.initial = {1};
		bigger.final = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
            CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            CHECK(!is_incl);
		}
	}

	SECTION("{epsilon} <= {epsilon}, empty alphabet")
	{
		OnTheFlyAlphabet alph{};
		smaller.initial = {1};
		smaller.final = {1};
		bigger.initial = {11};
		bigger.final = {11};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
            CHECK(is_incl);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            CHECK(is_incl);
		}
	}

	SECTION("{epsilon} !<= {}, empty alphabet")
	{
		OnTheFlyAlphabet alph{};
		smaller.initial = {1};
		smaller.final = {1};

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &cex, &alph, params);

			REQUIRE(!is_incl);
			REQUIRE(cex.word == Word{});

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(cex.word == Word{});
            REQUIRE(is_incl);
		}
	}

	SECTION("a* + b* <= (a+b)*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		smaller.initial = {1, 2};
		smaller.final = {1, 2};
		smaller.delta.add(1, alph["a"], 1);
		smaller.delta.add(2, alph["b"], 2);

		bigger.initial = {11};
		bigger.final = {11};
		bigger.delta.add(11, alph["a"], 11);
		bigger.delta.add(11, alph["b"], 11);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &alph, params);
			REQUIRE(is_incl);

            is_incl = is_included(bigger, smaller, &alph, params);
            REQUIRE(!is_incl);
		}
	}

	SECTION("(a+b)* !<= a* + b*")
	{
		OnTheFlyAlphabet alph{"a", "b"};
		smaller.initial = {1};
		smaller.final = {1};
		smaller.delta.add(1, alph["a"], 1);
		smaller.delta.add(1, alph["b"], 1);

		bigger.initial = {11, 12};
		bigger.final = {11, 12};
		bigger.delta.add(11, alph["a"], 11);
		bigger.delta.add(12, alph["b"], 12);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;

			bool is_incl = is_included(smaller, bigger, &cex, &alph, params);

			REQUIRE(!is_incl);
			REQUIRE((
				cex.word == Word{alph["a"], alph["b"]} ||
				cex.word == Word{alph["b"], alph["a"]}));

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(is_incl);
            REQUIRE((
                cex.word == Word{alph["a"], alph["b"]} ||
                cex.word == Word{alph["b"], alph["a"]}));
		}
	}

	SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
	{
        OnTheFlyAlphabet alph{"a", "b"};
		smaller.initial = {1};
		smaller.final = {1};
		smaller.delta.add(1, alph["a"], 1);
		smaller.delta.add(1, alph["b"], 1);

		bigger.initial = {11};
		bigger.final = {11, 12, 13, 14, 15};

		bigger.delta.add(11, alph["a"], 12);
		bigger.delta.add(11, alph["b"], 12);
		bigger.delta.add(12, alph["a"], 13);
		bigger.delta.add(12, alph["b"], 13);

		bigger.delta.add(13, alph["a"], 14);
		bigger.delta.add(14, alph["a"], 14);

		bigger.delta.add(13, alph["b"], 15);
		bigger.delta.add(15, alph["b"], 15);

		for (const auto& algo : ALGORITHMS) {
			params["algorithm"] = algo;
			bool is_incl = is_included(smaller, bigger, &cex, &alph, params);
			REQUIRE(!is_incl);

			REQUIRE(cex.word.size() == 4);
			REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
			REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
			REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
			REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
			REQUIRE(cex.word[2] != cex.word[3]);

            is_incl = is_included(bigger, smaller, &cex, &alph, params);
            REQUIRE(is_incl);

            REQUIRE(cex.word.size() == 4);
            REQUIRE((cex.word[0] == alph["a"] || cex.word[0] == alph["b"]));
            REQUIRE((cex.word[1] == alph["a"] || cex.word[1] == alph["b"]));
            REQUIRE((cex.word[2] == alph["a"] || cex.word[2] == alph["b"]));
            REQUIRE((cex.word[3] == alph["a"] || cex.word[3] == alph["b"]));
            REQUIRE(cex.word[2] != cex.word[3]);
		}
	}

	SECTION("wrong parameters 1")
	{
        OnTheFlyAlphabet alph{};

		CHECK_THROWS_WITH(is_included(smaller, bigger, &alph, params),
			Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(is_included(smaller, bigger, &alph));
	}

	SECTION("wrong parameters 2")
	{
        OnTheFlyAlphabet alph{};
		params["algorithm"] = "foo";

		CHECK_THROWS_WITH(is_included(smaller, bigger, &alph, params),
			Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(is_included(smaller, bigger, &alph));
	}
} // }}}

TEST_CASE("Mata::Nfa::are_equivalent")
{
    Nfa smaller(10);
    Nfa bigger(16);
    Word cex;
    StringMap params;

    const std::unordered_set<std::string> ALGORITHMS = {
            "naive",
            "antichains",
    };

    SECTION("{} == {}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(are_equivalent(smaller, bigger, &alph, params));
            CHECK(are_equivalent(smaller, bigger, params));
            CHECK(are_equivalent(smaller, bigger));

            CHECK(are_equivalent(bigger, smaller, &alph, params));
            CHECK(are_equivalent(bigger, smaller, params));
            CHECK(are_equivalent(bigger, smaller));
        }
    }

    SECTION("{} == {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        bigger.initial = {1};
        bigger.final = {1};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("{epsilon} == {epsilon}, empty alphabet")
    {
        OnTheFlyAlphabet alph{};
        smaller.initial = {1};
        smaller.final = {1};
        bigger.initial = {11};
        bigger.final = {11};

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(are_equivalent(smaller, bigger, &alph, params));
            CHECK(are_equivalent(smaller, bigger, params));
            CHECK(are_equivalent(smaller, bigger));

            CHECK(are_equivalent(bigger, smaller, &alph, params));
            CHECK(are_equivalent(bigger, smaller, params));
            CHECK(are_equivalent(bigger, smaller));
        }
    }

    SECTION("a* + b* == (a+b)*")
    {
        OnTheFlyAlphabet alph{"a", "b"};
        smaller.initial = {1, 2};
        smaller.final = {1, 2};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(2, alph["b"], 2);

        bigger.initial = {11};
        bigger.final = {11};
        bigger.delta.add(11, alph["a"], 11);
        bigger.delta.add(11, alph["b"], 11);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            //TODO:what about we test the plumbing versions primarily?
            // Debugging with the dispatcher is annoying.

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("a* != (a|b)*, was throwing exception")
    {
        Mata::Nfa::Nfa aut;
        Mata::RE2Parser::create_nfa(&aut, "a*");
        Mata::Nfa::Nfa aut2;
        Mata::RE2Parser::create_nfa(&aut2, "(a|b)*");
        CHECK(!Mata::Nfa::are_equivalent(aut, aut2));
    }

    SECTION("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
    {
        OnTheFlyAlphabet alph{"a", "b"};
        smaller.initial = {1};
        smaller.final = {1};
        smaller.delta.add(1, alph["a"], 1);
        smaller.delta.add(1, alph["b"], 1);

        bigger.initial = {11};
        bigger.final = {11, 12, 13, 14, 15};

        bigger.delta.add(11, alph["a"], 12);
        bigger.delta.add(11, alph["b"], 12);
        bigger.delta.add(12, alph["a"], 13);
        bigger.delta.add(12, alph["b"], 13);

        bigger.delta.add(13, alph["a"], 14);
        bigger.delta.add(14, alph["a"], 14);

        bigger.delta.add(13, alph["b"], 15);
        bigger.delta.add(15, alph["b"], 15);

        for (const auto& algo : ALGORITHMS) {
            params["algorithm"] = algo;

            CHECK(!are_equivalent(smaller, bigger, &alph, params));
            CHECK(!are_equivalent(smaller, bigger, params));
            CHECK(!are_equivalent(smaller, bigger));

            CHECK(!are_equivalent(bigger, smaller, &alph, params));
            CHECK(!are_equivalent(bigger, smaller, params));
            CHECK(!are_equivalent(bigger, smaller));
        }
    }

    SECTION("wrong parameters 1")
    {
        OnTheFlyAlphabet alph{};

        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, &alph, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, params),
                          Catch::Contains("requires setting the \"algo\" key"));
        CHECK_NOTHROW(are_equivalent(smaller, bigger));
    }

    SECTION("wrong parameters 2")
    {
        OnTheFlyAlphabet alph{};
        params["algorithm"] = "foo";

        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, &alph, params),
                          Catch::Contains("received an unknown value"));
        CHECK_THROWS_WITH(are_equivalent(smaller, bigger, params),
                          Catch::Contains("received an unknown value"));
        CHECK_NOTHROW(are_equivalent(smaller, bigger));
    }
}

TEST_CASE("Mata::Nfa::revert()")
{ // {{{
	Nfa aut(9);

	SECTION("empty automaton")
	{
		Nfa result = revert(aut);

		REQUIRE(nothing_in_trans(result));
		REQUIRE(result.initial.size() == 0);
		REQUIRE(result.final.size() == 0);
	}

	SECTION("no-transition automaton")
	{
		aut.initial.add(1);
		aut.initial.add(3);

		aut.final.add(2);
		aut.final.add(5);

		Nfa result = revert(aut);

		REQUIRE(nothing_in_trans(result));
		REQUIRE(result.initial[2]);
		REQUIRE(result.initial[5]);
		REQUIRE(result.final[1]);
		REQUIRE(result.final[3]);
	}

	SECTION("one-transition automaton")
	{
		aut.initial.add(1);
		aut.final.add(2);
		aut.delta.add(1, 'a', 2);

		Nfa result = revert(aut);

		REQUIRE(result.initial[2]);
		REQUIRE(result.final[1]);
		REQUIRE(result.delta.contains(2, 'a', 1));
		REQUIRE(result.delta.size() == aut.delta.size());
	}

	SECTION("bigger automaton")
	{
		aut.initial = {1, 2};
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'a', 3);
		aut.delta.add(1, 'b', 4);
		aut.delta.add(2, 'a', 2);
		aut.delta.add(2, 'a', 3);
		aut.delta.add(2, 'b', 4);
		aut.delta.add(3, 'b', 4);
		aut.delta.add(3, 'c', 7);
		aut.delta.add(3, 'b', 2);
		aut.delta.add(7, 'a', 8);
		aut.final = {3};

		Nfa result = revert(aut);
		//REQUIRE(result.final == StateSet({1, 2}));
        REQUIRE(StateSet(result.final.get_elements()) == StateSet({1, 2}));
		REQUIRE(result.delta.contains(2, 'a', 1));
		REQUIRE(result.delta.contains(3, 'a', 1));
		REQUIRE(result.delta.contains(4, 'b', 1));
		REQUIRE(result.delta.contains(2, 'a', 2));
		REQUIRE(result.delta.contains(3, 'a', 2));
		REQUIRE(result.delta.contains(4, 'b', 2));
		REQUIRE(result.delta.contains(4, 'b', 3));
		REQUIRE(result.delta.contains(7, 'c', 3));
		REQUIRE(result.delta.contains(2, 'b', 3));
		REQUIRE(result.delta.contains(8, 'a', 7));
		REQUIRE(StateSet(result.initial.get_elements()) == StateSet({3}));
	}

	SECTION("Automaton A") {
		Nfa nfa{ 11 };
		FILL_WITH_AUT_A(nfa);
		Nfa res = revert(nfa);
		CHECK(res.initial[5]);
		CHECK(res.final[1]);
		CHECK(res.final[3]);
		CHECK(res.get_num_of_trans() == 15);
		CHECK(res.delta.contains(5, 'a', 5));
		CHECK(res.delta.contains(5, 'a', 7));
		CHECK(res.delta.contains(9, 'a', 9));
		CHECK(res.delta.contains(9, 'c', 5));
		CHECK(res.delta.contains(9, 'b', 3));
		CHECK(res.delta.contains(7, 'a', 3));
		CHECK(res.delta.contains(7, 'a', 10));
		CHECK(res.delta.contains(7, 'b', 10));
		CHECK(res.delta.contains(7, 'c', 10));
		CHECK(res.delta.contains(7, 'b', 1));
		CHECK(res.delta.contains(3, 'a', 7));
		CHECK(res.delta.contains(3, 'c', 7));
		CHECK(res.delta.contains(3, 'a', 1));
		CHECK(res.delta.contains(1, 'b', 7));
		CHECK(res.delta.contains(10, 'a', 1));
	}

	SECTION("Automaton B") {
		Nfa nfa{ 15 };
		FILL_WITH_AUT_B(nfa);
		Nfa res = revert(nfa);
		CHECK(res.initial[2]);
		CHECK(res.initial[12]);
		CHECK(res.final[4]);
		CHECK(res.get_num_of_trans() == 12);
		CHECK(res.delta.contains(8, 'a', 4));
		CHECK(res.delta.contains(8, 'c', 4));
		CHECK(res.delta.contains(4, 'b', 8));
		CHECK(res.delta.contains(6, 'b', 4));
		CHECK(res.delta.contains(6, 'a', 4));
		CHECK(res.delta.contains(2, 'a', 6));
		CHECK(res.delta.contains(2, 'a', 0));
		CHECK(res.delta.contains(2, 'b', 2));
		CHECK(res.delta.contains(0, 'a', 2));
		CHECK(res.delta.contains(12, 'c', 2));
		CHECK(res.delta.contains(12, 'b', 14));
		CHECK(res.delta.contains(14, 'a', 12));
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
		aut.initial.add('q');
		REQUIRE(is_deterministic(aut));

		// add the same initial state
		aut.initial.add('q');
		REQUIRE(is_deterministic(aut));

		// add another initial state
		aut.initial.add('r');
		REQUIRE(!is_deterministic(aut));

		// add a final state
		aut.final.add('q');
		REQUIRE(!is_deterministic(aut));
	}

	SECTION("trivial automata")
	{
		aut.initial.add('q');
		aut.delta.add('q', 'a', 'r');
		REQUIRE(is_deterministic(aut));

		// unreachable states
		aut.delta.add('s', 'a', 'r');
		REQUIRE(is_deterministic(aut));

		// transitions over a different symbol
		aut.delta.add('q', 'b', 'h');
		REQUIRE(is_deterministic(aut));

		// nondeterminism
		aut.delta.add('q', 'a', 's');
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
		OnTheFlyAlphabet alph{};

		// is complete for the empty alphabet
		REQUIRE(is_complete(aut, alph));

		alph.translate_symb("a1");
		alph.translate_symb("a2");

		// the empty automaton is complete even for a non-empty alphabet
		REQUIRE(is_complete(aut, alph));

		// add a non-reachable state (the automaton should still be complete)
		aut.delta.add('q', alph["a1"], 'q');
		REQUIRE(is_complete(aut, alph));
	}

	SECTION("small automaton")
	{
		OnTheFlyAlphabet alph{};

        aut.initial.add(4);
		aut.delta.add(4, alph["a"], 8);
		aut.delta.add(4, alph["c"], 8);
		aut.delta.add(4, alph["a"], 6);
		aut.delta.add(4, alph["b"], 6);
		aut.delta.add(8, alph["b"], 4);
		aut.delta.add(6, alph["a"], 2);
		aut.delta.add(2, alph["b"], 2);
		aut.delta.add(2, alph["a"], 0);
		aut.delta.add(2, alph["c"], 12);
		aut.delta.add(0, alph["a"], 2);
		aut.delta.add(12, alph["a"], 14);
		aut.delta.add(14, alph["b"], 12);
		aut.final.add({2, 12});

		REQUIRE(!is_complete(aut, alph));

		make_complete(aut, alph, 100);
		REQUIRE(is_complete(aut, alph));
	}

	SECTION("using a non-alphabet symbol")
	{
		OnTheFlyAlphabet alph{};

		aut.initial.add(4);
		aut.delta.add(4, alph["a"], 8);
		aut.delta.add(4, alph["c"], 8);
		aut.delta.add(4, alph["a"], 6);
		aut.delta.add(4, alph["b"], 6);
		aut.delta.add(6, 100, 4);

		CHECK_THROWS_WITH(is_complete(aut, alph),
			Catch::Contains("symbol that is not in the provided alphabet"));
	}
} // }}}

TEST_CASE("Mata::Nfa::is_prfx_in_lang()")
{ // {{{
	Nfa aut('q'+1);

	SECTION("empty automaton")
	{
		Run w;
		w.word = {'a', 'b', 'd'};
		REQUIRE(!is_prfx_in_lang(aut, w));

		w.word = { };
		REQUIRE(!is_prfx_in_lang(aut, w));
	}

	SECTION("automaton accepting only epsilon")
	{
		aut.initial.add('q');
		aut.final.add('q');

		Run w;
		w.word = { };
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = {'a', 'b'};
		REQUIRE(is_prfx_in_lang(aut, w));
	}

	SECTION("small automaton")
	{
		FILL_WITH_AUT_B(aut);

        Run w;
		w.word = {'b', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = { };
		REQUIRE(!is_prfx_in_lang(aut, w));

		w.word = {'c', 'b', 'a'};
		REQUIRE(!is_prfx_in_lang(aut, w));

		w.word = {'c', 'b', 'a', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = {'a', 'a'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = {'c', 'b', 'b', 'a', 'c', 'b'};
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = Word(100000, 'a');
		REQUIRE(is_prfx_in_lang(aut, w));

		w.word = Word(100000, 'b');
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

    aut.add_state(8);
    SECTION("no-transition automaton")
    {
        aut.initial.add(1);
        aut.initial.add(3);

        aut.final.add(2);
        aut.final.add(5);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(1,3));
        REQUIRE(result.get(2,5));
        REQUIRE(!result.get(5,1));
        REQUIRE(!result.get(2,3));
    }

    SECTION("small automaton")
    {
        aut.initial.add(1);
        aut.final.add(2);
        aut.delta.add(1, 'a', 4);
        aut.delta.add(4, 'b', 5);
        aut.delta.add(2, 'b', 5);
        aut.delta.add(1, 'b', 4);

        Simlib::Util::BinaryRelation result = compute_relation(aut);
        REQUIRE(result.get(4,1));
        REQUIRE(!result.get(2,5));

    }

    Nfa aut_big(9);

    SECTION("bigger automaton")
    {
        aut_big.initial = {1, 2};
        aut_big.delta.add(1, 'a', 2);
        aut_big.delta.add(1, 'a', 3);
        aut_big.delta.add(1, 'b', 4);
        aut_big.delta.add(2, 'a', 2);
        aut_big.delta.add(2, 'b', 2);
        aut_big.delta.add(2, 'a', 3);
        aut_big.delta.add(2, 'b', 4);
        aut_big.delta.add(3, 'b', 4);
        aut_big.delta.add(3, 'c', 7);
        aut_big.delta.add(3, 'b', 2);
        aut_big.delta.add(5, 'c', 3);
        aut_big.delta.add(7, 'a', 8);
        aut_big.final = {3};

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
	StateToStateMap state_map;

	SECTION("empty automaton")
	{
		Nfa result = reduce(aut, false, &state_map);

		REQUIRE(nothing_in_trans(result));
		REQUIRE(result.initial.empty());
		REQUIRE(result.final.empty());
	}

	SECTION("simple automaton")
	{
		aut.add_state(2);
        aut.initial.add(1);

        aut.final.add(2);
		Nfa result = reduce(aut, false, &state_map);

		REQUIRE(nothing_in_trans(result));
		REQUIRE(result.initial[state_map[1]]);
		REQUIRE(result.final[state_map[2]]);
		REQUIRE(result.size() == 2);
		REQUIRE(state_map[1] == state_map[0]);
		REQUIRE(state_map[2] != state_map[0]);
	}

	SECTION("big automaton")
	{
		aut.add_state(9);
		aut.initial = {1, 2};
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'a', 3);
		aut.delta.add(1, 'b', 4);
		aut.delta.add(2, 'a', 2);
		aut.delta.add(2, 'b', 2);
		aut.delta.add(2, 'a', 3);
		aut.delta.add(2, 'b', 4);
		aut.delta.add(3, 'b', 4);
		aut.delta.add(3, 'c', 7);
		aut.delta.add(3, 'b', 2);
		aut.delta.add(5, 'c', 3);
		aut.delta.add(7, 'a', 8);
		aut.delta.add(9, 'b', 2);
		aut.delta.add(9, 'c', 0);
		aut.delta.add(0, 'a', 4);
		aut.final = {3, 9};


		Nfa result = reduce(aut, false, &state_map);

		REQUIRE(result.size() == 6);
		REQUIRE(result.initial[state_map[1]]);
		REQUIRE(result.initial[state_map[2]]);
		REQUIRE(result.delta.contains(state_map[9], 'c', state_map[0]));
		REQUIRE(result.delta.contains(state_map[9], 'c', state_map[7]));
		REQUIRE(result.delta.contains(state_map[3], 'c', state_map[0]));
		REQUIRE(result.delta.contains(state_map[0], 'a', state_map[8]));
		REQUIRE(result.delta.contains(state_map[7], 'a', state_map[4]));
		REQUIRE(result.delta.contains(state_map[1], 'a', state_map[3]));
		REQUIRE(!result.delta.contains(state_map[3], 'b', state_map[4]));
		REQUIRE(result.delta.contains(state_map[2], 'a', state_map[2]));
		REQUIRE(result.final[state_map[9]]);
		REQUIRE(result.final[state_map[3]]);

        result = reduce(aut, true, &state_map);
        CHECK(result.size() == 3);
        CHECK(result.initial.size() == 2);
        for (State initial : result.initial) {
            CHECK(((initial == state_map[1]) || (initial == state_map[2])));
        }
        REQUIRE(result.final.size() == 1);
        for (State final : result.final) {
            CHECK(final == state_map[3]);
        }
        CHECK(result.delta.size() == 6);
        CHECK(result.delta.contains(state_map[1], 'a', state_map[3]));
        CHECK(result.delta.contains(state_map[1], 'a', state_map[2]));
        CHECK(result.delta.contains(state_map[2], 'a', state_map[2]));
        CHECK(result.delta.contains(state_map[2], 'b', state_map[2]));
        CHECK(result.delta.contains(state_map[2], 'a', state_map[3]));
        CHECK(result.delta.contains(state_map[3], 'b', state_map[2]));
	}

	SECTION("no transitions from non-final state")
	{
		aut.delta.add(0, 'a', 1);
		aut.initial = { 0 };
		Nfa result = reduce(aut, &state_map);
		CHECK(Mata::Nfa::are_equivalent(result, aut));
	}
}

TEST_CASE("Mata::Nfa::union_norename()") {
    Run one{{1},{}};
    Run zero{{0}, {}};

    Nfa lhs(2);
    lhs.initial.add(0);
    lhs.delta.add(0, 0, 1);
    lhs.final.add(1);
    REQUIRE(!is_in_lang(lhs, one));
    REQUIRE(is_in_lang(lhs, zero));

    Nfa rhs(2);
    rhs.initial.add(0);
    rhs.delta.add(0, 1, 1);
    rhs.final.add(1);
    REQUIRE(is_in_lang(rhs, one));
    REQUIRE(!is_in_lang(rhs, zero));

    SECTION("failing minimal scenario") {
        Nfa result = uni(lhs, rhs);
        REQUIRE(is_in_lang(result, one));
        REQUIRE(is_in_lang(result, zero));
    }
}

TEST_CASE("Mata::Nfa::remove_final()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        REQUIRE(aut.final[2]);
        REQUIRE(aut.final[12]);
        aut.final.remove(12);
        REQUIRE(aut.final[2]);
        REQUIRE(!aut.final[12]);
    }
}

TEST_CASE("Mata::Nfa::delta.remove()")
{
    Nfa aut('q' + 1);

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.delta.add(1, 3, 4);
        aut.delta.add(1, 3, 5);

        SECTION("Simple remove")
        {
            REQUIRE(aut.delta.contains(1, 3, 4));
            REQUIRE(aut.delta.contains(1, 3, 5));
            aut.delta.remove(1, 3, 5);
            REQUIRE(aut.delta.contains(1, 3, 4));
            REQUIRE(!aut.delta.contains(1, 3, 5));
        }

        SECTION("Remove missing transition")
        {
            REQUIRE_THROWS_AS(aut.delta.remove(1, 1, 5), std::invalid_argument);
        }

        SECTION("Remove the last state_to from targets") {
            REQUIRE(aut.delta.contains(6, 'a', 2));
            aut.delta.remove(6, 'a', 2);
            REQUIRE(!aut.delta.contains(6, 'a', 2));
            REQUIRE(aut.delta[6].empty());

            REQUIRE(aut.delta.contains(4, 'a', 8));
            REQUIRE(aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta.contains(4, 'a', 6));
            REQUIRE(aut.delta.contains(4, 'b', 6));
            REQUIRE(aut.delta[4].size() == 3);
            aut.delta.remove(4, 'a', 6);
            REQUIRE(!aut.delta.contains(4, 'a', 6));
            REQUIRE(aut.delta.contains(4, 'b', 6));
            REQUIRE(aut.delta[4].size() == 3);

            aut.delta.remove(4, 'a', 8);
            REQUIRE(!aut.delta.contains(4, 'a', 8));
            REQUIRE(aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta[4].size() == 2);

            aut.delta.remove(4, 'c', 8);
            REQUIRE(!aut.delta.contains(4, 'a', 8));
            REQUIRE(!aut.delta.contains(4, 'c', 8));
            REQUIRE(aut.delta[4].size() == 1);
        }
    }
}

TEST_CASE("Mafa::Nfa::get_moves_from()")
{
    Nfa aut{};

    SECTION("Add new states within the limit")
    {
        aut.add_state(19);
        aut.initial.add(0);
        aut.initial.add(1);
        aut.initial.add(2);
        REQUIRE_NOTHROW(aut.get_moves_from(0));
        REQUIRE_NOTHROW(aut.get_moves_from(1));
        REQUIRE_NOTHROW(aut.get_moves_from(2));
        REQUIRE(aut.get_moves_from(0).empty());
        REQUIRE(aut.get_moves_from(1).empty());
        REQUIRE(aut.get_moves_from(2).empty());
    }

    SECTION("Add new states over the limit")
    {
        aut.add_state(1);
        REQUIRE_NOTHROW(aut.initial.add(0));
        REQUIRE_NOTHROW(aut.initial.add(1));
        //REQUIRE_THROWS_AS(aut.initial.add(2), std::runtime_error);
        REQUIRE_NOTHROW(aut.get_moves_from(0));
        REQUIRE_NOTHROW(aut.get_moves_from(1));
        //REQUIRE_THROWS(aut.get_moves_from(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
        REQUIRE(aut.get_moves_from(0).empty());
        REQUIRE(aut.get_moves_from(1).empty());
        //REQUIRE_THROWS(aut.get_moves_from(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    }

    //TODO: modify or remove these
    //SECTION("Add new states without specifying the number of states")
    //{
    //    REQUIRE_THROWS_AS(aut.initial.add(0), std::runtime_error);
    //    //REQUIRE_THROWS(aut.get_moves_from(2)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    //}

    //SECTION("Add new initial without specifying the number of states with over +1 number")
    //{
    //    REQUIRE_THROWS_AS(aut.initial.add(25), std::runtime_error);
    //    //REQUIRE_THROWS(aut.get_moves_from(25)); // FIXME: Fails on assert. Catch2 cannot catch assert failure.
    //}
}


TEST_CASE("Mata::Nfa::get_trans_as_sequence(}")
{
    Nfa aut('q' + 1);
    TransSequence expected{};

    aut.delta.add(1, 2, 3);
    expected.push_back(Trans{1, 2, 3});
    aut.delta.add(1, 3, 4);
    expected.push_back(Trans{1, 3, 4});
    aut.delta.add(2, 3, 4);
    expected.push_back(Trans{2, 3, 4});


    REQUIRE(aut.get_trans_as_sequence() == expected);
}

TEST_CASE("Mata::Nfa::remove_epsilon()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.remove_epsilon('c');
    REQUIRE(aut.delta.contains(10, 'a', 7));
    REQUIRE(aut.delta.contains(10, 'b', 7));
    REQUIRE(!aut.delta.contains(10, 'c', 7));
    REQUIRE(aut.delta.contains(7, 'a', 5));
    REQUIRE(aut.delta.contains(7, 'a', 3));
    REQUIRE(!aut.delta.contains(7, 'c', 3));
    REQUIRE(aut.delta.contains(7, 'b', 9));
    REQUIRE(aut.delta.contains(7, 'a', 7));
    REQUIRE(aut.delta.contains(5, 'a', 5));
    REQUIRE(!aut.delta.contains(5, 'c', 9));
    REQUIRE(aut.delta.contains(5, 'a', 9));
}

TEST_CASE("Profile Mata::Nfa::remove_epsilon()", "[.profiling]")
{
    for (size_t n{}; n < 100000; ++n) {
        Nfa aut{20};
        FILL_WITH_AUT_A(aut);
        aut.remove_epsilon('c');
    }
}

TEST_CASE("Mata::Nfa::get_num_of_trans()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.get_num_of_trans() == 15);
}

TEST_CASE("Mata::Nfa::get_one_letter_aut()")
{
    Nfa aut(11);
    Symbol abstract_symbol{'x'};
    FILL_WITH_AUT_A(aut);

    Nfa digraph{aut.get_one_letter_aut() };

    REQUIRE(digraph.size() == aut.size());
    REQUIRE(digraph.get_num_of_trans() == 12);
    REQUIRE(digraph.delta.contains(1, abstract_symbol, 10));
    REQUIRE(digraph.delta.contains(10, abstract_symbol, 7));
    REQUIRE(!digraph.delta.contains(10, 'a', 7));
    REQUIRE(!digraph.delta.contains(10, 'b', 7));
    REQUIRE(!digraph.delta.contains(10, 'c', 7));
}

TEST_CASE("Mata::Nfa::get_reachable_states()")
{
    Nfa aut{20};

    SECTION("Automaton A")
    {
        FILL_WITH_AUT_A(aut);
        aut.delta.remove(3, 'b', 9);
        aut.delta.remove(5, 'c', 9);
        aut.delta.remove(1, 'a', 10);

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

        aut.initial.remove(1);
        aut.initial.remove(3);

        reachable = aut.get_reachable_states();
        CHECK(reachable.empty());
    }

    SECTION("Automaton B")
    {
        FILL_WITH_AUT_B(aut);
        aut.delta.remove(2, 'c', 12);
        aut.delta.remove(4, 'c', 8);
        aut.delta.remove(4, 'a', 8);

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

        aut.final.remove(2);
        reachable = aut.get_reachable_states();
        CHECK(reachable.size() == 4);
        CHECK(reachable.find(0) != reachable.end());
        CHECK(reachable.find(2) != reachable.end());
        CHECK(reachable.find(4) != reachable.end());
        CHECK(reachable.find(6) != reachable.end());
        CHECK(aut.get_useful_states().empty());

        aut.final.add(4);
        reachable = aut.get_reachable_states();
        CHECK(reachable.find(4) != reachable.end());
    }
}

TEST_CASE("Mata::Nfa::trim() for profiling", "[.profiling],[trim]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        Nfa new_aut{ aut };
        new_aut.trim();
    }
}

TEST_CASE("Mata::Nfa::get_useful_states() for profiling", "[.profiling],[useful_states]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    for (size_t i{ 0 }; i < 10000; ++i) {
        aut.get_useful_states();
    }
}

TEST_CASE("Mata::Nfa::trim()")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.remove(1, 'a', 10);

    Nfa old_aut{aut};

    SECTION("Without state map") {
        aut.trim();
        CHECK(aut.initial.size() == old_aut.initial.size());
        CHECK(aut.final.size() == old_aut.final.size());
        CHECK(aut.size() == 4);
        for (const Word& word: get_shortest_words(old_aut))
        {
            CHECK(is_in_lang(aut, Run{word,{}}));
        }

        aut.final.remove(2); // '2' is the new final state in the earlier trimmed automaton.
        aut.trim();
        CHECK(aut.delta.empty());
        CHECK(aut.size() == 0);
    }

    SECTION("With state map") {
        StateToStateMap state_map{};
        aut.trim(&state_map);
        CHECK(aut.initial.size() == old_aut.initial.size());
        CHECK(aut.final.size() == old_aut.final.size());
        CHECK(aut.size() == 4);
        for (const Word& word: get_shortest_words(old_aut))
        {
            CHECK(is_in_lang(aut, Run{word,{}}));
        }
        REQUIRE(state_map.size() == 4);
        CHECK(state_map.at(1) == 0);
        CHECK(state_map.at(3) == 1);
        CHECK(state_map.at(7) == 3);
        CHECK(state_map.at(5) == 2);

        aut.final.remove(2); // '2' is the new final state in the earlier trimmed automaton.
        aut.trim(&state_map);
        CHECK(aut.delta.empty());
        CHECK(aut.size() == 0);
        CHECK(state_map.empty());
    }
}

TEST_CASE("Mata::Nfa::Nfa::delta.empty()")
{
    Nfa aut{};

    SECTION("Empty automaton")
    {
        CHECK(aut.delta.empty());
    }

    SECTION("No transitions automaton")
    {
        aut.add_state();
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with no transitions")
    {
        aut.add_state();
        aut.initial.add(0);
        aut.final.add(0);
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state();
        aut.initial.add(0);
        aut.final.add(0);
        aut.delta.add(0, 'a', 0);
        CHECK(!aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state(1);
        aut.initial.add(0);
        aut.final.add(1);
        CHECK(aut.delta.empty());
    }

    SECTION("Single state automaton with transitions")
    {
        aut.add_state(1);
        aut.initial.add(0);
        aut.final.add(1);
        aut.delta.add(0, 'a', 1);
        CHECK(!aut.delta.empty());
    }
}

TEST_CASE("Mata::Nfa::delta.operator[]")
{
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    REQUIRE(aut.get_num_of_trans() == 15);
    aut.delta[25];
    REQUIRE(aut.size() == 20);

    aut.delta.get_mutable_post(25);
    REQUIRE(aut.size() == 26);
    REQUIRE(aut.delta[25].empty());

    aut.delta.get_mutable_post(50);
    REQUIRE(aut.size() == 51);
    REQUIRE(aut.delta[50].empty());

    Nfa aut1 = aut;
    aut1.delta.get_mutable_post(60);
    REQUIRE(aut1.size() == 61);
    REQUIRE(aut1.delta[60].empty());

    const Nfa aut2 = aut;
    aut2.delta[60];
    REQUIRE(aut2.size() == 51);
    REQUIRE(aut2.delta[60].empty());
}

TEST_CASE("Mata::Nfa::Nfa::unify_(initial/final)()") {
    Nfa nfa{10};

    SECTION("No initial") {
        nfa.unify_initial();
        CHECK(nfa.size() == 10);
        CHECK(nfa.initial.empty());
    }

    SECTION("initial==final unify final") {
        nfa.initial.add(0);
        nfa.final.add(0);
        nfa.final.add(1);
        nfa.unify_final();
        REQUIRE(nfa.size() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
        CHECK(nfa.initial[10]);
    }

    SECTION("initial==final unify initial") {
        nfa.initial.add(0);
        nfa.initial.add(1);
        nfa.final.add(0);
        nfa.unify_initial();
        REQUIRE(nfa.size() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
        CHECK(nfa.final[10]);
    }

    SECTION("Single initial") {
        nfa.initial.add(0);
        nfa.unify_initial();
        CHECK(nfa.size() == 10);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[0]);
    }

    SECTION("Multiple initial") {
        nfa.initial.add(0);
        nfa.initial.add(1);
        nfa.unify_initial();
        CHECK(nfa.size() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
    }

    SECTION("With transitions") {
        nfa.initial.add(0);
        nfa.initial.add(1);
        nfa.delta.add(0, 'a', 3);
        nfa.delta.add(1, 'b', 0);
        nfa.delta.add(1, 'c', 1);
        nfa.unify_initial();
        CHECK(nfa.size() == 11);
        CHECK(nfa.initial.size() == 1);
        CHECK(nfa.initial[10]);
        CHECK(nfa.delta.contains(10, 'a', 3));
        CHECK(nfa.delta.contains(10, 'b', 0));
        CHECK(nfa.delta.contains(10, 'c', 1));
        CHECK(nfa.delta.contains(0, 'a', 3));
        CHECK(nfa.delta.contains(1, 'b', 0));
        CHECK(nfa.delta.contains(1, 'c', 1));
    }

    SECTION("No final") {
        nfa.unify_final();
        CHECK(nfa.size() == 10);
        CHECK(nfa.final.empty());
    }

    SECTION("Single final") {
        nfa.final.add(0);
        nfa.unify_final();
        CHECK(nfa.size() == 10);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[0]);
    }

    SECTION("Multiple final") {
        nfa.final.add(0);
        nfa.final.add(1);
        nfa.unify_final();
        CHECK(nfa.size() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
    }

    SECTION("With transitions") {
        nfa.final.add(0);
        nfa.final.add(1);
        nfa.delta.add(3, 'a', 0);
        nfa.delta.add(4, 'b', 1);
        nfa.delta.add(1, 'c', 1);
        nfa.unify_final();
        CHECK(nfa.size() == 11);
        CHECK(nfa.final.size() == 1);
        CHECK(nfa.final[10]);
        CHECK(nfa.delta.contains(3, 'a', 10));
        CHECK(nfa.delta.contains(4, 'b', 10));
        CHECK(nfa.delta.contains(1, 'c', 10));
        CHECK(nfa.delta.contains(3, 'a', 0));
        CHECK(nfa.delta.contains(4, 'b', 1));
        CHECK(nfa.delta.contains(1, 'c', 1));
    }
}

TEST_CASE("Mata::Nfa::Nfa::get_epsilon_transitions()") {
    Nfa aut{20};
    FILL_WITH_AUT_A(aut);
    aut.delta.add(0, EPSILON, 3);
    aut.delta.add(3, EPSILON, 3);
    aut.delta.add(3, EPSILON, 4);

    auto state_eps_trans{ aut.get_epsilon_transitions(0) };
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3 });
    state_eps_trans = aut.get_epsilon_transitions(3);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3, 4 });

    aut.delta.add(8, 42, 3);
    aut.delta.add(8, 42, 4);
    aut.delta.add(8, 42, 6);

    state_eps_trans = aut.get_epsilon_transitions(8, 42);
    CHECK(state_eps_trans->symbol == 42);
    CHECK(state_eps_trans->targets == StateSet{3, 4, 6 });

    CHECK(aut.get_epsilon_transitions(1) == aut.get_moves_from(1).end());
    CHECK(aut.get_epsilon_transitions(5) == aut.get_moves_from(5).end());
    CHECK(aut.get_epsilon_transitions(19) == aut.get_moves_from(19).end());

    auto state_transitions{ aut.delta[0] };
    state_eps_trans = aut.get_epsilon_transitions(state_transitions);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3 });
    state_transitions = aut.delta[3];
    state_eps_trans = aut.get_epsilon_transitions(state_transitions);
    CHECK(state_eps_trans->symbol == EPSILON);
    CHECK(state_eps_trans->targets == StateSet{3, 4 });

    state_transitions = aut.get_moves_from(1);
    CHECK(aut.get_epsilon_transitions(state_transitions) == state_transitions.end());
    state_transitions = aut.get_moves_from(5);
    CHECK(aut.get_epsilon_transitions(state_transitions) == state_transitions.end());
    state_transitions = aut.get_moves_from(19);
    CHECK(aut.get_epsilon_transitions(state_transitions) == state_transitions.end());
}

TEST_CASE("Mata::Nfa::Nfa::defragment()") {
    Nfa aut{};
    aut.delta.add(0, 42, 2);
    aut.delta.add(0, 42, 1);
    aut.delta.add(1, 42, 2);
    aut.delta.add(1, 42, 4);
    aut.delta.add(2, 42, 2);
    aut.delta.add(4, 42, 2);
    aut.delta.add(4, 42, 1);
    aut.delta.add(5, 42, 4);
    aut.initial.add(3);
    aut.final.add(3);
    aut.final.add(4);
    CHECK(aut.initial[3]);
    CHECK(aut.final[3]);
    CHECK(aut.final[4]);
    CHECK(aut.size() == 6);
    CHECK(aut.delta.contains(1, 42, 2));
    aut.defragment();
    CHECK(aut.initial[5]);
    CHECK(aut.final[5]);
    CHECK(aut.final[3]);
    CHECK(!aut.final[4]);
    CHECK(aut.size() == 6);
    CHECK(aut.delta.contains(1, 42, 3));
    CHECK(aut.delta.contains(4, 42, 3)); // previously (5,42,4)
    aut.defragment();
    CHECK(aut.initial[5]);
    CHECK(aut.final[5]);
    CHECK(aut.size() == 6);
    // transitions contains not been changed
    CHECK(aut.delta.contains(1, 42, 3));
    CHECK(aut.delta.contains(4, 42, 3));
}

TEST_CASE("Mata::Nfa::Nfa::delta()") {
    Delta delta(6);
}

TEST_CASE("A segmentation fault in the make_complement") {
    Nfa r(1);
    OnTheFlyAlphabet alph{};
    alph["a"];
    alph["b"];

    r.initial = {0};
    r.delta.add(0, 0, 0);
    REQUIRE(not is_complete(r, alph));
    make_complete(r, alph, 1);
    REQUIRE(is_complete(r, alph));
}

TEST_CASE("Mata::Nfa:: create simple automata") {
    Nfa nfa{ create_empty_string_nfa() };
    CHECK(is_in_lang(nfa, { {}, {} }));
    CHECK(get_word_lengths(nfa) == std::set<std::pair<int, int>>{ std::make_pair(0, 0) });

    OnTheFlyAlphabet alphabet{};
    StringToSymbolMap symbol_map{ { "a", 0 }, { "b", 1 }, { "c", 2 } };
    alphabet.add_symbols_from(symbol_map);
    nfa = create_sigma_star_nfa(&alphabet);
    CHECK(is_in_lang(nfa, { {}, {} }));
    CHECK(is_in_lang(nfa, { { 0 }, {} }));
    CHECK(is_in_lang(nfa, { { 1 }, {} }));
    CHECK(is_in_lang(nfa, { { 2 }, {} }));
    CHECK(is_in_lang(nfa, { { 0, 1 }, {} }));
    CHECK(is_in_lang(nfa, { { 1, 0 }, {} }));
    CHECK(is_in_lang(nfa, { { 2, 2, 2 }, {} }));
    CHECK(is_in_lang(nfa, { { 0, 1, 2, 2, 0, 1, 2, 1, 0, 0, 2, 1 }, {} }));
    CHECK(!is_in_lang(nfa, { { 3 }, {} }));
}
