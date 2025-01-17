// example02.cc - determinizing an automaton

#include "mata/nfa/nfa.hh"

#include <iostream>
#include <fstream>

using namespace mata::nfa;

// Debugging
#define MAXBREAKPOINT 11
#define BOO
#define LONG_AUT
#define PRINTTODOT

int main() {
    #ifndef LONG_AUT
    Nfa aut(3);
    aut.initial = {1 };
    aut.final = {2 };
    aut.delta.add(1, 'a', 2);

    #else
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

    #endif




    std::unordered_map<StateSet, State> subset_map;
    Nfa determ = determinize(aut, &subset_map);

    determ.print_to_dot(std::cout);

}
