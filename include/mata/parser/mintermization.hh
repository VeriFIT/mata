/* mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used as a symbol to minterminized version of the automaton.
 */

#ifndef MATA_MINTERM_HH
#define MATA_MINTERM_HH

#include "mata/cudd/cuddObj.hh"

#include "inter-aut.hh"

namespace mata {

class Mintermization {
private: // data types
    struct OptionalBdd {
        enum class TYPE {NOTHING_E, BDD_E};

        TYPE type{};
        BDD val{};

        OptionalBdd() = default;
        explicit OptionalBdd(TYPE t) : type(t) {}
        explicit OptionalBdd(const BDD& bdd) : type(TYPE::BDD_E), val(bdd) {}
        OptionalBdd(TYPE t, const BDD& bdd) : type(t), val(bdd) {}

        OptionalBdd operator*(const OptionalBdd& b) const;
        OptionalBdd operator+(const OptionalBdd& b) const;
        OptionalBdd operator!() const;
    };

    using DisjunctStatesPair = std::pair<const FormulaGraph *, const FormulaGraph *>;

private: // private data members
    Cudd bdd_mng{}; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
    std::unordered_map<std::string, BDD> symbol_to_bddvar{};
    std::unordered_map<const FormulaGraph*, BDD> trans_to_bddvar{};
    std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states{};
    std::unordered_set<BDD> bdds{}; // bdds created from transitions

private:
    void trans_to_bdd_nfa(const IntermediateAut& aut);
    void trans_to_bdd_afa(const IntermediateAut& aut);

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param source_bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    std::unordered_set<BDD> compute_minterms(const std::unordered_set<BDD>& source_bdds);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    BDD graph_to_bdd_nfa(const FormulaGraph& graph);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * This version of method is a more general one and accepts also
     * formula including states.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    OptionalBdd graph_to_bdd_afa(const FormulaGraph& graph);

    /**
     * Method mintermizes given automaton which has bitvector alphabet.
     * It transforms its transitions to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automaton to explicit one.
     * @param aut Automaton to be mintermized.
     * @return Mintermized automaton
     */
    mata::IntermediateAut mintermize(const mata::IntermediateAut& aut);

    /**
     * Methods mintermize given automata which have bitvector alphabet.
     * It transforms transitions of all automata to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automata to explicit one (sharing the same minterms).
     * @param auts Automata to be mintermized.
     * @return Mintermized automata corresponding to the input autamata
     */
    std::vector<mata::IntermediateAut> mintermize(const std::vector<const mata::IntermediateAut *> &auts);
    std::vector<mata::IntermediateAut> mintermize(const std::vector<mata::IntermediateAut> &auts);

    /**
     * The method performs the mintermization over @aut with given @minterms.
     * It is method specialized for NFA.
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_nfa(mata::IntermediateAut& res, const mata::IntermediateAut& aut,
                             const std::unordered_set<BDD>& minterms);

    /**
     * The method for mintermization of alternating finite automaton using
     * a given set of minterms
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_afa(mata::IntermediateAut& res, const mata::IntermediateAut& aut,
                             const std::unordered_set<BDD>& minterms);

    Mintermization() : bdd_mng(0), symbol_to_bddvar{}, trans_to_bddvar() {}
}; // class Mintermization.

} // namespace mata
#endif //MATA_MINTERM_HH
