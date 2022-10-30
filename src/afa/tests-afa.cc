// TODO: some header

#include "../3rdparty/catch.hpp"

#include <unordered_set>

#include <mata/afa.hh>
using namespace Mata::Afa;
using namespace Mata::util;
using namespace Mata::Parser;

TEST_CASE("Mata::Afa::Trans::operator<<")
{ // {{{
	Trans trans(1, 0, {{0, 1}, {0, 2}});

	REQUIRE(std::to_string(trans) == "(1, 0, { { 0, 1}, { 0, 2}})");
} // }}}

TEST_CASE("Mata::Afa::ClosedSet creating closed sets")
{ // {{{
    ClosedSet<State> c1 = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, 2, Nodes());
    ClosedSet<State> c2 = ClosedSet<State>(ClosedSet<State>::downward_closed, 10, 20, Nodes());

    REQUIRE(c1.get_type() == ClosedSet<State>::upward_closed);
    REQUIRE(c2.get_type() == ClosedSet<State>::downward_closed);
    REQUIRE(c1.get_type() != c2.get_type());
    REQUIRE(c1.get_antichain().size() == 0);
    REQUIRE(c2.get_antichain().size() == 0);

} // }}}

TEST_CASE("Mata::Afa::ClosedSet operation over closed sets")
{ // {{{
    ClosedSet<State> c1 = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, 3, Nodes());
    ClosedSet<State> c2 = ClosedSet<State>(ClosedSet<State>::downward_closed, 0, 3, Nodes());

    REQUIRE(!c1.contains(Node{0}));
    REQUIRE(!c2.contains(Node{0}));

    c1.insert(Node{0, 1});    
    c2.insert(Node{0, 1});

    REQUIRE(!c1.contains(Node{0}));
    REQUIRE(c2.contains(Node{0}));

    c1.insert(Node{0, 2});    
    c2.insert(Node{0, 2});

    REQUIRE(c1.contains(Node{0, 1, 2}));
    REQUIRE(!c2.contains(Node{0, 1, 2}));
    REQUIRE(!c1.contains(Node{}));
    REQUIRE(c2.contains(Node{}));

    ClosedSet<State> c3 = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, 3, Nodes{Node{0, 1}});
    ClosedSet<State> c4 = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, 3, Nodes{Node{0, 3}});
    
    REQUIRE(c3.Union(c4).contains(Node{0, 1}));
    REQUIRE(c3.Union(c4).contains(Node{0, 3}));
    REQUIRE(!c3.intersection(c4).contains(Node{0, 1}));
    REQUIRE(!c3.intersection(c4).contains(Node{0, 3}));

    REQUIRE(c3.Union(c4).contains(Node{0, 1, 3}));
    REQUIRE(c3.intersection(c4).contains(Node{0, 1, 3}));
    REQUIRE(!c3.Union(c4).contains(Node{}));
    REQUIRE(!c3.intersection(c4).contains(Node{}));

    ///

    ClosedSet<State> c5 = ClosedSet<State>(ClosedSet<State>::downward_closed, 0, 3, Nodes{Node{0, 1}});
    ClosedSet<State> c6 = ClosedSet<State>(ClosedSet<State>::downward_closed, 0, 3, Nodes{Node{0, 3}});
    
    REQUIRE(c5.Union(c6).contains(Node{0, 1}));
    REQUIRE(c5.Union(c6).contains(Node{0, 3}));
    REQUIRE(!c5.intersection(c6).contains(Node{0, 1}));
    REQUIRE(!c5.intersection(c6).contains(Node{0, 3}));

    REQUIRE(!c5.Union(c6).contains(Node{0, 1, 3}));
    REQUIRE(!c5.intersection(c6).contains(Node{0, 1, 3}));
    REQUIRE(c5.Union(c6).contains(Node{}));
    REQUIRE(c5.intersection(c6).contains(Node{}));

    ///

    REQUIRE(std::to_string(c5.Union(c6).get_antichain()) == "{ { 0, 1}, { 0, 3}}");
    REQUIRE(std::to_string(c5.intersection(c6).get_antichain()) == "{ { 0}}");

    ClosedSet<State> c7 = ClosedSet<State>(ClosedSet<State>::downward_closed, 0, 3, Nodes{Node{0, 3}});
    c7.insert(Node{0});
    REQUIRE(std::to_string(c7.get_antichain()) == "{ { 0, 3}}");
    c7.insert(Node{0, 2});
    REQUIRE(std::to_string(c7.get_antichain()) == "{ { 0, 2}, { 0, 3}}");
    c7.insert(Node{0, 2, 3});
    REQUIRE(std::to_string(c7.get_antichain()) == "{ { 0, 2, 3}}");

    ClosedSet<State> c8 = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, 3, Nodes{Node{0, 3}});
    c8.insert(Node{0, 1, 3});
    REQUIRE(std::to_string(c8.get_antichain()) == "{ { 0, 3}}");
    c8.insert(Node{0, 2});
    REQUIRE(std::to_string(c8.get_antichain()) == "{ { 0, 2}, { 0, 3}}");
    c8.insert(Node{0});
    REQUIRE(std::to_string(c8.get_antichain()) == "{ { 0}}");

} // }}}


