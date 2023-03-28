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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Elapsed time: " << elapsed.count() << " ms\n";

    return EXIT_SUCCESS;
}
