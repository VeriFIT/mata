// TODO: some header

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/nfa/strings.hh"
#include "mata/nfa/builder.hh"

using namespace mata::nfa;
using namespace mata::strings;
using namespace mata::utils;
using namespace mata::nfa::builder;

// Some common automata {{{

// Automaton A
#define FILL_WITH_AUT_A(x) \
	x.initialstates = {1, 3}; \
	x.finalstates = {5}; \
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
	x.initialstates = {4}; \
	x.finalstates = {2, 12}; \
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
