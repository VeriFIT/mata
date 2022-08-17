// example1.cc - constructing an automaton, then dumping it

#include <mata/nfa.hh>
#include <iostream>

using namespace Mata::Nfa;

int main()
{
	Nfa aut(4);

	aut.initialstates = {0,1};
	aut.finalstates = {2,3};
	aut.add_trans(0, 0, 2);
	aut.add_trans(1, 1, 3);

	aut.print_to_DOT(std::cout);
}
