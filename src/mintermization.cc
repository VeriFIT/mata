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

namespace
{
    const Mata::FormulaGraph* detect_state_part(const Mata::FormulaGraph* node)
    {
        std::vector<const Mata::FormulaGraph *> todo{node};
        while (!todo.empty()) {
            const auto act_node = todo.back();
            assert(act_node != nullptr);
            todo.pop_back();
            if (act_node->children.size() != 2)
                continue;
            if (act_node->children.front().node.is_symbol() && act_node->children[1].node.is_state())
                return &act_node->children[1];
            else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_symbol())
                return &act_node->children.front();
            else if (act_node->children.front().node.is_operand() && act_node->children[1].node.is_state())
                return act_node;
            else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_operand())
                return act_node;
            else {
                for (const Mata::FormulaGraph& child : act_node->children) {
                    todo.push_back(&child);
                }
            }
        }

        return nullptr;
    }
}

std::vector<BDD> Mata::Mintermization::trans_to_bdd(const IntermediateAut &aut)
{
    std::vector<BDD> bdds;

    for (const auto& trans : aut.transitions) {
        // Foreach transition create a BDD
        const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
        assert(symbol_part.node.is_operator()); // beginning of symbol part of transition
        bdds.push_back(graph_to_bdd(symbol_part));
        trans_to_bddvar[&symbol_part] = bdds.back();
    }

    return bdds;
}

std::vector<BDD> Mata::Mintermization::trans_to_bdd_afa(const IntermediateAut &aut)
{
    assert(aut.is_afa());
    std::vector<BDD> bdds;

    for (const auto& trans : aut.transitions) {
        if (trans.second.node.is_state())
            continue;
        lhs_to_disjuncts_and_states[&trans.first] = std::vector<DisjunctStatesPair>();
        // split transition to disjuncts
        const FormulaGraph *act_graph = &trans.second;
        while (act_graph->node.is_operator() && act_graph->node.operator_type == FormulaNode::OR) {
            // map lhs to disjunct and its state fomula
            lhs_to_disjuncts_and_states[&trans.first].push_back(DisjunctStatesPair(act_graph, detect_state_part(
                    act_graph)));
            act_graph = &(act_graph->children.front());
        }

        // Foreach disjunct create a BDD
        for (const DisjunctStatesPair ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // create bdd for the whole disjunct
            const auto bdd = graph_to_bdd_generalized(*ds_pair.first);
            trans_to_bddvar[ds_pair.first] = bdds.back();
            assert(bdd.type == BddOrNothing::BDD_E); // TODO this will probably fail since the last conjunct is state
            bdds.push_back(bdd.val);
        }
    }

    return bdds;
}

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

const Mata::Mintermization::BddOrNothing Mata::Mintermization::graph_to_bdd_generalized(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (node.is_state())
            return BddOrNothing(BddOrNothing::NOTHING_E);
        if (symbol_to_bddvar.count(node.name)) {
            return BddOrNothing(symbol_to_bddvar.at(node.name));
        } else {
            BDD res = bdd_mng.bddVar();
            symbol_to_bddvar[node.name] = res;
            return BddOrNothing(res);
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::AND) {
            assert(graph.children.size() == 2);
            const BddOrNothing op1 = graph_to_bdd_generalized(graph.children[0]);
            const BddOrNothing op2 = graph_to_bdd_generalized(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OR) {
            assert(graph.children.size() == 2);
            const BddOrNothing op1 = graph_to_bdd_generalized(graph.children[0]);
            const BddOrNothing op2 = graph_to_bdd_generalized(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::NEG) {
            assert(graph.children.size() == 1);
            const BddOrNothing op1 = graph_to_bdd_generalized(graph.children[0]);
            return !op1;
        } else
            assert(false);
    }

    assert(false);
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

    assert(false);
}

void Mata::Mintermization::minterms_to_aut(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                                           const std::vector<BDD>& minterms)
{
    size_t symbol = 0;
    for (const auto& trans : aut.transitions) {
            // for each t=(q1,s,q2)
        const auto &symbol_part = trans.second.children[0];

        assert(trans_to_bddvar.count(&symbol_part));
        const BDD &bdd = trans_to_bddvar[&symbol_part];

        for (const auto &minterm: minterms) {
            // for each minterm x:
            if (!((bdd * minterm).IsZero())) {
                // if for symbol s of t is BDD_s < x
                // add q1,x,q2 to transitions
                IntermediateAut::parse_transition(res, {trans.first.raw, std::to_string(symbol),
                                                        trans.second.children[1].node.raw});
                symbol++;
            }
        }
    }
}

void Mata::Mintermization::minterms_to_aut_afa(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                                           const std::vector<BDD>& minterms)
{
    size_t symbol = 0;
    for (const auto& trans : aut.transitions) {
        for (const auto& ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // for each t=(q1,s,q2)
            const auto disjunct = ds_pair.first;

            assert(trans_to_bddvar.count(disjunct));
            const BDD &bdd = trans_to_bddvar[disjunct];

            for (const auto &minterm: minterms) {
                // for each minterm x:
                if (!((bdd * minterm).IsZero())) {
                    // if for symbol s of t is BDD_s < x
                    // add q1,x,q2 to transitions
                    const auto str_symbol = std::to_string(symbol);
                    FormulaNode node_symbol(FormulaNode::OPERAND, str_symbol, str_symbol,
                                            Mata::FormulaNode::OperandType::SYMBOL);
                    res.add_transition(trans.first, node_symbol, *ds_pair.second);
                    symbol++;
                }
            }
        }
    }
}

Mata::IntermediateAut Mata::Mintermization::mintermize(const Mata::IntermediateAut& aut)
{
    if (!aut.is_nfa() && !aut.is_afa() && aut.alphabet_type == IntermediateAut::BITVECTOR) {
        throw std::runtime_error("We currently support mintermization only for NFA with bitvectors");
    }

    if (aut.is_afa())
        aut.print_transitions_trees(std::cout);

    std::vector<BDD> bdds = aut.is_nfa() ? trans_to_bdd(aut) : trans_to_bdd_afa(aut);

    // Build minterm tree over BDDs
    std::vector<BDD> minterms = compute_minterms(bdds);
    IntermediateAut res = aut;
    res.alphabet_type = IntermediateAut::EXPLICIT;
    res.transitions.clear();

    if (aut.is_nfa())
        minterms_to_aut(res, aut, minterms);
    else if (aut.is_afa())
        minterms_to_aut_afa(res, aut, minterms);

    return res;
}
