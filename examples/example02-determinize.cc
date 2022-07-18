// example02.cc - determinizing an automaton

#include <mata/nfa.hh>
#include <iostream>

using namespace Mata::Nfa;

int main()
{
	Nfa aut;
	Nfa determ;

	aut.initialstates = {1, 3};
	aut.finalstates = {5};
	aut.add_trans(1, 'a', 3);
	aut.add_trans(1, 'a', 10);
	aut.add_trans(1, 'b', 7);
	aut.add_trans(3, 'a', 7);
	aut.add_trans(3, 'b', 9);
	aut.add_trans(9, 'a', 9);
	aut.add_trans(7, 'b', 1);
	aut.add_trans(7, 'a', 3);
	aut.add_trans(7, 'c', 3);
	aut.add_trans(10, 'a', 7);
	aut.add_trans(10, 'b', 7);
	aut.add_trans(10, 'c', 7);
	aut.add_trans(7, 'a', 5);
	aut.add_trans(5, 'a', 5);
	aut.add_trans(5, 'c', 9);

	determinize(&determ, aut);

	std::cout << std::to_string(determ);
}