TEST_CASE("Mata::Afa creating an AFA, basic properties")
{ // {{{

    Afa aut(4);

	aut.initialstates = {0};
	aut.finalstates = {3};
	aut.add_trans(0, 0, Node{1, 2});
	aut.add_trans(1, 0, Node{2});
	aut.add_trans(1, 1, Node{2, 3});
	aut.add_trans(2, 1, Node{3});
	aut.add_trans(3, 1, Node{3});
	aut.add_trans(3, 0, Node{0});

	REQUIRE(aut.trans_size() == 6);
	REQUIRE(aut.has_final(3));
	REQUIRE(!aut.has_final(2));
	REQUIRE(aut.has_initial(0));
	REQUIRE(!aut.has_initial(1));
} // }}}

TEST_CASE("Mata::Afa transition test")
{ // {{{

    Afa aut(3);

	aut.initialstates = {0};
	aut.finalstates = {2};
	aut.add_trans(0, 0, Nodes{Node{0}});
	aut.add_trans(0, 1, Nodes{Node{1}});
	aut.add_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_trans(2, 0, Nodes{Node{2}, Node{0, 1}});

    REQUIRE(std::to_string(aut.post(Nodes{}).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{}}).get_antichain()) == "{ {}}");

    REQUIRE(std::to_string(aut.post(0, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.post(Node{0}, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{0}}, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{0}}), 0).get_antichain()) == "{ { 0}}");

    REQUIRE(std::to_string(aut.post(0, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.post(Node{0}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{0}}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{0}}), 1).get_antichain()) == "{ { 1}}");

    REQUIRE(std::to_string(aut.post(1, 0).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{1}, 0).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{1}}, 0).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{1}}), 0).get_antichain()) == "{}");

    REQUIRE(std::to_string(aut.post(1, 1).get_antichain()) == "{ { 0}, { 1, 2}}");
    REQUIRE(std::to_string(aut.post(Node{1}, 1).get_antichain()) == "{ { 0}, { 1, 2}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{1}}, 1).get_antichain()) == "{ { 0}, { 1, 2}}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{1}}), 1).get_antichain()) == "{ { 0}, { 1, 2}}");

    REQUIRE(std::to_string(aut.post(2, 0).get_antichain()) == "{ { 0, 1}, { 2}}");
    REQUIRE(std::to_string(aut.post(Node{2}, 0).get_antichain()) == "{ { 0, 1}, { 2}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{2}}, 0).get_antichain()) == "{ { 0, 1}, { 2}}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{2}}), 0).get_antichain()) == "{ { 0, 1}, { 2}}");

    REQUIRE(std::to_string(aut.post(2, 1).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{2}, 1).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{2}}, 1).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(ClosedSet<State>
    (ClosedSet<State>::upward_closed, 0, 2, Nodes{Node{2}}), 1).get_antichain()) == "{}");

    REQUIRE(std::to_string(aut.post(Node{0, 1}, 0).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{0, 2}, 0).get_antichain()) == "{ { 0, 1}, { 0, 2}}");
    REQUIRE(std::to_string(aut.post(Node{1, 2}, 0).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{0, 1, 2}, 0).get_antichain()) == "{}");

    REQUIRE(std::to_string(aut.post(Node{0, 1}, 1).get_antichain()) == "{ { 0, 1}, { 1, 2}}");
    REQUIRE(std::to_string(aut.post(Node{0, 2}, 1).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{1, 2}, 1).get_antichain()) == "{}");
    REQUIRE(std::to_string(aut.post(Node{0, 1, 2}, 1).get_antichain()) == "{}");

    REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}, 1).get_antichain()) == "{ { 0}, { 1}}");
    REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}).get_antichain()) == "{ { 0}, { 1}}");

} // }}}

