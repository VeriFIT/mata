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

#include <mata/mintermization.hh>

#include <cudd/cudd.h>
#include <cudd/cuddObj.hh>

namespace
{
    void cudd()
    {
        Cudd mgr(0,0);
        BDD x = mgr.bddVar();
        BDD y = mgr.bddVar();
        BDD f = x * y;
        BDD g = y + !x;
        std::cout << "f is" << (f <= g ? "" : " not")
                  << " less than or equal to g\n";
    }
}

Mata::IntermediateAut Mata::Mintermization::mintermize(const Mata::IntermediateAut& aut)
{
    // Foreach transition create a BDD: vector<BDD> f(aut)
    // Build minterm tree over BDDs: vector<MinTerm BDD> g(vector<BDD>)
    // for each t=(q1,s,q2); for each minterm x:
    //   if for symbol s of t is BDD_s < x: bool p (BDD, BDD)
    //      add q1,x,q2 to transitions
}
