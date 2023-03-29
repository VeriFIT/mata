#include <mata/inter-aut.hh>
#include <mata/nfa.hh>
#include <mata/mintermization.hh>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>

using namespace Mata::Nfa;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cerr << "Input file missing\n";
		return EXIT_FAILURE;
	}

	std::string filename = argv[1];

	std::fstream fs(filename, std::ios::in);
	if (!fs) {
		std::cerr << "Could not open file \'" << filename << "'\n";
		return EXIT_FAILURE;
	}

	Mata::Parser::Parsed parsed;
	Nfa aut;
    Mata::StringToSymbolMap stsm;
	const char* nfa_str = "NFA";
	try {
		parsed = Mata::Parser::parse_mf(fs, true);
		fs.close();

		if (parsed.size() != 1) {
			throw std::runtime_error(
				"The number of sections in the input file is not 1\n");
		}
		if (strncmp(parsed[0].type.c_str(), nfa_str, strlen(nfa_str)) != 0) {
			throw std::runtime_error("The type of input automaton is not NFA\n");
		}

        std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);
        Mata::Mintermization mintermization;
        auto mintermized = mintermization.mintermize(inter_auts);
        assert(mintermized.size() == 1);
		aut = construct(mintermized[0], &stsm);
	}
	catch (const std::exception& ex) {
		fs.close();
		std::cerr << "libMATA error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

    Mata::OnTheFlyAlphabet alph{ stsm };
    auto start = std::chrono::system_clock::now();

    /**************************************************
     *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
     **************************************************/

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Elapsed time: " << elapsed.count() << " ms\n";

	return EXIT_SUCCESS;
}