TEST_CASE("Mata::Afa inverse transition test")
{ // {{{

    Afa aut(3);

	aut.initialstates = {0};
	aut.finalstates = {2};
	aut.add_inverse_trans(0, 0, Nodes{Node{0}});
	aut.add_inverse_trans(0, 1, Nodes{Node{1}});
	aut.add_inverse_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_inverse_trans(2, 0, Nodes{Node{2}, Node{0, 1}});

    REQUIRE(std::to_string(aut.pre(Node{}, 0).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(Node{}, 1).get_antichain()) == "{ {}}");

    REQUIRE(std::to_string(aut.pre(0, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(Node{0}, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{0}}, 0).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{0}}), 0).get_antichain()) == "{ { 0}}");

    REQUIRE(std::to_string(aut.pre(1, 0).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(Node{1}, 0).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{1}}, 0).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{1}}), 0).get_antichain()) == "{ {}}");

    REQUIRE(std::to_string(aut.pre(2, 0).get_antichain()) == "{ { 2}}");
    REQUIRE(std::to_string(aut.pre(Node{2}, 0).get_antichain()) == "{ { 2}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{2}}, 0).get_antichain()) == "{ { 2}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{2}}), 0).get_antichain()) == "{ { 2}}");


    REQUIRE(std::to_string(aut.pre(0, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.pre(Node{0}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{0}}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{0}}), 1).get_antichain()) == "{ { 1}}");

    REQUIRE(std::to_string(aut.pre(1, 1).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(Node{1}, 1).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{1}}, 1).get_antichain()) == "{ { 0}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{1}}), 1).get_antichain()) == "{ { 0}}");

    REQUIRE(std::to_string(aut.pre(2, 1).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(Node{2}, 1).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{2}}, 1).get_antichain()) == "{ {}}");
    REQUIRE(std::to_string(aut.pre(ClosedSet<State>
    (ClosedSet<State>::downward_closed, 0, 2, Nodes{Node{2}}), 1).get_antichain()) == "{ {}}");


    REQUIRE(std::to_string(aut.pre(Node{0, 1}, 0).get_antichain()) == "{ { 0, 2}}");
    REQUIRE(std::to_string(aut.pre(Node{0, 2}, 0).get_antichain()) == "{ { 0, 2}}");
    REQUIRE(std::to_string(aut.pre(Node{1, 2}, 0).get_antichain()) == "{ { 2}}");
    REQUIRE(std::to_string(aut.pre(Node{0, 1, 2}, 0).get_antichain()) == "{ { 0, 2}}");

    REQUIRE(std::to_string(aut.pre(Node{0, 1}, 1).get_antichain()) == "{ { 0, 1}}");
    REQUIRE(std::to_string(aut.pre(Node{0, 2}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.pre(Node{1, 2}, 1).get_antichain()) == "{ { 0, 1}}");
    REQUIRE(std::to_string(aut.pre(Node{0, 1, 2}, 1).get_antichain()) == "{ { 0, 1}}");

    REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}, 0).get_antichain()) == "{ { 0}, { 2}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}, 1).get_antichain()) == "{ { 1}}");
    REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}).get_antichain()) == "{ { 0}, { 1}, { 2}}");

} // }}}

