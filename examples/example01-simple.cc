// example1.cc - constructing an automaton, then dumping it

#include <vata2/nfa.hh>
#include <iostream>

using namespace Vata2::Nfa;

int main()
{
	Nfa aut;

	aut.initialstates = {1,2};
	aut.finalstates = {3,4};
	aut.add_trans(1, 'a', 3);
	aut.add_trans(2, 'b', 4);

	std::cout << serialize_vtf(&aut);
}
