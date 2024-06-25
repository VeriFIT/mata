// TODO: Insert file header.

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"

#include <limits>
#include <fstream>
#include <cmath>

namespace mata::nfa {

extern const std::string TYPE_NFA;

using State = unsigned long;
using StateSet = mata::utils::OrdVector<State>;

struct Run {
    Word word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
};

using StateRenaming = std::unordered_map<State, State>;

/**
 * @brief Map of additional parameter name and value pairs.
 *
 * Used by certain functions for specifying some additional parameters in the following format:
 * ```cpp
 * ParameterMap {
 *     { "algorithm", "classical" },
 *     { "minimize", "true" }
 * }
 * ```
 */
using ParameterMap = std::unordered_map<std::string, std::string>;

struct Limits {
public:
    static const State min_state = std::numeric_limits<State>::min();
    static const State max_state = std::numeric_limits<State>::max();
    static const Symbol min_symbol = std::numeric_limits<Symbol>::min();
    static const Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

struct Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

// defining indexes of logic operators used in an input vector for tseytin transformation
#define TSEY_AND -1
#define TSEY_OR -2
#define TSEY_NOT -3

// define charactes used for SAT and QBF solvers
#define SOL_EOL std::string("0\n")
#define SOL_NEG std::string("-")
#define SOL_DELIM std::string(" ")
#define SOL_HEADER std::string("p cnf ")
#define SOL_FORALL std::string("a")
#define SOL_EXISTS std::string("e")

/**
 * Base class for representing the input parameters for SAT and QBF reduction
 */
struct AutStats {
public:
    /** holds the number of states of the created automaton*/
    size_t state_num;
    /** holds the number of symbols of the created automaton*/
    size_t alpha_num;

    /** sets of example words defining the automaton's language*/
    std::set <Word> accept;
    std::set <Word> reject;

    /** output stream*/
    std::ostream& output;

public:

    /** Constructor for AutStats */
    AutStats(size_t states, size_t alphabet, std::ostream& out, std::set<Word> acc = {}, std::set<Word> rej = {})
        : state_num(states), alpha_num(alphabet), accept(acc), reject(rej), output(out) {}

    /**
     * Debug function printing the members of the class to the output
     */
    void print(std::ostream &output){
        output << "States = " << this->state_num << ", Symbols = " << this->alpha_num << std::endl;
        output << "Accept:";
        for (const auto& word: this->accept){
            for (size_t i = 0; i < word.size(); i++){
                output << " " << word[i];
            }
            output << ",";
        }

        output << std::endl << "Reject:";
        for (const auto& word: this->reject){
            for (size_t i = 0; i < word.size(); i++){
                output << " " << word[i];
            }
            output << ",";
        }
        output << std::endl << "---------------------------"<< std::endl;
    }

    /***
     * @brief Create the instance of Nfa from the result of the solver reduction from the given input, depends on the solver
     * @param solver_result Input stream of the solver result
     * @param params Selected solver reduction
     * @return Nfa instance corresponding to the solver generated automaton
     */
    Nfa build_result(std::istream& solver_result, const ParameterMap& params);
};

/**
 * Class for input parameters for SAT reduction, inherits from AutStats
 *  Variables indexed by rows for N=3 S=2:
 *      1  - T1a1    2  - T1a2    3  - T1a3        4  - T2a1    5  - T2a2    6  - T2a3
 *      7  - T3a1    8  - T3a2    9  - T3a3        10 - T1b1    11 - T1b2    12 - T1b3
 *      13 - T2b1    14 - T2b2    15 - T2b3        16 - T3b1    17 - T3a2    18 - T3b3
 *      19 - F1      20 - F2      21 - F3
 */
struct SatStats : public AutStats{
public:

    /**
     * @brief Construct a new explicit SatStats from other SatStats.
     */
    SatStats(const SatStats& other) = default;

    explicit SatStats(const size_t num_of_states, const size_t size_of_alphabet, std::ostream& out, std::set<Word> acc = {},
                        std::set<Word> rej = {})  : AutStats(num_of_states, size_of_alphabet, out, acc, rej) {}

    /**
     * Generates the clauses for determinism of the automaton
     *  -1 or -2    and     -1 or -3    and     -2 or -3
     */
    void determine_clauses() const;

    /**
     * Generates the clauses for completeness of the automaton
     *  1 or 2 or 3
     */
    void complete_clauses() const;

