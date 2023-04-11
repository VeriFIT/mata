/**
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `SKIP_MINTERMIZATION` below to `false`
 */
#include <mata/inter-aut.hh>
#include <mata/nfa.hh>
#include <mata/mintermization.hh>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstring>
#include <vector>

using namespace Mata::Nfa;

const bool SKIP_MINTERMIZATION = false;

int main(int argc, char *argv[])
{
    /**
     * NOTE: Comment out automata, that you do not want to process or add your own automata.
     */
    std::vector<std::string> automata = {
            "../automata/b-armc-incl-easiest/aut1.mata",
            "../automata/b-armc-incl-easiest/aut2.mata",
            "../automata/b-armc-incl-easy/aut1.mata",
            "../automata/b-armc-incl-easy/aut2.mata",
            "../automata/b-armc-incl-hard/aut1.mata",
            "../automata/b-armc-incl-hard/aut2.mata",
            "../automata/b-armc-incl-medium/aut1.mata",
            "../automata/b-armc-incl-medium/aut2.mata",
            "../automata/b-armc-incl-medium-hard/aut1.mata",
            "../automata/b-armc-incl-medium-hard/aut2.mata",
            "../automata/b-hand-made-easiest/aut1.mata",
            "../automata/b-hand-made-easiest/aut2.mata",
            "../automata/b-hand-made-easy/aut1.mata",
            "../automata/b-param-easiest/aut0.mata",
            "../automata/b-param-easiest/aut1.mata",
            "../automata/b-param-easy/aut0.mata",
            "../automata/b-param-easy/aut1.mata",
            "../automata/b-param-harder/aut0.mata",
            "../automata/b-param-harder/aut1.mata",
            "../automata/b-param-harder/aut10.mata",
            "../automata/b-param-harder/aut11.mata",
            "../automata/b-param-harder/aut12.mata",
            "../automata/b-param-harder/aut13.mata",
            "../automata/b-param-harder/aut14.mata",
            "../automata/b-param-harder/aut15.mata",
            "../automata/b-param-harder/aut16.mata",
            "../automata/b-param-harder/aut17.mata",
            "../automata/b-param-harder/aut2.mata",
            "../automata/b-param-harder/aut3.mata",
            "../automata/b-param-harder/aut4.mata",
            "../automata/b-param-harder/aut5.mata",
            "../automata/b-param-harder/aut6.mata",
            "../automata/b-param-harder/aut7.mata",
            "../automata/b-param-harder/aut8.mata",
            "../automata/b-param-harder/aut9.mata",
            "../automata/b-param-hardest/aut1.mata",
            "../automata/b-param-hardest/aut2.mata",
            "../automata/b-param-medium/aut0.mata",
            "../automata/b-param-medium/aut1.mata",
            "../automata/b-param-medium/aut10.mata",
            "../automata/b-param-medium/aut11.mata",
            "../automata/b-param-medium/aut12.mata",
            "../automata/b-param-medium/aut13.mata",
            "../automata/b-param-medium/aut14.mata",
            "../automata/b-param-medium/aut15.mata",
            "../automata/b-param-medium/aut16.mata",
            "../automata/b-param-medium/aut2.mata",
            "../automata/b-param-medium/aut3.mata",
            "../automata/b-param-medium/aut4.mata",
            "../automata/b-param-medium/aut5.mata",
            "../automata/b-param-medium/aut6.mata",
            "../automata/b-param-medium/aut7.mata",
            "../automata/b-param-medium/aut8.mata",
            "../automata/b-param-medium/aut9.mata",
            "../automata/b-param-medium-hard/aut0.mata",
            "../automata/b-param-medium-hard/aut1.mata",
            "../automata/b-param-medium-hard/aut2.mata",
            "../automata/b-param-medium-hard/aut3.mata",
            "../automata/b-regex-easiest/aut21.mata",
            "../automata/b-regex-easiest/aut26.mata",
            "../automata/b-regex-easiest/aut27.mata",
            "../automata/b-regex-easiest/aut35.mata",
            "../automata/b-regex-easiest/aut51.mata",
            "../automata/b-regex-easy/aut26.mata",
            "../automata/b-regex-easy/aut40.mata",
            "../automata/b-regex-easy/aut61.mata",
            "../automata/b-regex-easy/aut69.mata",
            "../automata/b-regex-easy/aut7.mata",
            "../automata/b-smt-easiest/aut1.mata",
            "../automata/b-smt-easiest/aut2.mata",
    };

    std::vector<std::string> source_automata;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::string filename = argv[i];
            std::fstream fs(filename, std::ios::in);
            if (!fs) {
                std::cerr << "Could not open file \'" << filename << "'\n";
                return EXIT_FAILURE;
            }

            std::string line;
            while (std::getline(fs, line)) {
                source_automata.push_back(line);
            }
        }
    } else {
        source_automata = automata;
    }

    for (const auto& aut_file : source_automata) {
        std::fstream fs(aut_file, std::ios::in);
        if (!fs) {
            std::cerr << "Could not open file \'" << aut_file << "'\n";
            return EXIT_FAILURE;
        }

        Mata::Parser::Parsed parsed;
        Nfa aut;
        Mata::StringToSymbolMap stsm;
        const std::string nfa_str = "NFA";
        const std::string bits_str = "-bits";
        try {
            parsed = Mata::Parser::parse_mf(fs, true);
            fs.close();

            if (parsed.size() != 1) {
                throw std::runtime_error(
                        "The number of sections in the input file is not 1\n");
            }
            if (parsed[0].type.compare(0, nfa_str.length(), nfa_str) != 0) {
                throw std::runtime_error("The type of input automaton is not NFA\n");
            }

            std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);

            if (SKIP_MINTERMIZATION or parsed[0].type.compare(parsed[0].type.length() - bits_str.length(), bits_str.length(), bits_str) != 0) {
                aut = construct(inter_auts[0], &stsm);
            } else {
                Mata::Mintermization mintermization;
                auto minterm_start = std::chrono::system_clock::now();
                auto mintermized = mintermization.mintermize(inter_auts);
                auto minterm_end = std::chrono::system_clock::now();
                std::chrono::duration<double> elapsed = minterm_end - minterm_start;
                assert(mintermized.size() == 1);
                aut = construct(mintermized[0], &stsm);
                std::cout << "mintermization:" << elapsed.count() << "\n";
            }
        }
        catch (const std::exception &ex) {
            fs.close();
            std::cerr << "error: " << ex.what() << "\n";
            return EXIT_FAILURE;
        }

        std::cout << "Processing " << aut_file << std::endl;

        Mata::OnTheFlyAlphabet alph{stsm};
        auto start = std::chrono::system_clock::now();

        /**************************************************
         *  HERE COMES YOUR CODE THAT YOU WANT TO PROFILE *
         *   - Use alphabet alph as source alphabet       *
         *   - Use Nfa aut as source automaton            *
         *   - e.g. complement(aut, alph);                *
         **************************************************/

        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "time: " << elapsed.count() << "\n";
    }

    return EXIT_SUCCESS;
}
