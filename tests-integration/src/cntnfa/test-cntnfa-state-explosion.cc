// TODO: some header

#include "../utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA = false;

// Helper to create word from the string
mata::Word make_word(const std::string& input, mata::OnTheFlyAlphabet& alphabet) {
    mata::Word word;
    for (char c : input) {
        std::string symb(1, c);
        word.push_back(alphabet.translate_symb(symb));
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
    Nfa nfa_automaton;
    if (load_automaton(nfa_filename, nfa_automaton, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Load CNTNFA
    Cntnfa cntnfa_automaton;
    if (load_cntnfa_with_counters(cntnfa_filename, cntnfa_automaton, alphabet) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::string input(1000, 'a');
    input += std::string(1000, 'b');
    mata::Word word = make_word(input, alphabet);

    TIME_BEGIN(accepted_by_nfa);
    nfa_automaton.is_in_lang(word);
    TIME_END(accepted_by_nfa);

    TIME_BEGIN(accepted_by_cntnfa);
    cntnfa_automaton.is_in_lang_of_counter_nfa(word);
    TIME_END(accepted_by_cntnfa);

    return EXIT_SUCCESS;
}
