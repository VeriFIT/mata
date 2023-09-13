/*
 * mintermization.hh -- Mintermization of automaton.
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
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

#include "mata/parser/mintermization.hh"

#include<vector>

using MintermizationAlgebra = struct Mata::MintermizationAlgebra;

namespace {
    const Mata::FormulaGraph* detect_state_part(const Mata::FormulaGraph* node)
    {
        if (node->node.is_state())
            return node;

        std::vector<const Mata::FormulaGraph *> worklist{node};
        while (!worklist.empty()) {
            const auto act_node = worklist.back();
            assert(act_node != nullptr);
            worklist.pop_back();
            if (act_node->children.size() != 2)
                continue;

            if (act_node->children.front().node.is_and() && act_node->children[1].node.is_state())
                return act_node; // ... & a1 & q1 ... & qn
            else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_and())
                return act_node; // ... & a1 & q1 ... & qn
            else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state())
                return act_node; // ... & a1 & q1 & q2
            else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state())
                return act_node; // ... & a1 & q1 & q2
            else if (act_node->node.is_operator() && act_node->children[1].node.is_state())
                return &act_node->children[1]; // a1 & q1
            else if (act_node->children.front().node.is_state() && act_node->node.is_operator())
                return &act_node->children.front(); // a1 & q1
            else {
                for (const Mata::FormulaGraph& child : act_node->children) {
                    worklist.push_back(&child);
                }
            }
        }

        return nullptr;
    }
}

struct Mata::MintermizationAlgebra Mata::MintermizationAlgebra::getTrue() const {
    return MintermizationAlgebra(bdd_mng, bdd_mng.bddOne());
}

struct Mata::MintermizationAlgebra Mata::MintermizationAlgebra::getFalse() const {
    return MintermizationAlgebra(bdd_mng, bdd_mng.bddZero());
}

struct Mata::MintermizationAlgebra Mata::MintermizationAlgebra::getVar() const {
    return MintermizationAlgebra(bdd_mng, bdd_mng.bddVar());
}

void Mata::Mintermization::trans_to_vars_nfa(const IntermediateAut &aut)
{
    assert(aut.is_nfa());

    for (const auto& trans : aut.transitions) {
        // Foreach transition create a MintermizationAlgebra
        const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
        assert((symbol_part.node.is_operator() || symbol_part.children.empty()) &&
            "Symbol part must be either formula or single symbol");
        const MintermizationAlgebra val = graph_to_vars_nfa(symbol_part);
        if (val.isFalse())
            continue;
        vars.insert(val);
        trans_to_var.insert(std::make_pair(&symbol_part, val));
    }
}

void Mata::Mintermization::trans_to_vars_afa(const IntermediateAut &aut)
{
    assert(aut.is_afa());

    for (const auto& trans : aut.transitions) {
        lhs_to_disjuncts_and_states[&trans.first] = std::vector<DisjunctStatesPair>();
        if (trans.second.node.is_state()) { // node from state to state
            lhs_to_disjuncts_and_states[&trans.first].emplace_back(&trans.second, &trans.second);
        }
        // split transition to disjuncts
        const FormulaGraph *act_graph = &trans.second;

        if (!trans.second.node.is_state() && act_graph->node.is_operator() && act_graph->node.operator_type != FormulaNode::OperatorType::OR) // there are no disjuncts
            lhs_to_disjuncts_and_states[&trans.first].emplace_back(act_graph, detect_state_part(
                    act_graph));
        else if (!trans.second.node.is_state()) {
            while (act_graph->node.is_operator() && act_graph->node.operator_type == FormulaNode::OperatorType::OR) {
                // map lhs to disjunct and its state formula. The content of disjunct is right son of actual graph
                // since the left one is a rest of formula
                lhs_to_disjuncts_and_states[&trans.first].emplace_back(&act_graph->children[1],
                                                                                       detect_state_part(
                                                                                           &act_graph->children[1]));
                act_graph = &(act_graph->children.front());
            }

            // take care of last disjunct
            lhs_to_disjuncts_and_states[&trans.first].emplace_back(act_graph,
                                                                                   detect_state_part(
                                                                                           act_graph));
        }

        // Foreach disjunct create a MintermizationAlgebra
        for (const DisjunctStatesPair& ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // create val for the whole disjunct
            const auto val = (ds_pair.first == ds_pair.second) ? // disjunct contains only states
                    OptionalValue(domain_base.getTrue()) : // transition from state to states -> add true as symbol
                             graph_to_vars_afa(*ds_pair.first);
            assert(val.type == OptionalValue::TYPE::VALUE_E);
            if (val.val.isFalse())
                continue;
            trans_to_var.insert(std::make_pair(ds_pair.first, val.val));
            vars.insert(val.val);
        }
    }
}

std::unordered_set<MintermizationAlgebra> Mata::Mintermization::compute_minterms(
        const std::unordered_set<MintermizationAlgebra>& source_bdds)
{
    std::unordered_set<MintermizationAlgebra> stack{ domain_base.getTrue() };
    for (const MintermizationAlgebra& b: source_bdds) {
        std::unordered_set<MintermizationAlgebra> next;
        /**
         * TODO: Possible optimization - we can remember which transition belongs to the currently processed vars
         * and mintermize automaton somehow directly here. However, it would be better to do such optimization
         * in copy of this function and this one keep clean and straightforward.
         */
        for (const auto& minterm : stack) {
            MintermizationAlgebra b1 = minterm && b;
            if (!b1.isFalse()) {
                next.insert(b1);
            }
            MintermizationAlgebra b0 = minterm && !b;
            if (!b0.isFalse()) {
                next.insert(b0);
            }
        }
        stack = next;
    }

    return stack;
}

