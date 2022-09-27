/*
 * mintermization.hh -- Mintermization of automaton
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
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

std::vector<BDD> Mata::Mintermization::compute_minterms(const std::vector<BDD>& bdds)
{
    std::vector<BDD> stack;
    stack.push_back(bdds.front());
    stack.push_back(!bdds.front());

    for (size_t i = 1; i < bdds.size(); ++i) {
        std::vector<BDD> next;
        for (const auto& minterm : stack) {
            BDD b = minterm * bdds[i];
            if (!b.IsZero())
                next.push_back(b);
            BDD b1 = minterm * !bdds[i];
            if (!b1.IsZero())
                next.push_back(b1);
        }
        stack = next;
    }

    return stack;
}

const BDD Mata::Mintermization::graph_to_bdd(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (symbol_to_bddvar.count(node.name)) {
            return symbol_to_bddvar.at(node.name);
        } else {
            BDD res = bdd_mng.bddVar();
            symbol_to_bddvar[node.name] = res;
            return res;
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::AND) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd(graph.children[0]);
            const BDD op2 = graph_to_bdd(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OR) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd(graph.children[0]);
            const BDD op2 = graph_to_bdd(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::NEG) {
            assert(graph.children.size() == 1);
            const BDD op1 = graph_to_bdd(graph.children[0]);
            return !op1;
        } else
            assert(false);
    }
}

Mata::IntermediateAut Mata::Mintermization::mintermize(const Mata::IntermediateAut& aut)
{
    if (!aut.is_nfa() && aut.alphabet_type == IntermediateAut::BITVECTOR) {
        throw std::runtime_error("We currently support mintermization only for NFA with bitvectors");
    }

    std::unordered_map<const FormulaGraph *, BDD> trans_to_bddvar;
    std::unordered_map<std::string, BDD> symbol_to_bddvar;
    std::vector<BDD> bdds;
    for (const auto& trans : aut.transitions) {
        // Foreach transition create a BDD: vector<BDD> f(aut)
        const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
        assert(symbol_part.node.is_operator()); // beginning of symbol part of transition
        bdds.push_back(graph_to_bdd(symbol_part));
        trans_to_bddvar[&symbol_part] = bdds.back();
    }

    // Build minterm tree over BDDs
    std::vector<BDD> minterms = compute_minterms(bdds);
    IntermediateAut res = aut;
    res.alphabet_type = IntermediateAut::EXPLICIT;
    res.transitions.clear();
    size_t symbol = 0;

    for (const auto& trans : aut.transitions) {
        // for each t=(q1,s,q2)
        const auto& symbol_part = trans.second.children[0];
        assert(trans_to_bddvar.count(&symbol_part));
        const BDD& bdd = trans_to_bddvar[&symbol_part];

        for (const auto& minterm : minterms) {
            // for each minterm x:
            if (!((bdd * minterm).IsZero())) {
                // if for symbol s of t is BDD_s < x: bool p (BDD, BDD)
                // add q1,x,q2 to transitions
                IntermediateAut::parse_transition(res, {trans.first.raw, std::to_string(symbol),
                                                        trans.second.children[1].node.raw});
                symbol++;
            }
        }
    }

    return res;
}
