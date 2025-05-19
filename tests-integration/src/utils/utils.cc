#include <string>
#include "utils.hh"
#include "mata/nfa/types.hh"
#include "mata/parser/inter-aut.hh"

namespace mata::nfa {

int load_automaton(
        const std::string& filename,
        Nfa& aut,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata
) {
    std::vector<mata::IntermediateAut> inter_auts;
    TIME_BEGIN(parsing_nfa);
    if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
        std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
        return EXIT_FAILURE;
    }
    TIME_END(parsing_nfa);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            aut = mata::nfa::builder::construct(inter_auts[0], &alphabet);
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization_nfa);
            mata::IntermediateAut mintermized = mintermization.mintermize(inter_auts[0]);
            TIME_END(mintermization_nfa);
            aut = mata::nfa::builder::construct(mintermized, &alphabet);
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int load_automata(
        std::vector<std::string>& filenames,
        std::vector<Nfa>& auts,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata
) {
    std::vector<mata::IntermediateAut> inter_auts;
    TIME_BEGIN(parsing_nfa);
    for (const std::string& filename : filenames) {
        if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
            std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
            return EXIT_FAILURE;
        }
    }
    TIME_END(parsing_nfa);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            // This is not foolproof and assumes, that everything is BITVECTOR
            for (mata::IntermediateAut& inter_aut : inter_auts) {
                assert(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR);
                auts.push_back(mata::nfa::builder::construct(inter_aut, &alphabet));
            }
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization_nfa);
            std::vector<mata::IntermediateAut> mintermized = mintermization.mintermize(inter_auts);
            TIME_END(mintermization_nfa);
            for (mata::IntermediateAut& inter_aut : mintermized) {
                assert(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR);
                auts.push_back(mata::nfa::builder::construct(inter_aut, &alphabet));
            }
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int load_intermediate_automaton(
        const std::string& filename,
        std::vector<mata::IntermediateAut>& out_inter_auts
) {
    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    mata::parser::Parsed parsed;
    try {
        parsed = mata::parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error(
                    "The number of sections in the input file is not 1\n");
        }

        if (!parsed[0].type.starts_with(TYPE_NFA)) {
            std::cout << (parsed[0].type) << std::endl;
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
        out_inter_auts.push_back(inter_auts[0]);
    } catch (const std::exception& ex) {
        fs.close();
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

} // namespace mata::nfa

namespace mata::cntnfa {

int load_counter_automaton(
        const std::string& filename,
        Cntnfa& aut,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata
) {
    std::vector<mata::IntermediateAut> inter_auts;
    TIME_BEGIN(parsing_cntnfa);
    if (load_intermediate_counter_automaton(filename, inter_auts) != EXIT_SUCCESS) {
        std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
        return EXIT_FAILURE;
    }
    TIME_END(parsing_cntnfa);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            aut = mata::cntnfa::builder::construct(inter_auts[0], &alphabet);
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization_cntnfa);
            mata::IntermediateAut mintermized = mintermization.mintermize(inter_auts[0]);
            TIME_END(mintermization_cntnfa);
            aut = mata::cntnfa::builder::construct(mintermized, &alphabet);
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int load_counter_automata(
        std::vector<std::string>& filenames,
        std::vector<Cntnfa>& auts,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata
) {
    std::vector<mata::IntermediateAut> inter_auts;
    TIME_BEGIN(parsing_cntnfa);
    for (const std::string& filename : filenames) {
        if (load_intermediate_counter_automaton(filename, inter_auts) != EXIT_SUCCESS) {
            std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
            return EXIT_FAILURE;
        }
    }
    TIME_END(parsing_cntnfa);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            // This is not foolproof and assumes, that everything is BITVECTOR
            for (mata::IntermediateAut& inter_aut : inter_auts) {
                assert(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR);
                auts.push_back(mata::cntnfa::builder::construct(inter_aut, &alphabet));
            }
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization_cntnfa);
            std::vector<mata::IntermediateAut> mintermized = mintermization.mintermize(inter_auts);
            TIME_END(mintermization_cntnfa);
            for (mata::IntermediateAut& inter_aut : mintermized) {
                assert(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR);
                auts.push_back(mata::cntnfa::builder::construct(inter_aut, &alphabet));
            }
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int load_intermediate_counter_automaton(
        const std::string& filename,
        std::vector<mata::IntermediateAut>& out_inter_auts
) {
    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    mata::parser::Parsed parsed;
    try {
        parsed = mata::parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error(
                    "The number of sections in the input file is not 1\n");
        }

        // Note: We need to use the NFA type here, becuase the parser does not support the CNTNFA type.
        if (!(parsed[0].type.starts_with(mata::nfa::TYPE_NFA) || parsed[0].type.starts_with(TYPE_CNTNFA))) {
            std::cout << (parsed[0].type) << std::endl;
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
        out_inter_auts.push_back(inter_auts[0]);
    } catch (const std::exception& ex) {
        fs.close();
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int load_cntnfa_with_counters(
        const std::string& filename,
        Cntnfa& aut,
        mata::OnTheFlyAlphabet& alphabet
) {
    // Open the file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open CNTNFA file: " << filename << "\n";
        return EXIT_FAILURE;
    }

    try {
        // Parse the CNTNFA section
        mata::parser::ParsedSection section = mata::parser::parse_mf_section(file);
        file.close();
        // Construct the CNTNFA
        aut = builder::construct_counter_nfa(section, &alphabet);
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load CNTNFA: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

} // namespace mata::cntnfa
