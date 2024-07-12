// example1.cc - constructing an automaton, then dumping it

#include "mata/nfa/nfa.hh"

#include <iostream>

using namespace mata::nfa;

int main() {
    Nfa aut(4);

    aut.initial = { 0,1 };
    aut.final = { 2,3 };
    aut.delta.add(0, 0, 2);
    aut.delta.add(1, 1, 3);

    aut.print_to_dot(std::cout);
}
