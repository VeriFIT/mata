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

namespace mata {
    struct MintermizationDomain {
        Cudd bdd_mng; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
        BDD val;

        MintermizationDomain() : bdd_mng(0), val(BDD()) {}

        MintermizationDomain(Cudd mng) : bdd_mng(mng), val(BDD()) {}

        MintermizationDomain(Cudd mng, BDD val) : bdd_mng(mng), val(val) {};

        friend MintermizationDomain operator&&(const MintermizationDomain& lhs, const MintermizationDomain &rhs) {
            return {lhs.bdd_mng, lhs.val * rhs.val};
        }

        friend MintermizationDomain operator||(const MintermizationDomain& lhs, const MintermizationDomain &rhs) {
            return {lhs.bdd_mng, lhs.val + rhs.val};
        }

        friend MintermizationDomain operator!(const MintermizationDomain &lhs) {
            return {lhs.bdd_mng, !lhs.val};
        }

        bool operator==(const MintermizationDomain &rhs) const {
            return this->val == rhs.val;
        }

        bool isFalse() const {
            return val.IsZero();
        }

        MintermizationDomain getTrue() const;
        MintermizationDomain getFalse() const;
        MintermizationDomain getVar() const;
    };
}

// custom specialization of std::hash can be injected in namespace std
namespace std {
    template<>
    struct hash<struct mata::MintermizationDomain> {
        size_t operator()(const struct mata::MintermizationDomain &algebra) const noexcept {
            return hash<BDD>{}(algebra.val);
        }
    };
}

namespace mata {
class Mintermization {
private: // data types
    struct OptionalValue {
        enum class TYPE {NOTHING_E, VALUE_E};

        TYPE type;
        MintermizationDomain val;

        OptionalValue() : type(TYPE::NOTHING_E) {}
        explicit OptionalValue(const MintermizationDomain& algebra) : type(TYPE::VALUE_E), val(algebra) {}
        OptionalValue(TYPE t, const MintermizationDomain& algebra) : type(t), val(algebra) {}

        OptionalValue operator*(const OptionalValue& b) const;
        OptionalValue operator+(const OptionalValue& b) const;
        OptionalValue operator!() const;
    };

    using DisjunctStatesPair = std::pair<const FormulaGraph *, const FormulaGraph *>;

private: // private data members
    MintermizationDomain domain_base;
    std::unordered_map<std::string, MintermizationDomain> symbol_to_var{};
    std::unordered_map<const FormulaGraph*, MintermizationDomain> trans_to_var{};
    std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states{};
    std::unordered_set<MintermizationDomain> vars{}; // vars created from transitions

private:
    void trans_to_vars_nfa(const IntermediateAut& aut);

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param source_bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    std::unordered_set<MintermizationDomain> compute_minterms(
            const std::unordered_set<MintermizationDomain>& source_bdds);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    MintermizationDomain graph_to_vars_nfa(const FormulaGraph& graph);

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
                             const std::unordered_set<MintermizationDomain>& minterms);

    Mintermization() : domain_base(), symbol_to_var{}, trans_to_var() {
    }
}; // class Mintermization.
} // namespace mata
#endif //MATA_MINTERM_HH
