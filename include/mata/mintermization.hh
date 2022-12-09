/*
 * mintermization.hh -- Mintermization of automaton
 * It transforms an automaton with a bitvector formula used as a symbol to minterminized version of the automaton.
 *
 * Copyright (c) 2022 Martin Hruska <hruskamartin25@gmail.com>
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

#ifndef _MATA_MINTERM_HH
#define _MATA_MINTERM_HH

#include <cudd/cplusplus/cuddObj.hh>

#include <mata/inter-aut.hh>

namespace Mata
{

class Mintermization
{
private: // data types
    struct BddOrNothing
    {
        enum TYPE {NOTHING_E, BDD_E};

        TYPE type;
        BDD val;

        explicit BddOrNothing(TYPE t) : type(t) {}
        explicit BddOrNothing(const BDD& bdd) : type(BDD_E), val(bdd) {}
        BddOrNothing(TYPE t, const BDD& bdd) : type(t), val(bdd) {}

        BddOrNothing operator*(const BddOrNothing& b) const
        {
            if (this->type == NOTHING_E)
                return b;
            else if (b.type == NOTHING_E)
                return *this;
            else
                return BddOrNothing{BDD_E, this->val * b.val};
        }

        BddOrNothing operator+(const BddOrNothing& b) const
        {
            if (this->type == NOTHING_E)
                return b;
            else if (b.type == NOTHING_E)
                return *this;
            else
                return BddOrNothing{BDD_E, this->val + b.val};
        }

        BddOrNothing operator!() const
        {
            if (this->type == NOTHING_E)
                return BddOrNothing(NOTHING_E);
            else
                return BddOrNothing{BDD_E, !this->val};
        }
    };

    using DisjunctStatesPair = std::pair<const FormulaGraph *, const FormulaGraph *>;

private: // private data members
    Cudd bdd_mng;
    std::unordered_map<std::string, BDD> symbol_to_bddvar;
    std::unordered_map<const FormulaGraph *, BDD> trans_to_bddvar;
    std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states;

private:
    std::vector<BDD> trans_to_bdd(const IntermediateAut& aut);
    std::vector<BDD> trans_to_bdd_afa(const IntermediateAut& aut);

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    static std::vector<BDD> compute_minterms(const std::vector<BDD>& bdds);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    const BDD graph_to_bdd(const FormulaGraph& graph);

    /**
     * Transforms a graph representing formula at transition to bdd.
     * This version of method is a more general one and accepts also
     * formula including states.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    const BddOrNothing graph_to_bdd_generalized(const FormulaGraph& graph);

    /**
     * Methods mintermizes given automaton which has bitvector alphabet.
     * It transforms its transitions to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automaton to explicit one.
     * @param aut Automaton to be mintermized.
     * @return Mintermized automaton
     */
    Mata::IntermediateAut mintermize(const Mata::IntermediateAut& aut);

    /**
     * The method performs the mintermization over @aut with given @minterms.
     * It is method specialized for NFA.
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut, const std::vector<BDD>& minterms);

    /**
     * The method for mintermization of alternating finite automaton using
     * a given set of minterms
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_afa(Mata::IntermediateAut& res,
                             const Mata::IntermediateAut& aut, const std::vector<BDD>& minterms);

    Mintermization() : bdd_mng(0), symbol_to_bddvar{}, trans_to_bddvar()
    {}
};

}  // namespace Mata
#endif //_MATA_MINTERM_HH
