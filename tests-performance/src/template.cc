#include "mata/nfa/nfa.hh"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

int main(int argc, char *argv[]) {
    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    auto start = std::chrono::system_clock::now();

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     **************************************************/

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "time: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
