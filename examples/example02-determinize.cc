// example02.cc - determinizing an automaton

#include "mata/nfa/nfa.hh"

#include <iostream>

using namespace mata::nfa;

int main() {
    Nfa aut(10);
    aut.initial = { 0, 2 };
    aut.final = { 4 };
    aut.delta.add(0, 'a', 2);
    aut.delta.add(0, 'a', 9);
    aut.delta.add(0, 'b', 6);
    aut.delta.add(2, 'a', 6);
    aut.delta.add(2, 'b', 8);
    aut.delta.add(8, 'a', 8);
    aut.delta.add(6, 'b', 0);
    aut.delta.add(6, 'a', 2);
    aut.delta.add(6, 'c', 2);
    aut.delta.add(9, 'a', 6);
    aut.delta.add(9, 'b', 6);
    aut.delta.add(9, 'c', 6);
    aut.delta.add(6, 'a', 4);
    aut.delta.add(4, 'a', 4);
    aut.delta.add(4, 'c', 8);

    Nfa determ{ determinize(aut) };
    determ.print_to_dot(std::cout);
}