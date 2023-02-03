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
	StateClosedSet c1 = StateClosedSet(Mata::upward_closed_set, 0, 2, Nodes());
	StateClosedSet c2 = StateClosedSet(Mata::downward_closed_set, 10, 20, Nodes());

	REQUIRE(c1.type() == Mata::upward_closed_set);
	REQUIRE(c2.type() == Mata::downward_closed_set);
	REQUIRE(c1.type() != c2.type());
	REQUIRE(c1.antichain().size() == 0);
	REQUIRE(c2.antichain().size() == 0);

} // }}}

TEST_CASE("Mata::Afa::ClosedSet operation over closed sets")
{ // {{{
	StateClosedSet c1 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes());
	StateClosedSet c2 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes());

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

	StateClosedSet c3 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes{Node{0, 1}});
	StateClosedSet c4 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes{Node{0, 3}});

	REQUIRE(c3.Union(c4).contains(Node{0, 1}));
	REQUIRE(c3.Union(c4).contains(Node{0, 3}));
	REQUIRE(!c3.intersection(c4).contains(Node{0, 1}));
	REQUIRE(!c3.intersection(c4).contains(Node{0, 3}));

	REQUIRE(c3.Union(c4).contains(Node{0, 1, 3}));
	REQUIRE(c3.intersection(c4).contains(Node{0, 1, 3}));
	REQUIRE(!c3.Union(c4).contains(Node{}));
	REQUIRE(!c3.intersection(c4).contains(Node{}));

	StateClosedSet c5 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes{Node{0, 1}});
	StateClosedSet c6 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes{Node{0, 3}});

	REQUIRE(c5.Union(c6).contains(Node{0, 1}));
	REQUIRE(c5.Union(c6).contains(Node{0, 3}));
	REQUIRE(!c5.intersection(c6).contains(Node{0, 1}));
	REQUIRE(!c5.intersection(c6).contains(Node{0, 3}));

	REQUIRE(!c5.Union(c6).contains(Node{0, 1, 3}));
	REQUIRE(!c5.intersection(c6).contains(Node{0, 1, 3}));
	REQUIRE(c5.Union(c6).contains(Node{}));
	REQUIRE(c5.intersection(c6).contains(Node{}));

	REQUIRE(std::to_string(c5.Union(c6).antichain()) == "{ { 0, 1}, { 0, 3}}");
	REQUIRE(std::to_string(c5.intersection(c6).antichain()) == "{ { 0}}");

	StateClosedSet c7 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes{Node{0, 3}});
	c7.insert(Node{0});
	REQUIRE(std::to_string(c7.antichain()) == "{ { 0, 3}}");
	c7.insert(Node{0, 2});
	REQUIRE(std::to_string(c7.antichain()) == "{ { 0, 2}, { 0, 3}}");
	c7.insert(Node{0, 2, 3});
	REQUIRE(std::to_string(c7.antichain()) == "{ { 0, 2, 3}}");

	StateClosedSet c8 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes{Node{0, 3}});
	c8.insert(Node{0, 1, 3});
	REQUIRE(std::to_string(c8.antichain()) == "{ { 0, 3}}");
	c8.insert(Node{0, 2});
	REQUIRE(std::to_string(c8.antichain()) == "{ { 0, 2}, { 0, 3}}");
	c8.insert(Node{0});
	REQUIRE(std::to_string(c8.antichain()) == "{ { 0}}");

	StateClosedSet c9 = StateClosedSet(Mata::upward_closed_set, 0, 4, Nodes());

	c9.insert(Node{1, 4});    
	c9.insert(Node{1, 2, 3});

	REQUIRE(c9.complement().antichain() == Nodes{{0, 1, 2}, {0, 1, 3}, {0, 2, 3, 4}});
	REQUIRE(!(c9.complement().antichain() == Nodes{{0, 2}, {0, 1, 3}, {0, 2, 3, 4}}));

	REQUIRE(c9.complement().complement().antichain() == Nodes{{1, 4}, {1, 2, 3}});
	REQUIRE(!(c9.complement().complement().antichain() == Nodes{{0, 1, 4}, {1, 2, 3}}));

	StateClosedSet c10 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes());

	c10.insert(Node{1, 2});
	c10.insert(Node{2, 3});

	REQUIRE(c10.complement().antichain() == Nodes{{0}, {1, 3}});
	
	REQUIRE(c10.type() == Mata::downward_closed_set);
	c10 = c10.complement();	
	REQUIRE(c10.type() == Mata::upward_closed_set);

	StateClosedSet c11 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes());
	REQUIRE(c11.complement().antichain() == Nodes{{}});

	StateClosedSet c12 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes());
	REQUIRE(c12.complement().antichain() == Nodes{{0, 1, 2, 3}});

	StateClosedSet c13 = StateClosedSet(Mata::downward_closed_set, 0, 3, Nodes());
	c13.insert(Node{0, 1, 2, 3});
	REQUIRE(c13.complement().antichain() == Nodes{});

	StateClosedSet c14 = StateClosedSet(Mata::upward_closed_set, 0, 3, Nodes());
	c14.insert(Node{0, 1, 2, 3});
	REQUIRE(c14.complement().antichain() == Nodes{{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}});

} // }}}