    /**
     * @brief Generates the clauses for accepting and rejecting words, calls recurse_tseytin and recurse_tseytin_reject
     * @param max_index First free variable that can be used for CNF transformation
     * @return New max_index representing the first free variable that is possible to use for CNF transformation
     */
    size_t example_clauses(size_t max_index);

    /**
     * @brief Recursively generates and stores the clauses for accepting a word in DNF
     * @param base Partial vector for storage for the result
     * @param state State where the previous transition ended
     * @param word Word that is being accepted
     * @param pos Current position in the word
     * @param [out] result Output vector with the formula for CNF transformation
     * @param skip_init Number of variables to skip in case of initial variables for nfa (default is 0)
     */
    void recurse_tseytin_accept(const std::vector<int>& base, size_t state, Word word, const unsigned pos,
                                                std::vector<int>& result, size_t skip_init = 0);

    /**
     * @brief Recursively generates and prints the clauses on output for rejecting a word
     * @param base Partial string for storage fo the output
     * @param state State where the previous transition ended
     * @param pos Current position in the word
     * @param word Word that is being rejected
     * @param skip_init Number of variables to skip in case of initial variables for nfa (default is 0)
     */
    void recurse_tseytin_reject(const std::string& base, size_t state, Word word, const unsigned pos, size_t skip_init = 0);

    /**
     * @brief Generates the clauses for accepting and rejecting words for a nondeterministic automaton,
     * possibility of multiple initial states, calls recurse_tseytin and recurse_tseytin_reject
     * @param max_index First free variable that can be used for CNF transformation
     * @return New max_index representing the first free variable that is possible to use for CNF transformation
    */
    size_t example_nfa_clauses(size_t max_index);

}; // struct SatStats

/**
 * Class for input parameters for QBF reduction, inherits from AutStats
 *  Variables indexed by rows for N=3 S=2:
 *      1  - T1a1    2  - T1a2    3  - T1a3        4  - T2a1    5  - T2a2    6  - T2a3
 *      7  - T3a1    8  - T3a2    9  - T3a3        10 - T1b1    11 - T1b2    12 - T1b3
 *      13 - T2b1    14 - T2b2    15 - T2b3        16 - T3b1    17 - T3a2    18 - T3b3
 *      19 - I1      20 - I2      21 - I3          22 - F1      23 - F2      24 - F3
 */
struct QbfStats : public AutStats {
public:
    /** size of the binary vector representing the state*/
    unsigned state_bin;

    /**
     * @brief Construct a new explicit QbfStats from other QbfStats.
     */
    QbfStats(const QbfStats& other) = default;

    explicit QbfStats(const size_t num_of_states, const size_t size_of_alphabet, std::ostream& out, std::set<Word> acc = {},
                        std::set<Word> rej = {}) : AutStats(num_of_states, size_of_alphabet, out, acc, rej),
                        state_bin(static_cast<unsigned>(ceil(log2(static_cast<double>(num_of_states))))){}

    /**
     * @brief Recompute the state_bin value from the number of states
     */
    void recompute_bin(){
        this->state_bin = static_cast<unsigned>(ceil(log2(static_cast<double>(this->state_num))));
    }

    /**
     * @brief Generates and prints clauses for setting the initial and final states for accepting a word
     * @param state_base First variable of the first state
     * @param end_base First variable of the last state
     */
    void init_final_clauses(size_t state_base, size_t end_base);

    /**
     * @brief Generates the clauses for initial and final states when accepting a word by iterating through
     * the combinations of the binary vector
     * @param var_base First quantified variable of binary vector of the initial/final state
     * @param result_base Starting initial/final static variables
     */
    void init_final(size_t var_base, size_t result_base);

    /**
     * @brief Generates and stores the clauses for initial and final states when rejecting a word
     *  by iterating thought the combinations of the binary vector
     * @param var_base First quantified variable of binary vector of the initial/final state
     * @param result_base Starting initial/final static variables
     * @param [out] result Output vector with the generated clauses
     */
    void init_final_reject(size_t var_base, size_t result_base, std::vector<int>& result);

    /***
     * @brief Generates and stores clauses for setting the initial and final state for rejecting the word
     * @param state_base First variable of the first state
     * @param end_base First variable of the last state
     * @param [out] result Vector where the generated clauses are stored for CNF transformation
     */
    void init_final_clauses_reject(size_t state_base, size_t end_base, std::vector<int>& result);

