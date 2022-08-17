// example03 - testing language emptiness

#include <mata/nfa.hh>
#include <iostream>

using namespace Mata::Nfa;

int main()
{
	Nfa aut(10);

	std::cout << "Language empty: " << is_lang_empty(aut) << "\n";

	aut.initialstates = {0, 2};
	aut.finalstates = {4};
	aut.add_trans(0, 0, 2);
	aut.add_trans(0, 0, 9);
	aut.add_trans(0, 1, 6);
	aut.add_trans(2, 0, 6);
	aut.add_trans(2, 1, 8);
	aut.add_trans(8, 0, 8);
	aut.add_trans(6, 1, 0);
	aut.add_trans(6, 0, 2);
	aut.add_trans(6, 2, 2);
	aut.add_trans(9, 0, 6);
	aut.add_trans(9, 1, 6);
	aut.add_trans(9, 2, 6);
	aut.add_trans(6, 0, 4);
	aut.add_trans(4, 0, 4);
	aut.add_trans(4, 2, 8);

	std::cout << "Language empty: " << is_lang_empty(aut) << "\n";
}