TEST_CASE("Mata::Afa creating an AFA, basic properties")
{ // {{{

	Afa aut(4);

	aut.initialstates = {{0}};
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



	REQUIRE(aut.get_num_of_states() == 4);
	REQUIRE(aut.add_new_state() == 4);
	REQUIRE(aut.add_new_state() == 5);
	REQUIRE(aut.add_new_state() == 6);
	REQUIRE(aut.add_new_state() == 7);
	REQUIRE(aut.get_num_of_states() == 8);

	auto transitions1 = aut.get_trans_from_state(0);
	auto transitions2 = aut.get_trans_from_state(1);

	REQUIRE(transitions1.size() == 1);
	REQUIRE(transitions2.size() == 2);

	REQUIRE(transitions1[0].src == 0);
	REQUIRE(transitions1[0].symb == 0);
	REQUIRE(transitions1[0].dst == Nodes{Node{1, 2}});

	REQUIRE(transitions2[0].src == 1);
	REQUIRE(transitions2[0].symb == 0);
	REQUIRE(transitions2[0].dst == Nodes{Node{2}});

	REQUIRE(transitions2[1].src == 1);
	REQUIRE(transitions2[1].symb == 1);
	REQUIRE(transitions2[1].dst == Nodes{Node{2, 3}});

	aut.add_trans(7, 0, Node{0});

	REQUIRE(aut.trans_size() == 7);

	aut.add_trans(7, 0, Node{1});
	aut.add_trans(7, 0, Node{2, 3});

	auto transitions3 = aut.get_trans_from_state(7);

	REQUIRE(transitions3.size() == 1);
	REQUIRE(transitions3[0].src == 7);
	REQUIRE(transitions3[0].symb == 0);
	REQUIRE(transitions3[0].dst == Nodes{Node{0}, Node{1}, Node{2, 3}});

} // }}}

TEST_CASE("Mata::Afa transition test")
{ // {{{

	Afa aut(3);

	aut.initialstates = {{0}};
	aut.finalstates = {2};
	aut.add_trans(0, 0, Nodes{Node{0}});
	aut.add_trans(0, 1, Nodes{Node{1}});
	aut.add_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_trans(2, 0, Nodes{Node{2}, Node{0, 1}});

	REQUIRE(std::to_string(aut.post(Nodes{}).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{}}).antichain()) == "{ {}}");

	REQUIRE(std::to_string(aut.post(0, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.post(Node{0}, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{0}}, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{0}}), 0).antichain()) == "{ { 0}}");

	REQUIRE(std::to_string(aut.post(0, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.post(Node{0}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{0}}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{0}}), 1).antichain()) == "{ { 1}}");

	REQUIRE(std::to_string(aut.post(1, 0).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{1}, 0).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{1}}, 0).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{1}}), 0).antichain()) == "{}");

	REQUIRE(std::to_string(aut.post(1, 1).antichain()) == "{ { 0}, { 1, 2}}");
	REQUIRE(std::to_string(aut.post(Node{1}, 1).antichain()) == "{ { 0}, { 1, 2}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{1}}, 1).antichain()) == "{ { 0}, { 1, 2}}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{1}}), 1).antichain()) == "{ { 0}, { 1, 2}}");

	REQUIRE(std::to_string(aut.post(2, 0).antichain()) == "{ { 0, 1}, { 2}}");
	REQUIRE(std::to_string(aut.post(Node{2}, 0).antichain()) == "{ { 0, 1}, { 2}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{2}}, 0).antichain()) == "{ { 0, 1}, { 2}}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{2}}), 0).antichain()) == "{ { 0, 1}, { 2}}");

	REQUIRE(std::to_string(aut.post(2, 1).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{2}, 1).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{2}}, 1).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(StateClosedSet
	(Mata::upward_closed_set, 0, 2, Nodes{Node{2}}), 1).antichain()) == "{}");

	REQUIRE(std::to_string(aut.post(Node{0, 1}, 0).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{0, 2}, 0).antichain()) == "{ { 0, 1}, { 0, 2}}");
	REQUIRE(std::to_string(aut.post(Node{1, 2}, 0).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{0, 1, 2}, 0).antichain()) == "{}");

	REQUIRE(std::to_string(aut.post(Node{0, 1}, 1).antichain()) == "{ { 0, 1}, { 1, 2}}");
	REQUIRE(std::to_string(aut.post(Node{0, 2}, 1).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{1, 2}, 1).antichain()) == "{}");
	REQUIRE(std::to_string(aut.post(Node{0, 1, 2}, 1).antichain()) == "{}");

	REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}, 1).antichain()) == "{ { 0}, { 1}}");
	REQUIRE(std::to_string(aut.post(Nodes{Node{0}, Node{1}}).antichain()) == "{ { 0}, { 1}}");

} // }}}

