#ifndef __MATA_NFA_LEARNING_HH__
#define __MATA_NFA_LEARNING_HH__

#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/alphabet.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/learning.hh"

#include <string>
#include <stdbool.h>
#include <vector>
using namespace mata::nfa;
using namespace mata;
using namespace std;
using namespace mata::utils;
using namespace mata::nfa::algorithms;

/**
 * @brief Structure of one row in the OT
 * stores its binary vector, access string
 * and a set of row in cover relation
 * 
 */
struct Row{
    BoolVector T = BoolVector();
    Word value = Word();
    //needed only for NLstar
    vector<shared_ptr<Row>>covering = vector<shared_ptr<Row>>();
};

/**
 * @brief Structure used for constructing
 * conjectures
 */
struct State_construction{
    Row row;
    long unsigned int index;
    State_construction(Row row, long unsigned int index) : row(row), index(index) {}
};

struct unclosed{
    Word unclosed_word = Word();
    shared_ptr<Row>unclosed_row = nullptr;
};

/**
 * @brief Used to save information about
 * consistency and closure checks
 * 
 */
struct const_check_res{
    bool consistent;
    bool closed;
    Word c;
    unclosed not_closed;
    const_check_res() : consistent(false), closed(false), c(), not_closed() {}
};

/**
 * @brief OT for NL* algorithm inferring RFSA/DFA
 * stores all rows in all, distinguishing to top and
 * bottom parts is done with vectorsz of pointer (S, Splus)
 * E stores experiment strings
 * 
 */
class OT{
    public:
        vector<shared_ptr<Row>> S = vector<shared_ptr<Row>>(); // top part
        vector<shared_ptr<Row>> Splus = vector<shared_ptr<Row>>(); // bottom part
        vector<shared_ptr<Row>> all = vector<shared_ptr<Row>>(); // stores all rows
        vector<Word> E = vector<Word>(); // experiment strings
        map<Word, shared_ptr<Row>> all_map = map<Word, shared_ptr<Row>>(); // map for faster access to rows
   
    OT(OrdVector<Symbol> alphabet, Nfa teacher, const ParameterMap& params);
    ~OT();

    /**
     * @brief Join operator defined for two rows -- basically binary or
     * 
     */
    inline BoolVector join_op(BoolVector r1, BoolVector r2);

    /**
     * @brief if r1[i] = +, it implies that r2[i] = +
     * 
     * @param r1 first binary vector
     * @param r2 second binary vector
     * @return true if r1 is covered by r2
     * @return false if not covered
     */
    inline bool covers(BoolVector r1, BoolVector r2);

    /**
     * @brief Returns a membership query
     * for a given access string concatenated with a 
     * given experiment string
     * 
     * @param aut oracle automaton answering the query 
     * @param e experiment string
     * @param value access string
     * @return true if accepted
     * @return false if not accepted
     */
    bool get_t(Nfa aut, Word e, Word value);

    /**
     * @brief Evaluates if a certain row from the upper part
     * is prime, meaning that it shouldnt be joined by
     * rows it covers
     * 
     * @param row evaluated row
     * @return true if prime
     * @return false if composed (not prime)
     */
    bool is_prime(shared_ptr<Row> row);

    /**
     * @brief RFSA Closure property -- checks if
     * all rows in bottom part are join of prime rows
     * from the top part (have to also be in cover relation)
     * 
     * @param not_closed returned unclosed row access string, ptr to row
     * @return true if closed
     * @return false if not closed
     */
    bool rfsa_closure(unclosed& not_closed);

    /**
     * @brief DFA Closure property -- checks if all
     * rows in the bottom part also appear in the top part
     * of the table
     * 
     * @param not_closed returned unclosed row access string, ptr to row
     * @return true if closed
     * @return false not closed
     */
    bool dfa_closure(unclosed& not_closed);

    /**
     * @brief RFSA Consistency property -- checks if all
     * pairs of rows in cover relation imply that their extensions are in
     * cover relation
     * 
     * @param c word that will be added to experiment strings if not consistent
     * @param alphabet input alphabet
     * @return true if consistent
     * @return false if not consistent
     */
    bool rfsa_consistency(Word& c, OrdVector<Symbol> alphabet);

