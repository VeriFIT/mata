// TODO: some header

#include "../utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA = false;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::string nfa_filename = argv[1];
    std::string cntnfa_filename = argv[2];

    mata::OnTheFlyAlphabet alphabet;

    std::cout << std::fixed << std::setprecision(4);

    // Load NFA
    Nfa nfa;
    TIME_BEGIN(load_nfa);
    if (load_automaton(nfa_filename, nfa, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    TIME_END(load_nfa);

    // Load CNTNFA
    Cntnfa cntnfa;
    TIME_BEGIN(load_cntnfa);
    if (load_cntnfa_with_counters(cntnfa_filename, cntnfa, alphabet) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    TIME_END(load_cntnfa);

    // Intersection: NFA × NFA
    TIME_BEGIN(intersect_nfa);
    Nfa nfa_result = mata::nfa::intersection(nfa, nfa);
    TIME_END(intersect_nfa);

    // Intersection: CNTNFA × CNTNFA
    TIME_BEGIN(intersect_cntnfa);
    Cntnfa cntnfa_result = mata::cntnfa::intersection(cntnfa, cntnfa);
    TIME_END(intersect_cntnfa);

    // Output metrics for PycoBench
    // std::cout << "states_nfa: " << nfa.num_of_states() << std::endl;
    // std::cout << "states_intersect_nfa: " << nfa_result.num_of_states() << std::endl;
    // std::cout << "states_cntnfa: " << cntnfa.num_of_states() << std::endl;
    // std::cout << "states_intersect_cntnfa: " << cntnfa_result.num_of_states() << std::endl;

    return EXIT_SUCCESS;
}
