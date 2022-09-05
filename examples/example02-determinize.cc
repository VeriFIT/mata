// example02.cc - determinizing an automaton

#include <mata/nfa.hh>
#include <iostream>

using namespace Mata::Nfa;

int main()
{
	Nfa aut(10);
	Nfa determ;

	aut.initialstates = {0, 2};
	aut.finalstates = {4};
	aut.add_trans(0, 'a', 2);
	aut.add_trans(0, 'a', 9);
	aut.add_trans(0, 'b', 6);
	aut.add_trans(2, 'a', 6);
	aut.add_trans(2, 'b', 8);
	aut.add_trans(8, 'a', 8);
	aut.add_trans(6, 'b', 0);
	aut.add_trans(6, 'a', 2);
	aut.add_trans(6, 'c', 2);
	aut.add_trans(9, 'a', 6);
	aut.add_trans(9, 'b', 6);
	aut.add_trans(9, 'c', 6);
	aut.add_trans(6, 'a', 4);
	aut.add_trans(4, 'a', 4);
	aut.add_trans(4, 'c', 8);

	determinize(&determ, aut);

	determ.print_to_DOT(std::cout);
}
