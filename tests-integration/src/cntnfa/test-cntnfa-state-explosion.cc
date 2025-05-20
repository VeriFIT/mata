// TODO: some header

#include "../utils/utils.hh"
#include "mata/cntnfa/cntnfa.hh"
#include "mata/nfa/nfa.hh"

constexpr bool MINTERMIZE_AUTOMATA = false;

// Helper to create word from the string
mata::Word make_word(const std::string& input, mata::OnTheFlyAlphabet& alphabet) {
    mata::Word word;
    for (char c : input) {
        word.push_back(alphabet.translate_symb(std::string(1, c)));
    }
    return word;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::string nfa_filename = argv[1];
    std::string cntnfa_filename = argv[2];

    mata::OnTheFlyAlphabet alphabet;

    // Setting precision of the times to fixed points and 4 decimal places
    std::cout << std::fixed << std::setprecision(4);

    // Load NFA
    Nfa nfa;
    if (load_automaton(nfa_filename, nfa, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Load CNTNFA
    Cntnfa cntnfa;
    TIME_BEGIN(parsing_cntnfa);
    if (load_cntnfa_with_counters(cntnfa_filename, cntnfa, alphabet) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    TIME_END(parsing_cntnfa);

    const int length = 10000;
    std::string input(length, '0');
    mata::Word word = make_word(input, alphabet);

    // Membership test
    TIME_BEGIN(accepted_by_nfa);
    bool nfa_result = nfa.is_in_lang(word);
    (void)nfa_result;
    TIME_END(accepted_by_nfa);

    TIME_BEGIN(accepted_by_cntnfa);
    bool cntnfa_result = cntnfa.is_in_lang_of_counter_nfa(word);
    (void)cntnfa_result;
    TIME_END(accepted_by_cntnfa);

    // Intersection
    TIME_BEGIN(intersection_nfa);
    Nfa intersect_nfa;
    intersect_nfa = mata::nfa::intersection(nfa, nfa);
    TIME_END(intersection_nfa);

    TIME_BEGIN(intersection_cntnfa);
    Cntnfa intersect_cntnfa;
    intersect_cntnfa = mata::cntnfa::intersection_counter_nfas(cntnfa, cntnfa);
    TIME_END(intersection_cntnfa);

    // Union
    TIME_BEGIN(union_nfa);
    Nfa uni_nfa;
    uni_nfa = mata::nfa::union_nondet(nfa, nfa);
    TIME_END(union_nfa);

    TIME_BEGIN(union_cntnfa);
    Cntnfa uni_cntnfa;
    uni_cntnfa = mata::cntnfa::union_nondet_counter_nfas(cntnfa, cntnfa);
    TIME_END(union_cntnfa);

    return EXIT_SUCCESS;
}
