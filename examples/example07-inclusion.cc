// example07.cc - language inclusion check

#include "mata/nfa/nfa.hh"

#include <iostream>
#include <fstream>

using namespace mata::nfa;

unsigned long example_cnt = 0;

// #define EXAMPLE(language) std::cout << "----------Example " << ++example_cnt << ": " << language << "----------" << std::endl;
// #define RUN_EXAMPLE(smaller, bigger) do{\
//     std::cout << "Ord vectors: " << is_included(smaller, bigger, nullptr, nullptr) << std::endl;\
//     std::cout << "Boost bit vectors: " << is_included(smaller, bigger, nullptr) << std::endl;\
//     std::cout << "Opposite" << std::endl;\
//     std::cout << "Ord vectors: " << is_included(bigger, smaller, nullptr, nullptr) << std::endl;\
//     std::cout << "Boost bit vectors: " << is_included(bigger, smaller, nullptr) << std::endl;\
//     std::cout << "\n\n";\
// } while(0);

int main() {
//     Nfa smaller(10);
//     Nfa bigger(16);

//     EXAMPLE("{} <= {}")
//     RUN_EXAMPLE(smaller, bigger)

//     Nfa smaller2(10);
//     Nfa bigger2(16);

//     EXAMPLE("{} <= {epsilon}")
//     bigger2.initial = {1};
//     bigger2.final = {1};
//     RUN_EXAMPLE(smaller2, bigger2)

//     Nfa smaller3(10);
//     Nfa bigger3(16);

//     EXAMPLE("{epsilon} <= {epsilon}")
//     smaller3.initial = {1};
//     smaller3.final = {1};
//     bigger3.initial = {11};
//     bigger3.final = {11};
//     RUN_EXAMPLE(smaller3, bigger3)

//     Nfa smaller4(10);
//     Nfa bigger4(16);

//     EXAMPLE("(a+b)* !<= a* + b*")
//     mata::OnTheFlyAlphabet alph{ std::vector<std::string>{ "a", "b"} };
//     smaller4.initial = {1};
//     smaller4.final = {1};
//     smaller4.delta.add(1, alph["a"], 1);
//     smaller4.delta.add(1, alph["b"], 1);
//     bigger4.initial = {11, 12};
//     bigger4.final = {11, 12};
//     bigger4.delta.add(11, alph["a"], 11);
//     bigger4.delta.add(12, alph["b"], 12);
//     RUN_EXAMPLE(smaller4, bigger4)

//     Nfa smaller5(10);
//     Nfa bigger5(16);

//     EXAMPLE("(a+b)* !<= eps + (a+b) + (a+b)(a+b)(a* + b*)")
//     mata::OnTheFlyAlphabet alph2{ std::vector<std::string>{ "a", "b"} };
//     smaller5.initial = {1};
//     smaller5.final = {1};
//     smaller5.delta.add(1, alph2["a"], 1);
//     smaller5.delta.add(1, alph2["b"], 1);
//     bigger5.initial = {11};
//     bigger5.final = {11, 12, 13, 14, 15};
//     bigger5.delta.add(11, alph2["a"], 12);
//     bigger5.delta.add(11, alph2["b"], 12);
//     bigger5.delta.add(12, alph2["a"], 13);
//     bigger5.delta.add(12, alph2["b"], 13);
//     bigger5.delta.add(13, alph2["a"], 14);
//     bigger5.delta.add(14, alph2["a"], 14);
//     bigger5.delta.add(13, alph2["b"], 15);
//     bigger5.delta.add(15, alph2["b"], 15);
//     RUN_EXAMPLE(smaller5, bigger5)
}