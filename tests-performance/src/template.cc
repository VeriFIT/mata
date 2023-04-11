#include <mata/nfa.hh>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

int main(int argc, char *argv[])
{
    auto start = std::chrono::system_clock::now();

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     **************************************************/

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "time: " << elapsed.count() << "\n";

    return EXIT_SUCCESS;
}
