#include "utils/utils.hh"

#include "mata/nfa/nfa.hh"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace mata::nfa;

int main() {
    Nfa b{10};
    b.initial.insert(0);
    b.final.insert({2, 4, 8, 7});
    b.delta.add(0, 'b', 1);
    b.delta.add(0, 'a', 2);
    b.delta.add(2, 'a', 4);
    b.delta.add(2, EPSILON, 3);
    b.delta.add(3, 'b', 4);
    b.delta.add(0, 'c', 5);
    b.delta.add(5, 'a', 8);
    b.delta.add(5, EPSILON, 6);
    b.delta.add(6, 'a', 9);
    b.delta.add(6, 'b', 7);

    Nfa c;
    TIME_BLOCK(reduce,
        for (size_t i{ 0 }; i < 1'000; ++i) {
            c = reduce(b);
        }
    );
    if (!are_equivalent(b, c)) { // Check correctness.
        return EXIT_FAILURE;
    }
}