TEST_CASE("Mata::Afa inverse transition test")
{ // {{{

	Afa aut(3);

	aut.initialstates = {{0}};
	aut.finalstates = {2};
	aut.add_inverse_trans(0, 0, Nodes{Node{0}});
	aut.add_inverse_trans(0, 1, Nodes{Node{1}});
	aut.add_inverse_trans(1, 1, Nodes{Node{0}, Node{1, 2}});
	aut.add_inverse_trans(2, 0, Nodes{Node{2}, Node{0, 1}});

	REQUIRE(std::to_string(aut.pre(Node{}, 0).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(Node{}, 1).antichain()) == "{ {}}");

	REQUIRE(std::to_string(aut.pre(0, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(Node{0}, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{0}}, 0).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{0}}), 0).antichain()) == "{ { 0}}");

	REQUIRE(std::to_string(aut.pre(1, 0).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(Node{1}, 0).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{1}}, 0).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{1}}), 0).antichain()) == "{ {}}");

	REQUIRE(std::to_string(aut.pre(2, 0).antichain()) == "{ { 2}}");
	REQUIRE(std::to_string(aut.pre(Node{2}, 0).antichain()) == "{ { 2}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{2}}, 0).antichain()) == "{ { 2}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{2}}), 0).antichain()) == "{ { 2}}");


	REQUIRE(std::to_string(aut.pre(0, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.pre(Node{0}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{0}}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{0}}), 1).antichain()) == "{ { 1}}");

	REQUIRE(std::to_string(aut.pre(1, 1).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(Node{1}, 1).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{1}}, 1).antichain()) == "{ { 0}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{1}}), 1).antichain()) == "{ { 0}}");

	REQUIRE(std::to_string(aut.pre(2, 1).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(Node{2}, 1).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{2}}, 1).antichain()) == "{ {}}");
	REQUIRE(std::to_string(aut.pre(StateClosedSet
	(Mata::downward_closed_set, 0, 2, Nodes{Node{2}}), 1).antichain()) == "{ {}}");


	REQUIRE(std::to_string(aut.pre(Node{0, 1}, 0).antichain()) == "{ { 0, 2}}");
	REQUIRE(std::to_string(aut.pre(Node{0, 2}, 0).antichain()) == "{ { 0, 2}}");
	REQUIRE(std::to_string(aut.pre(Node{1, 2}, 0).antichain()) == "{ { 2}}");
	REQUIRE(std::to_string(aut.pre(Node{0, 1, 2}, 0).antichain()) == "{ { 0, 2}}");

	REQUIRE(std::to_string(aut.pre(Node{0, 1}, 1).antichain()) == "{ { 0, 1}}");
	REQUIRE(std::to_string(aut.pre(Node{0, 2}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.pre(Node{1, 2}, 1).antichain()) == "{ { 0, 1}}");
	REQUIRE(std::to_string(aut.pre(Node{0, 1, 2}, 1).antichain()) == "{ { 0, 1}}");

	REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}, 0).antichain()) == "{ { 0}, { 2}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}, 1).antichain()) == "{ { 1}}");
	REQUIRE(std::to_string(aut.pre(Nodes{Node{0}, Node{2}}).antichain()) == "{ { 0}, { 1}, { 2}}");

} // }}}