    /**
     * @brief DFA Consistency property -- checks if all
     * rows in the top part that are equal are also equal
     * when extended by a symbol from the alphabet
     * 
     * @param c word that will be added to experiment strings if not consistent
     * @param alphabet input alphabet
     * @return true if consistent
     * @return false if not consistent
     */
    bool dfa_consistency(Word& c, OrdVector<Symbol> alphabet);

    /**
     * @brief When a row is moved to the top part
     * its covering relation vector has to be filled
     * from the scratch
     * Only for NLstar
     * 
     * @param row row to evaluate cover relation for
     */
    void get_covering(shared_ptr<Row> row);

    /**
     * @brief If a new experiment string is added,
     * cover relation of rows can change, if some coverage is not
     * valid anymore, it is removed
     * Only for NLstar
     * 
     */
    void update_covering_new_e();

    /**
     * @brief If a new row is added to the bottom part
     * rows in the top part check if they are not in cover relation with it
     * Only for NLstar
     * 
     * @param row row that was added
     */
    void update_covering_new_row(shared_ptr<Row> row);

    /**
     * @brief adds a new row to the bottom part of the OT
     * 
     * @param row row to be added
     */
    inline void add_to_Splus(shared_ptr<Row> row);

    /**
     * @brief When the table is not closed, this moves
     * the unclosed row to the top part of the table and adds new
     * rows to the bottom part
     * 
     * @param not_closed input string of the unclosed row
     * @param alphabet input alphabet
     * @param teacher oracle automaton
     */
    void state_not_closed(unclosed not_closed, OrdVector<Symbol> alphabet, Nfa teacher);

    /**
     * @brief Adds new columns to the table by adding experiment words
     * and extending the table by membership queries
     * 
     * @param teacher oracle answering queries
     * @param c word to get added to experiment strings
     * @param params parameters to distinguish L* and NL* approaches
     */
    void update_consistency(Nfa teacher, Word c, const ParameterMap& params);
    
    /**
     * @brief Adds all suffixes from cex to the OT as experiment strings (to E)
     * this adds new collumns to the table to distinguish states of the automaton
     * same as updating consistency
     * 
     * @param cex counter-example word
     * @param teacher oracle automaton
     * @param params parameters to distinguish L* and NL* approaches
     */
    void update_after_cex(Word cex, Nfa teacher, const ParameterMap& params);

    /**
     * @brief Checks whether OT is consistent a closed 
     * -> if we can construct a conjecture automaton
     * 
     * @param res structure saving information about closure, consistency properties
     * @param alphabet input alphabet
     * @param params parameters to distinguish L* and NL* approaches
     * @return true if can be constructed
     * @return false if cant be constructed
     * 
     */
    bool mata_can_be_constructed(const_check_res& res, OrdVector<Symbol> alphabet, const ParameterMap& params);
};

/**
 * @brief Membership query for OT - checks whether
 * given input word is accepted by the oracle or not
 * 
 * @param teacher automaton to get the query from
 * @param word input string
 * @return true if the word is accepted by teacher
 * @return false if its not accepted
 */
bool membership_query(Nfa teacher, Word word);

/**
 * @brief Constructs a conjecture automaton
 * first defines all the states of the conjecture, 
 * then computes transition function
 * 
 * @param table OT to get the conjecture from
 * @param alphabet input alphabet
 * @param params parameters to distinguish L* and NL* approaches
 * @return Nfa hypothesis automaton
 */
Nfa construct_conjecture(OT table, OrdVector<Symbol> alphabet, const ParameterMap& params);

/**
 * @brief Gets all suffixes from counter-example, that
 * will get added to the OT
 * 
 * @param cex word to get suffixes from
 * @return set<Word> set of suffixes
 */
inline set<Word> get_all_suffixes(Word cex);

/**
 * @brief Equivalence query for OT, checks whether
 * constructed conjecture's language is equivalent to the oracle
 * if not, returns a counter-example
 * 
 * @param teacher oracle automaton
 * @param conjecture constructed hypothesis
 * @param cex in case they are not equivalent - counter-example word
 * @return true if equivalent 
 * @return false if not equivalent
 */
bool equivalence_query(Nfa teacher, Nfa conjecture, Word& cex);

Nfa learn(Nfa teacher, const ParameterMap& params);


#endif