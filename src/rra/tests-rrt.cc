// TODO: some header

#include "../3rdparty/catch.hpp"

#include <mata/rrt.hh>
using namespace Mata::Rrt;
using namespace Mata::util;
using namespace Mata::Parser;

using GuardType = Mata::Rrt::Trans::Guard::GuardType;
using UpdateType = Mata::Rrt::Trans::Update::UpdateType;
using OutputType = Mata::Rrt::Trans::Output::OutputType;

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


TEST_CASE("Mata::Rrt::Rrt::add_trans()/has_trans()")
{ // {{{
	Rrt rrt;

	Trans trans1;
	trans1.src = 1;
	trans1.tgt = 2;
	trans1.lbl.guards = {{GuardType::IN1_VAR, 0},
		                   {GuardType::IN2_VAR, 0}};
  trans1.lbl.updates = {{UpdateType::REG_STORE_IN1, 0},
                        {UpdateType::REG_STORE_IN1, 1}};
  trans1.lbl.out1 = {OutputType::PUT_REG, 0};
  trans1.lbl.out2 = {OutputType::PUT_IN2, 0};    // the second argument is ignored

  REQUIRE(!rrt.has_trans(trans1));

	rrt.add_trans(trans1);

  // check for the transition
	Trans trans2;
	trans2.src = 1;
	trans2.lbl.guards = {{GuardType::IN1_VAR, 0},
		                   {GuardType::IN2_VAR, 0}};
  trans2.lbl.updates = {{UpdateType::REG_STORE_IN1, 0},
                        {UpdateType::REG_STORE_IN1, 1}};
  trans2.lbl.out1 = {OutputType::PUT_REG, 0};
  trans2.lbl.out2 = {OutputType::PUT_IN2, 5};    // the second argument is ignored

	trans2.tgt = 2;

  REQUIRE(rrt.has_trans(trans2));

  WARN_PRINT("Insufficient testing of Mata::Rrt::Rrt::add_trans()");
} // }}}

TEST_CASE("Mata::Rrt::Rrt::make_initial()/has_initial()")
{ // {{{
	Rrt rrt;

	REQUIRE(rrt.initialstates.empty());
  rrt.initialstates = {1,2,3};
  REQUIRE(rrt.has_initial(3));
  REQUIRE(!rrt.has_initial(4));
} // }}}

TEST_CASE("Mata::Rrt::Rrt::make_final()/has_final()")
{ // {{{
	Rrt rrt;

	REQUIRE(rrt.finalstates.empty());
  rrt.finalstates = {1,2,3};
  REQUIRE(rrt.has_final(3));
  REQUIRE(!rrt.has_final(4));
} // }}}

// TEST_CASE("Mata::Rrt::serialize() and operator<<()")
// { // {{{
	// Rrt rrt;
//
	// Trans trans;
	// trans.src = 1;
	// trans.lbl.guards = {{GuardType::IN1_VAR, 0},
											// {GuardType::IN2_VAR, 0}};
//
	// trans.tgt = 2;
//
	// rrt.add_trans(trans);
//
  // assert(false);
//
// } // }}}
