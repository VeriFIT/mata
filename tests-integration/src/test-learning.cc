#include "utils/utils.hh"
#include "mata/nfa/learning.hh"

constexpr bool MINTERMIZE_AUTOMATA{false};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Input files or algorithm missing\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];
    Nfa test_automaton;
    mata::OnTheFlyAlphabet alphabet;
    if (load_automaton(filename, test_automaton, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    StateRenaming* state_renaming = new StateRenaming();
    ParameterMap params_learning;
    params_learning["algorithm"] = "nlstar";
    ParameterMap params_residuals;
    params_residuals["algorithm"] = "residual";
    params_residuals["type"] = "with";
    params_residuals["direction"] = "forward";
    Nfa conjecture_nlstar = learn(test_automaton, params_learning);
    Nfa conjecture_residuals = reduce(test_automaton, state_renaming, params_residuals);
    if(conjecture_nlstar.num_of_states() != conjecture_residuals.num_of_states()){
        return EXIT_FAILURE;
    }
    if(!are_equivalent(conjecture_nlstar, conjecture_residuals)){
        return EXIT_FAILURE;
    }
    params_learning["algorithm"] = "lstar";
    Nfa conjecture_lstar = learn(test_automaton, params_learning);
    Nfa conjecture_minimize = minimize_brzozowski(test_automaton);
    if(!are_equivalent(conjecture_lstar, conjecture_minimize)){
        return EXIT_FAILURE;
    }
    if(conjecture_lstar.num_of_states() != conjecture_minimize.num_of_states()){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}