TEST_CASE("Mata::Afa antichain emptiness test")
{
    /////////////////////////////////
    // Example of an automaton
    /////////////////////////////////

    Afa aut(3);

	aut.initialstates = {0};
	aut.finalstates = {2};

    // TODO: Add transition and inverse transition simultaneously???
    // Do we always care about inverse transitions (in context of other
    // operations than backward emptiness test etc.?) Is it better
    // to add inverse transitions on demand or always?

	aut.add_trans(0, 0, Nodes{Node{0}});
	aut.add_trans(0, 1, Nodes{Node{1}});
	aut.add_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_trans(2, 0, Nodes{Node{2}, Node{0, 1}});
	
    aut.add_inverse_trans(0, 0, Nodes{Node{0}});
	aut.add_inverse_trans(0, 1, Nodes{Node{1}});
	aut.add_inverse_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_inverse_trans(2, 0, Nodes{Node{2}, Node{0, 1}});

    REQUIRE(antichain_concrete_forward_emptiness_test_old(aut));
    REQUIRE(antichain_concrete_backward_emptiness_test_old(aut));

    REQUIRE(antichain_concrete_forward_emptiness_test_new(aut));
    REQUIRE(antichain_concrete_backward_emptiness_test_new(aut));

	aut.finalstates = {0};
    REQUIRE(!antichain_concrete_forward_emptiness_test_old(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_old(aut));

    REQUIRE(!antichain_concrete_forward_emptiness_test_new(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_new(aut));


	aut.finalstates = {1};

    REQUIRE(!antichain_concrete_forward_emptiness_test_old(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_old(aut));

    REQUIRE(!antichain_concrete_forward_emptiness_test_new(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_new(aut));

    /////////////////////////////////
    // Example of an automaton
    /////////////////////////////////

    Afa aut1(10);

	aut1.initialstates = {0};
	aut1.finalstates = {9};

    aut1.add_trans(0, 0, Nodes{Node{1}});
    aut1.add_trans(1, 0, Nodes{Node{2}});
    aut1.add_trans(2, 0, Nodes{Node{3}});
    aut1.add_trans(3, 0, Nodes{Node{4}});
    aut1.add_trans(4, 0, Nodes{Node{5}});
    aut1.add_trans(5, 0, Nodes{Node{6}});
    aut1.add_trans(6, 0, Nodes{Node{7}});
    aut1.add_trans(7, 0, Nodes{Node{8}});
    aut1.add_trans(8, 0, Nodes{Node{8, 9}});

    aut1.add_inverse_trans(0, 0, Nodes{Node{1}});
    aut1.add_inverse_trans(1, 0, Nodes{Node{2}});
    aut1.add_inverse_trans(2, 0, Nodes{Node{3}});
    aut1.add_inverse_trans(3, 0, Nodes{Node{4}});
    aut1.add_inverse_trans(4, 0, Nodes{Node{5}});
    aut1.add_inverse_trans(5, 0, Nodes{Node{6}});
    aut1.add_inverse_trans(6, 0, Nodes{Node{7}});
    aut1.add_inverse_trans(7, 0, Nodes{Node{8}});
    aut1.add_inverse_trans(8, 0, Nodes{Node{8, 9}});

    REQUIRE(antichain_concrete_forward_emptiness_test_old(aut1));
    REQUIRE(antichain_concrete_backward_emptiness_test_old(aut1));

    REQUIRE(antichain_concrete_forward_emptiness_test_new(aut1));
    REQUIRE(antichain_concrete_backward_emptiness_test_new(aut1));

    aut1.add_trans(8, 0, Nodes{Node{9}});
    aut1.add_trans(8, 0, Nodes{Node{9}});

    aut1.add_inverse_trans(8, 0, Nodes{Node{9}});
    aut1.add_inverse_trans(8, 0, Nodes{Node{9}});

    REQUIRE(!antichain_concrete_forward_emptiness_test_old(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_old(aut));

    REQUIRE(!antichain_concrete_forward_emptiness_test_new(aut));
    REQUIRE(!antichain_concrete_backward_emptiness_test_new(aut));

    /////////////////////////////////
    // Automaton with no transitions
    /////////////////////////////////

    Afa aut2(3);

    REQUIRE(antichain_concrete_forward_emptiness_test_old(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_old(aut2));

    REQUIRE(antichain_concrete_forward_emptiness_test_new(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_new(aut2));

    aut2.initialstates = {0};

    REQUIRE(antichain_concrete_forward_emptiness_test_old(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_old(aut2));

    REQUIRE(antichain_concrete_forward_emptiness_test_new(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_new(aut2));

    aut2.finalstates = {1};

    REQUIRE(antichain_concrete_forward_emptiness_test_old(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_old(aut2));

    REQUIRE(antichain_concrete_forward_emptiness_test_new(aut2));
    REQUIRE(antichain_concrete_backward_emptiness_test_new(aut2));

    aut2.finalstates = {0};

    REQUIRE(!antichain_concrete_forward_emptiness_test_old(aut2));
    REQUIRE(!antichain_concrete_backward_emptiness_test_old(aut2));

    REQUIRE(!antichain_concrete_forward_emptiness_test_new(aut2));
    REQUIRE(!antichain_concrete_backward_emptiness_test_new(aut2));

}


/*
TEST_CASE("Mata::Afa::construct() correct calls")
{ // {{{
	Afa aut;
	Mata::Parser::ParsedSection parsec;

	SECTION("construct an empty automaton")
	{
		parsec.type = Mata::Afa::TYPE_AFA;

		construct(&aut, parsec);

		// REQUIRE(is_lang_empty(aut));
	}

	SECTION("construct a simple non-empty automaton accepting the empty word")
	{
		parsec.type = Mata::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q1"}});

		construct(&aut, parsec);

		// REQUIRE(!is_lang_empty(aut));
	}

	SECTION("construct an automaton with more than one initial/final states")
	{
		parsec.type = Mata::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1", "q2"}});
		parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

		construct(&aut, parsec);

		// REQUIRE(aut.initialstates.size() == 2);
		// REQUIRE(aut.finalstates.size() == 3);
	}

	SECTION("construct a simple non-empty automaton accepting only the word 'a'")
	{
		parsec.type = Mata::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q2"}});
		parsec.body = { {"q1", "a AND q2"} };

		construct(&aut, parsec);
	}
} // }}} */