Mata::Mintermization::OptionalValue Mata::Mintermization::graph_to_vars_afa(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (node.is_state())
            return OptionalValue();
        if (symbol_to_var.count(node.name)) {
            return OptionalValue(symbol_to_var.at(node.name));
        } else {
            MintermizationAlgebra res = (node.name == "true") ? domain_base.getTrue() :
                    (node.name == "false" ? domain_base.getFalse() :
                    domain_base.getVar());
            symbol_to_var.insert(std::make_pair(node.name, res));
            return OptionalValue(res);
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::AND) {
            assert(graph.children.size() == 2);
            const OptionalValue op1 = graph_to_vars_afa(graph.children[0]);
            const OptionalValue op2 = graph_to_vars_afa(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OperatorType::OR) {
            assert(graph.children.size() == 2);
            const OptionalValue op1 = graph_to_vars_afa(graph.children[0]);
            const OptionalValue op2 = graph_to_vars_afa(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::OperatorType::NEG) {
            assert(graph.children.size() == 1);
            const OptionalValue op1 = graph_to_vars_afa(graph.children[0]);
            return !op1;
        } else
            assert(false && "Unknown type of operation. It should conjunction, disjunction, or negation.");
    }

    assert(false);
    return {};
}

MintermizationAlgebra Mata::Mintermization::graph_to_vars_nfa(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (symbol_to_var.count(node.name)) {
            return symbol_to_var.at(node.name);
        } else {
            MintermizationAlgebra res = (node.name == "true") ? domain_base.getTrue() :
                      (node.name == "false" ? domain_base.getFalse() :
                      domain_base.getVar());
            symbol_to_var.insert(std::make_pair(node.name, res));
            return res;
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::AND) {
            assert(graph.children.size() == 2);
            const MintermizationAlgebra op1 = graph_to_vars_nfa(graph.children[0]);
            const MintermizationAlgebra op2 = graph_to_vars_nfa(graph.children[1]);
            return op1 && op2;
        } else if (node.operator_type == FormulaNode::OperatorType::OR) {
            assert(graph.children.size() == 2);
            const MintermizationAlgebra op1 = graph_to_vars_nfa(graph.children[0]);
            const MintermizationAlgebra op2 = graph_to_vars_nfa(graph.children[1]);
            return op1 || op2;
        } else if (node.operator_type == FormulaNode::OperatorType::NEG) {
            assert(graph.children.size() == 1);
            const MintermizationAlgebra op1 = graph_to_vars_nfa(graph.children[0]);
            return !op1;
        } else
            assert(false);
    }

    assert(false);
}

void Mata::Mintermization::minterms_to_aut_nfa(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                                           const std::unordered_set<MintermizationAlgebra>& minterms)
{
    for (const auto& trans : aut.transitions) {
            // for each t=(q1,s,q2)
        const auto &symbol_part = trans.second.children[0];

        size_t symbol = 0;
        if(trans_to_var.count(&symbol_part) == 0)
            continue; // Transition had zero var so it was not added to map
        const MintermizationAlgebra &var = trans_to_var.at(&symbol_part);

        for (const auto &minterm: minterms) {
            // for each minterm x:
            if (!((var && minterm).isFalse())) {
                // if for symbol s of t is MintermizationAlgebra_s < x
                // add q1,x,q2 to transitions
                IntermediateAut::parse_transition(res, {trans.first.raw, std::to_string(symbol),
                                                        trans.second.children[1].node.raw});
            }
            symbol++;
        }
    }
}

