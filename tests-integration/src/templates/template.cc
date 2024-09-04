#include "utils/utils.hh"
#include "mata/nfa/nfa.hh"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

using namespace mata::nfa;

int main() {
    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    TIME_BEGIN(tmp);

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     **************************************************/

    TIME_END(tmp);

    return EXIT_SUCCESS;
}
