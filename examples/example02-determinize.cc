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

    // Boost part of the example
    std::cout << "Boost determinization: \n\n\n\n\n\n\n";
    std::unordered_map<BoostSet, State> subset_map_boost;
    Nfa determ_boost = determinize_boost(aut, &subset_map_boost);
    std::cout << "\n\n\n\n\n\n\n\n";

    #ifdef PRINTTODOT
    std::cout << "Normal in dot format: ";
    determ.print_to_dot(std::cout);

    std::cout << "Boost in dot format: ";
    determ_boost.print_to_dot(std::cout);
    #endif

    determ.show_initial();
    determ.show_final();

    determ_boost.show_initial();
    determ_boost.show_final();

    std::cout << "For normal, 2 maps to " << subset_map[{2}] << std::endl;
    BoostSet bs{2, false, true};
    bs.resize(aut.delta.num_of_states() + 1);
    std::cout << "For boost, 2 maps to " << subset_map_boost[bs] << std::endl;
    bs.print();
    std::cout << std::endl;

}
