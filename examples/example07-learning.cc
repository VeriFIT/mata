#include "mata/nfa/learning.hh"
#include "mata/nfa/nfa.hh"

int main(){
    Nfa aut;
    aut.initial = {0};
    aut.final = {3};
    aut.delta.add(0, 'a', 0);
    aut.delta.add(0, 'b', 0);
    aut.delta.add(0, 'a', 1);
    aut.delta.add(1, 'a', 1);
    aut.delta.add(1, 'a', 0);
    aut.delta.add(1, 'b', 0);
    aut.delta.add(1, 'a', 2);
    aut.delta.add(1, 'b', 2);
    aut.delta.add(2, 'a', 1);
    aut.delta.add(2, 'a', 0);
    aut.delta.add(2, 'b', 0);
    aut.delta.add(2, 'a', 3);
    aut.delta.add(2, 'b', 3);
    aut.delta.add(3, 'a', 1);
    aut.delta.add(3, 'a', 0);
    aut.delta.add(3, 'b', 0);

    ParameterMap params;
    params["algorithm"] = "nlstar";
    Nfa res1 = learn(aut, params);
    res1.print_to_dot(std::cout);

    params["algorithm"] = "lstar";
    Nfa res2 = learn(aut, params);
    res2.print_to_dot(std::cout);
    
}