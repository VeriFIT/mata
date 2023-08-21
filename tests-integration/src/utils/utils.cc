#include "utils.hh"

int load_automaton(
        const std::string& filename,
        Nfa& aut,
        Mata::OnTheFlyAlphabet& alphabet,
        const bool skip_mintermization,
        const std::string& aut_name
) {
    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    Mata::Parser::Parsed parsed;
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

        bool is_not_nfa_bits = parsed[0].type.compare(parsed[0].type.length() - bits_str.length(), bits_str.length(), bits_str) != 0;
        if (skip_mintermization or is_not_nfa_bits) {
            aut = Mata::Nfa::Builder::construct(inter_auts[0], &alphabet);
        } else {
            Mata::Mintermization mintermization;
            auto minterm_start = std::chrono::system_clock::now();
            auto mintermized = mintermization.mintermize(inter_auts);
            auto minterm_end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed = minterm_end - minterm_start;
            assert(mintermized.size() == 1);
            aut = Mata::Nfa::Builder::construct(mintermized[0], &alphabet);
            std::cout << "mintermization-" << aut_name<< ":" << elapsed.count() << "\n";
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        fs.close();
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