void Mata::Mintermization::minterms_to_aut_afa(Mata::IntermediateAut& res, const Mata::IntermediateAut& aut,
                                           const std::unordered_set<MintermizationAlgebra>& minterms)
{
    for (const auto& trans : aut.transitions) {
        for (const auto& ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // for each t=(q1,s,q2)
            const auto disjunct = ds_pair.first;

            if (!trans_to_var.count(disjunct))
                continue; // Transition had zero var so it was not added to map
            const MintermizationAlgebra var = trans_to_var.at(disjunct);

            size_t symbol = 0;
            for (const auto &minterm: minterms) {
                // for each minterm x:
                if (!((var && minterm).isFalse())) {
                    // if for symbol s of t is MintermizationAlgebra_s < x
                    // add q1,x,q2 to transitions
                    const auto str_symbol = std::to_string(symbol);
                    FormulaNode node_symbol(FormulaNode::Type::OPERAND, str_symbol, str_symbol,
                                            Mata::FormulaNode::OperandType::SYMBOL);
                    if (ds_pair.second != nullptr)
                        res.add_transition(trans.first, node_symbol, *ds_pair.second);
                    else // transition without state on the right handed side
                        res.add_transition(trans.first, node_symbol);
                }
                ++symbol;
            }
        }
    }
}

Mata::IntermediateAut Mata::Mintermization::mintermize(const Mata::IntermediateAut& aut) {
    return mintermize(std::vector<const Mata::IntermediateAut *> {&aut})[0];
}

std::vector<Mata::IntermediateAut> Mata::Mintermization::mintermize(const std::vector<const Mata::IntermediateAut *> &auts)
{
    for (const Mata::IntermediateAut *aut : auts) {
        if ((!aut->is_nfa() && !aut->is_afa()) || aut->alphabet_type != IntermediateAut::AlphabetType::BITVECTOR) {
            throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
        }

        aut->is_nfa() ? trans_to_vars_nfa(*aut) : trans_to_vars_afa(*aut);
    }

    // Build minterm tree over MintermizationAlgebras
    auto minterms = compute_minterms(vars);

    std::vector<Mata::IntermediateAut> res;
    for (const Mata::IntermediateAut *aut : auts) {
        IntermediateAut mintermized_aut = *aut;
        mintermized_aut.alphabet_type = IntermediateAut::AlphabetType::EXPLICIT;
        mintermized_aut.transitions.clear();

        if (aut->is_nfa())
            minterms_to_aut_nfa(mintermized_aut, *aut, minterms);
        else if (aut->is_afa())
            minterms_to_aut_afa(mintermized_aut, *aut, minterms);

        res.push_back(mintermized_aut);
    }

    return res;
}

std::vector<Mata::IntermediateAut> Mata::Mintermization::mintermize(const std::vector<Mata::IntermediateAut> &auts) {
    std::vector<const Mata::IntermediateAut *> auts_pointers;
    for (const Mata::IntermediateAut &aut : auts) {
        auts_pointers.push_back(&aut);
    }
    return mintermize(auts_pointers);
}

Mata::Mintermization::OptionalValue Mata::Mintermization::OptionalValue::operator*(
    const Mata::Mintermization::OptionalValue& b) const {
    if (this->type == TYPE::NOTHING_E) {
        return b;
    } else if (b.type == TYPE::NOTHING_E) {
        return *this;
    } else {
        return OptionalValue{TYPE::VALUE_E, this->val && b.val };
    }
}

Mata::Mintermization::OptionalValue Mata::Mintermization::OptionalValue::operator+(
    const Mata::Mintermization::OptionalValue& b) const {
    if (this->type == TYPE::NOTHING_E) {
        return b;
    } else if (b.type == TYPE::NOTHING_E) {
        return *this;
    } else {
        return OptionalValue{TYPE::VALUE_E, this->val || b.val };
    }
}

Mata::Mintermization::OptionalValue Mata::Mintermization::OptionalValue::operator!() const {
    if (this->type == TYPE::NOTHING_E) {
        return OptionalValue();
    } else {
        return OptionalValue{TYPE::VALUE_E, !this->val };
    }
}
