/*
 * mintermization.hh -- Mintermization of automaton
 * It transforms an automaton with a bitvector formula used a symbol to minterminized version of the automaton.
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
private: // private data members
    Cudd bdd_mng;
    std::unordered_map<std::string, BDD> symbol_to_bddvar;

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    static std::vector<BDD> compute_minterms(const std::vector<BDD>& bdds);

    /**
     * Transforms a graph representing formula on transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    const BDD graph_to_bdd(const FormulaGraph& graph);

    /**
     * Methods mintermizes given automaton which has bitvector alphabet.
     * It transforms its transition to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automaton to explicit one.
     * @param aut Automaton to be mintermized.
     * @return Mintermized automaton
     */
    Mata::IntermediateAut mintermize(const Mata::IntermediateAut& aut);

    Mintermization() : bdd_mng(0), symbol_to_bddvar{}
    {}
};

}  // namespace Mata
#endif //_MATA_INTER_AUT_HH
