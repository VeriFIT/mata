// example1.cc - constructing an automaton, then dumping it

#include <mata/nfa.hh>
#include <iostream>

using namespace Mata::Nfa;

int main()
{
	Nfa aut;

	aut.initialstates = {1,2};
	aut.finalstates = {3,4};
	aut.add_trans(1, 'a', 3);
	aut.add_trans(2, 'b', 4);

	std::cout << std::to_string(aut);
}
