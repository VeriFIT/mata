/* mintermization.hh -- Mintermization of automaton.
 * It transforms an automaton with a bitvector formula used as a symbol to minterminized version of the automaton.
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MATA_MINTERM_HH
#define MATA_MINTERM_HH

#include "mata/cudd/cuddObj.hh"

#include "inter-aut.hh"


namespace Mata {
    struct MintermizationAlgebra {
        Cudd* bdd_mng; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
        BDD val;

        MintermizationAlgebra() : bdd_mng(nullptr), val(BDD()) {}

        MintermizationAlgebra(Cudd* mng) : bdd_mng(mng), val(BDD()) {}

        MintermizationAlgebra(BDD val, Cudd* mng) : bdd_mng(mng), val(val) {};

        MintermizationAlgebra(const MintermizationAlgebra& alg) = default;

        friend MintermizationAlgebra operator&&(const MintermizationAlgebra& lhs, const MintermizationAlgebra &rhs) {
            return MintermizationAlgebra(lhs.val * rhs.val, lhs.bdd_mng);
        }

        friend MintermizationAlgebra operator||(const MintermizationAlgebra& lhs, const MintermizationAlgebra &rhs) {
            return {lhs.val + rhs.val, lhs.bdd_mng};
        }

        friend MintermizationAlgebra operator!(const MintermizationAlgebra &lhs) {
            return {!lhs.val, lhs.bdd_mng};
        }

        bool operator==(const MintermizationAlgebra &rhs) const {
            return this->val == rhs.val;
        }

        bool isFalse() const {
            return val.IsZero();
        }

        MintermizationAlgebra getTrue() const;
        MintermizationAlgebra getFalse() const;
        MintermizationAlgebra getVar() const;
};
}

// custom specialization of std::hash can be injected in namespace std
namespace std {
    template<>
    struct hash<struct Mata::MintermizationAlgebra> {
        size_t operator()(const struct Mata::MintermizationAlgebra &algebra) const noexcept {
            return hash<BDD>{}(algebra.val);
        }
    };
}

namespace Mata {
class Mintermization {
private: // data types
    struct OptionalValue {
        enum class TYPE {NOTHING_E, VALUE_E};

        TYPE type;
        MintermizationAlgebra val;

        OptionalValue() : type(TYPE::NOTHING_E) {}
        explicit OptionalValue(const MintermizationAlgebra& algebra) : type(TYPE::VALUE_E), val(algebra) {}
        OptionalValue(TYPE t, const MintermizationAlgebra& algebra) : type(t), val(algebra) {}

        OptionalValue operator*(const OptionalValue& b) const;
        OptionalValue operator+(const OptionalValue& b) const;
        OptionalValue operator!() const;
    };

    using DisjunctStatesPair = std::pair<const FormulaGraph *, const FormulaGraph *>;

private: // private data members
    Cudd bdd_mng;
    std::unordered_map<std::string, MintermizationAlgebra> symbol_to_var{};
    std::unordered_map<const FormulaGraph*, MintermizationAlgebra> trans_to_var{};
    std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states{};
    std::unordered_set<MintermizationAlgebra> vars{}; // vars created from transitions

private:
    void trans_to_vars_nfa(const IntermediateAut& aut);
    void trans_to_vars_afa(const IntermediateAut& aut);

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param source_bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    std::unordered_set<MintermizationAlgebra> compute_minterms(
            const std::unordered_set<MintermizationAlgebra>& source_bdds);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    MintermizationAlgebra graph_to_vars_nfa(const FormulaGraph& graph);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * This version of method is a more general one and accepts also
     * formula including states.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    OptionalValue graph_to_vars_afa(const FormulaGraph& graph);

    /**
     * Method mintermizes given automaton which has bitvector alphabet.
     * It transforms its transitions to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automaton to explicit one.
     * @param aut Automaton to be mintermized.
     * @return Mintermized automaton
     */
    Mata::IntermediateAut mintermize(const Mata::IntermediateAut& aut);

    /**
     * Methods mintermize given automata which have bitvector alphabet.
     * It transforms transitions of all automata to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automata to explicit one (sharing the same minterms).
     * @param auts Automata to be mintermized.
     * @return Mintermized automata corresponding to the input autamata
     */
    std::vector<Mata::IntermediateAut> mintermize(const std::vector<const Mata::IntermediateAut *> &auts);
    std::vector<Mata::IntermediateAut> mintermize(const std::vector<Mata::IntermediateAut> &auts);

    /**
     * The method performs the mintermization over @aut with given @minterms.
     * It is method specialized for NFA.
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_nfa(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                             const std::unordered_set<MintermizationAlgebra>& minterms);

    /**
     * The method for mintermization of alternating finite automaton using
     * a given set of minterms
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_afa(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                             const std::unordered_set<MintermizationAlgebra>& minterms);

    Mintermization() : symbol_to_var{}, trans_to_var() {
        Mintermization::bdd_mng = Cudd(0);
    }
}; // class Mintermization.

} // namespace Mata

#endif //MATA_MINTERM_HH
