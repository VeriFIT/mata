// example03 - testing language emptiness

#include "mata/nfa/nfa.hh"

#include <iostream>

using namespace Mata::Nfa;

int main() {
    Nfa aut(10);

    std::cout << "Language empty: " << is_lang_empty(aut) << "\n";

    aut.initial = {0, 2};
    aut.final = {4};
    aut.delta.add(0, 0, 2);
    aut.delta.add(0, 0, 9);
    aut.delta.add(0, 1, 6);
    aut.delta.add(2, 0, 6);
    aut.delta.add(2, 1, 8);
    aut.delta.add(8, 0, 8);
    aut.delta.add(6, 1, 0);
    aut.delta.add(6, 0, 2);
    aut.delta.add(6, 2, 2);
    aut.delta.add(9, 0, 6);
    aut.delta.add(9, 1, 6);
    aut.delta.add(9, 2, 6);
    aut.delta.add(6, 0, 4);
    aut.delta.add(4, 0, 4);
    aut.delta.add(4, 2, 8);

    std::cout << "Language empty: " << is_lang_empty(aut) << "\n";
}
