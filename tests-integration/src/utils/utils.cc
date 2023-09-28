#include <string>
#include "utils.hh"
#include "mata/nfa/types.hh"
#include "mata/parser/inter-aut.hh"

int load_automaton(
        const std::string& filename,
        Nfa& aut,
        mata::OnTheFlyAlphabet& alphabet,
        const bool mintermize_automata
) {
    std::vector<mata::IntermediateAut> inter_auts;
    TIME_BEGIN(parsing);
    if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
        std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
        return EXIT_FAILURE;
    }
    TIME_END(parsing);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            aut = mata::nfa::builder::construct(inter_auts[0], &alphabet);
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization);
            mata::IntermediateAut mintermized = mintermization.mintermize(inter_auts[0]);
            TIME_END(mintermization);
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
    TIME_BEGIN(parsing);
    for (const std::string& filename : filenames) {
        if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
            std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
            return EXIT_FAILURE;
        }
    }
    TIME_END(parsing);
    try {
        if (!mintermize_automata or inter_auts[0].alphabet_type != mata::IntermediateAut::AlphabetType::BITVECTOR) {
            // This is not foolproof and assumes, that everything is BITVECTOR
            for (mata::IntermediateAut& inter_aut : inter_auts) {
                assert(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR);
                auts.push_back(mata::nfa::builder::construct(inter_aut, &alphabet));
            }
        } else {
            mata::Mintermization mintermization;
            TIME_BEGIN(mintermization);
            std::vector<mata::IntermediateAut> mintermized = mintermization.mintermize(inter_auts);
            TIME_END(mintermization);
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