TEST_CASE("Mata::Afa antichain emptiness test")
{
	/////////////////////////////////
	// Example of an automaton
	/////////////////////////////////

	Afa aut(3);

	aut.initialstates = {{0}};
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

	aut1.initialstates = {{0}};
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

	aut2.initialstates = {{0}};

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

TEST_CASE("Mata::Afa::construct() from IntermediateAut correct calls")
{ // {{{
    Afa aut;
    Mata::IntermediateAut inter_aut;
    Mata::StringToSymbolMap symbol_map;

    SECTION("construct an empty automaton")
    {
        inter_aut.automaton_type = Mata::IntermediateAut::AFA;
        aut = Mata::Afa::construct(inter_aut);
        REQUIRE(true);
    }

    SECTION("construct a simple non-empty automaton accepting the empty word from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p | q\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        aut = construct(inter_aut);

        REQUIRE(aut.initialstates.size() == 2);
        REQUIRE(aut.finalstates.size() == 2);
    }

    SECTION("construct an automaton with more than one initial/final states from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial p | q\n"
                "%Final p & q & r\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initialstates.size() == 2);
        REQUIRE(aut.finalstates.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q r\n"
                "%Alphabet-auto\n"
                "%Initial p q\n"
                "%Final p q r\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initialstates.size() == 2);
        REQUIRE(aut.finalstates.size() == 3);
    }

    SECTION("construct an automaton with implicit operator completion one initial/final states from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q r m n\n"
                "%Alphabet-auto\n"
                "%Initial p q r\n"
                "%Final p q m n\n";
        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        construct(&aut, inter_aut);

        REQUIRE(aut.initialstates.size() == 3);
        REQUIRE(aut.finalstates.size() == 4);
    }

    SECTION("construct a simple non-empty automaton accepting only the word 'a' from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1\n"
                "%Final q2\n"
                "q1 a & q2\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];
        construct(&aut, inter_aut, &symbol_map);
    }

    SECTION("construct a more complicated non-empty automaton from intermediate automaton")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum p q 3\n"
                "%Alphabet-auto\n"
                "%Initial q1 | q3\n"
                "%Final q5\n"
                "q1 a & ((q2 & q3) | (q4 & q5))\n"
                "q1 a & q1 & q3\n"
                "q1 b & q3 & q4\n"
                "q2 a & ((q3 & q4) | (q4 & q5) | (q3 & q6))\n"
                "q3 a & ((q3 & q4) | (q4 & q5) | (q3 & q6 & q4) & q5)\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        StringToStateMap state_map;
        construct(&aut, inter_aut, &symbol_map, &state_map);

        REQUIRE(aut.trans_size() == 4);
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["a"]).dst.size() == 3);
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["a"]).dst.begin()->count(
                state_map["q1"]));
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["a"]).dst.begin()->count(
                state_map["q3"]));
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["b"]).dst.size() == 1);
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["b"]).dst.begin()->count(
                state_map["q3"]));
        REQUIRE(aut.get_trans_from_state(state_map["q1"], symbol_map["b"]).dst.begin()->count(
                state_map["q4"]));
        REQUIRE(aut.get_trans_from_state(state_map["q2"], symbol_map["a"]).dst.size() == 3);
        REQUIRE(aut.get_trans_from_state(state_map["q3"], symbol_map["a"]).dst.size() == 2);
    }

    SECTION("Initial formula in DNF")
    {
        std::string file =
                "@AFA-explicit\n"
                "%Initial qQC0_0 | (qQC0_1 & qQC1_1 & qQC1_0) \n"
                "%Final !qQC0_2 & !qQC0_1 & !qQC1_1 & !qQC1_0 & !qQC0_0\n"
                "qQC0_1 a & (qQC0_1 | qQC0_2)\n"
                "qQC1_1 a & qQC1_1\n"
                "qQC0_2 a\n"
                "qQC1_0 a & qQC1_1\n"
                "qQC0_0 a & (qQC0_2 | qQC0_1)\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        StringToStateMap state_map;
        construct(&aut, inter_aut, &symbol_map, &state_map);

        REQUIRE(aut.initialstates.size() == 2);
        auto it = aut.initialstates.begin();
        REQUIRE(it->count(state_map["QC0_1"]));
        REQUIRE(it->count(state_map["QC1_1"]));
        REQUIRE(it->count(state_map["QC1_0"]));
        ++it;
        REQUIRE(it->count(state_map["QC0_0"]));
    }

    SECTION("AFA final states from multiple negations")
    {
        std::string file =
                "@AFA-explicit\n"
                "%Initial q1\n"
                "%Final !q0 & !q1 & !q3\n"
                "q0 a1 & q1\n"
                "q1 a2 & q2\n"
                "q2 a1 & (q3 | q2)\n"
                "q2 a2 & (q4 & q1)\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        inter_aut = auts[0];

        StringToStateMap state_map;
        construct(&aut, inter_aut, &symbol_map, &state_map);

        CHECK(aut.finalstates.size() == 2);
		CHECK(aut.finalstates.count(state_map.at("2")));
		CHECK(aut.finalstates.count(state_map.at("4")));

    }

} // }}}

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

		// REQUIRE(aut.initial.size() == 2);
		// REQUIRE(aut.final.size() == 3);
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