    /**
     * @brief Prints the clauses for covering the invalid combinations of the binary vector for accepting a word
     *  that have to be rejected
     * @param start First variable of the given state to invalidate
     */
    void valid_combinations(size_t start);

    /**
     * @brief Stores the clauses for covering the invalid combinations of the binary vector for a rejected word
     *  that have to be accepted
     * @param start First variable of the given state to validate
     * @param [out] input Vector where the generated clauses are stored for CNF transformation
     */
    void valid_combinations_reject(size_t start, std::vector <int>& input);

    /**
     * @brief Generates the transition clauses for accepting a word for a single symbol
     * @param var First variable of the given state
     * @param trans First transition variable for the required symbol
     */
    void accept_clauses(size_t var, size_t curr_trans);

    /**
     * @brief Generates and stores the transition clauses for rejecting a word
     * @param var First variable of the given state
     * @param trans First transition variable for the required symbol
     * @param [out] result Vector where the generated clauses are stored for CNF transformation
     */
    void reject_clauses(size_t free_var, size_t curr_trans, std::vector <int>& result);

    /***
     * @brief Generates the clauses for example words that should be accepted or rejected by the automaton
     * @param max_index Index of the first variable that is free and
     *                      can be used for new variables for CNF transformation (includes the state variables)
     */
    void example_clauses(size_t max_index);

    /***
     * @brief Prints the number of variables, clauses and quantified variables for QDIMACS FORMAT
     * @return First free index of the variable for CNF transformation (including state variables)
     */
    size_t print_qbf_header(){
        size_t states = this->state_num;        // number of states
        size_t alpha = this->alpha_num;         // number of symbols
        size_t bin = this->state_bin;           // size of binary vector
        size_t un_var = states * states * alpha + 2 * states;     // number of unquantified variables
        size_t acc_var = 0, rej_var = 0, claus = 0, dynamic_tsei_vars = 0;

        for (auto word: this->accept){
            if (word.empty()){
                claus++;                        // epsilon clause, no vars
                continue;
            }
            acc_var += (word.size() + 1) * bin;                     // new quant vars for a word, state variables

            claus += 2*states + states*states*word.size();         // init + final + transition clauses
            claus += (static_cast<size_t>(pow(2, static_cast<double>(bin))) - states) * (word.size()+1);    // valid clauses
        }

        for (auto word: this->reject){
            if (word.empty()){
                claus += states;                // epsilon clauses, no vars
                continue;
            }
            rej_var += (word.size() + 1) * bin;                             // new quant vars for a word, state variables

            // dynamic tseytin clauses
            dynamic_tsei_vars += 2*states + states*states*word.size();      // single var for each init, final
            dynamic_tsei_vars += (static_cast<size_t>(pow(2, static_cast<double>(bin))) - states) * (word.size()+1);    // trans clauses

            claus += 2 * states * ((bin+1)+1);                          // init + final clauses
            claus += states*states*word.size() * ((2*bin+1)+1);         // trans clauses
            claus += (static_cast<size_t>(pow(2, static_cast<double>(bin))) - states) * (word.size()+1) * (bin+1);   // valid clauses
            claus++;                            //  add tseitsen result clause
        }

        claus++;                                // add the clause for setting 0 as initial state

        // header line with the number fo used variables and clauses
        this->output << SOL_HEADER << un_var + acc_var + rej_var + dynamic_tsei_vars << SOL_DELIM << claus << std::endl;

        // print quantified variables
        if (rej_var != 0){
            this->output << SOL_FORALL;
            this->print_quant_vars(un_var+acc_var+1, rej_var);
            this->output << SOL_DELIM << SOL_EOL;
        }

        if (acc_var != 0 && dynamic_tsei_vars != 0){
            this->output << SOL_EXISTS;
            this->print_quant_vars(un_var+1, acc_var);
            this->print_quant_vars(un_var+acc_var+rej_var+1, dynamic_tsei_vars);
            this->output << SOL_DELIM << SOL_EOL;
        }

        return un_var+acc_var+rej_var+1;
    }

    /***
     * @brief Prints quantified variables
     * @param from Starting index of the variable
     * @param num Number of variables
     */
    void print_quant_vars(size_t from, size_t num){
        for (size_t i = 0; i < num; i++){
            this->output << SOL_DELIM << from+i;
        }
    }
};  // struct QbfStats

} // namespace mata::nfa.

#endif //MATA_TYPES_HH
