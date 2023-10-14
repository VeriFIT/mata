/*
 * mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
 */

#include "mata/parser/mintermization.hh"

namespace {
    const mata::FormulaGraph* detect_state_part(const mata::FormulaGraph* node)
    {
        if (node->node.is_state())
            return node;

        std::vector<const mata::FormulaGraph *> worklist{ node};
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
                for (const mata::FormulaGraph& child : act_node->children) {
                    worklist.push_back(&child);
                }
            }
        }

        return nullptr;
    }
}

void mata::Mintermization::trans_to_bdd_nfa(const IntermediateAut &aut)
{
    assert(aut.is_nfa());

    for (const auto& trans : aut.transitions) {
        // Foreach transition create a BDD
        const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
        assert((symbol_part.node.is_operator() || symbol_part.children.empty()) &&
            "Symbol part must be either formula or single symbol");
        const BDD bdd = graph_to_bdd_nfa(symbol_part);
        if (bdd.IsZero())
            continue;
        bdds.insert(bdd);
        trans_to_bddvar[&symbol_part] = bdd;
    }
}

void mata::Mintermization::trans_to_bdd_afa(const IntermediateAut &aut)
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

        // Foreach disjunct create a BDD
        for (const DisjunctStatesPair& ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // create bdd for the whole disjunct
            const auto bdd = (ds_pair.first == ds_pair.second) ? // disjunct contains only states
                    OptionalBdd(bdd_mng.bddOne()) : // transition from state to states -> add true as symbol
                    graph_to_bdd_afa(*ds_pair.first);
            assert(bdd.type == OptionalBdd::TYPE::BDD_E);
            if (bdd.val.IsZero())
                continue;
            trans_to_bddvar[ds_pair.first] = bdd.val;
            bdds.insert(bdd.val);
        }
    }
}

std::unordered_set<BDD> mata::Mintermization::compute_minterms(const std::unordered_set<BDD>& source_bdds)
{
    std::unordered_set<BDD> stack{ bdd_mng.bddOne() };
    for (const BDD& b: source_bdds) {
        std::unordered_set<BDD> next;
        /**
         * TODO: Possible optimization - we can remember which transition belongs to the currently processed bdds
         * and mintermize automaton somehow directly here. However, it would be better to do such optimization
         * in copy of this function and this one keep clean and straightforward.
         */
        for (const auto& minterm : stack) {
            BDD b1 = minterm * b;
            if (!b1.IsZero()) {
                next.insert(b1);
            }
            BDD b0 = minterm * !b;
            if (!b0.IsZero()) {
                next.insert(b0);
            }
        }
        stack = next;
    }

    return stack;
}

mata::Mintermization::OptionalBdd mata::Mintermization::graph_to_bdd_afa(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (node.is_state())
            return OptionalBdd(OptionalBdd::TYPE::NOTHING_E);
        if (symbol_to_bddvar.count(node.name)) {
            return OptionalBdd(symbol_to_bddvar.at(node.name));
        } else {
            BDD res = (node.is_true()) ? bdd_mng.bddOne() :
                    (node.is_false() ? bdd_mng.bddZero() : bdd_mng.bddVar());
            symbol_to_bddvar[node.name] = res;
            return OptionalBdd(res);
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::AND) {
            assert(graph.children.size() == 2);
            const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
            const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OperatorType::OR) {
            assert(graph.children.size() == 2);
            const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
            const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::OperatorType::NEG) {
            assert(graph.children.size() == 1);
            const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
            return !op1;
        } else
            assert(false && "Unknown type of operation. It should conjunction, disjunction, or negation.");
    }

    assert(false);
    return {};
}

BDD mata::Mintermization::graph_to_bdd_nfa(const FormulaGraph &graph)
{
    const FormulaNode& node = graph.node;

    if (node.is_operand()) {
        if (symbol_to_bddvar.count(node.name)) {
            return symbol_to_bddvar.at(node.name);
        } else {
            BDD res = (node.is_true()) ? bdd_mng.bddOne() :
                      (node.is_false() ? bdd_mng.bddZero() : bdd_mng.bddVar());
            symbol_to_bddvar[node.name] = res;
            return res;
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::AND) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OperatorType::OR) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::OperatorType::NEG) {
            assert(graph.children.size() == 1);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            return !op1;
        } else
            assert(false);
    }

    assert(false);
    return {};
}

void mata::Mintermization::minterms_to_aut_nfa(mata::IntermediateAut& res, const mata::IntermediateAut& aut,
                                               const std::unordered_set<BDD>& minterms)
{
    for (const auto& trans : aut.transitions) {
            // for each t=(q1,s,q2)
        const auto &symbol_part = trans.second.children[0];

        size_t symbol = 0;
        if(trans_to_bddvar.count(&symbol_part) == 0)
            continue; // Transition had zero bdd so it was not added to map
        const BDD &bdd = trans_to_bddvar[&symbol_part];

        for (const auto &minterm: minterms) {
            // for each minterm x:
            if (!((bdd * minterm).IsZero())) {
                // if for symbol s of t is BDD_s < x
                // add q1,x,q2 to transitions
                IntermediateAut::parse_transition(res, {trans.first.raw, std::to_string(symbol),
                                                        trans.second.children[1].node.raw});
            }
            symbol++;
        }
    }
}

void mata::Mintermization::minterms_to_aut_afa(mata::IntermediateAut& res, const mata::IntermediateAut& aut,
                                               const std::unordered_set<BDD>& minterms)
{
    for (const auto& trans : aut.transitions) {
        for (const auto& ds_pair : lhs_to_disjuncts_and_states[&trans.first]) {
            // for each t=(q1,s,q2)
            const auto disjunct = ds_pair.first;

            if (!trans_to_bddvar.count(disjunct))
                continue; // Transition had zero bdd so it was not added to map
            const BDD &bdd = trans_to_bddvar[disjunct];

            size_t symbol = 0;
            for (const auto &minterm: minterms) {
                // for each minterm x:
                if (!((bdd * minterm).IsZero())) {
                    // if for symbol s of t is BDD_s < x
                    // add q1,x,q2 to transitions
                    const auto str_symbol = std::to_string(symbol);
                    FormulaNode node_symbol(FormulaNode::Type::OPERAND, str_symbol, str_symbol,
                                            mata::FormulaNode::OperandType::SYMBOL);
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

mata::IntermediateAut mata::Mintermization::mintermize(const mata::IntermediateAut& aut) {
    return mintermize(std::vector<const mata::IntermediateAut *> { &aut})[0];
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<const mata::IntermediateAut *> &auts)
{
    for (const mata::IntermediateAut *aut : auts) {
        if ((!aut->is_nfa() && !aut->is_afa()) || aut->alphabet_type != IntermediateAut::AlphabetType::BITVECTOR) {
            throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
        }

        aut->is_nfa() ? trans_to_bdd_nfa(*aut) : trans_to_bdd_afa(*aut);
    }

    // Build minterm tree over BDDs
    auto minterms = compute_minterms(bdds);

    std::vector<mata::IntermediateAut> res;
    for (const mata::IntermediateAut *aut : auts) {
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

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<mata::IntermediateAut> &auts) {
    std::vector<const mata::IntermediateAut *> auts_pointers;
    for (const mata::IntermediateAut &aut : auts) {
        auts_pointers.push_back(&aut);
    }
    return mintermize(auts_pointers);
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator*(
    const mata::Mintermization::OptionalBdd& b) const {
    if (this->type == TYPE::NOTHING_E) {
        return b;
    } else if (b.type == TYPE::NOTHING_E) {
        return *this;
    } else {
        return OptionalBdd{ TYPE::BDD_E, this->val * b.val };
    }
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator+(
    const mata::Mintermization::OptionalBdd& b) const {
    if (this->type == TYPE::NOTHING_E) {
        return b;
    } else if (b.type == TYPE::NOTHING_E) {
        return *this;
    } else {
        return OptionalBdd{ TYPE::BDD_E, this->val + b.val };
    }
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator!() const {
    if (this->type == TYPE::NOTHING_E) {
        return OptionalBdd(TYPE::NOTHING_E);
    } else {
        return OptionalBdd{ TYPE::BDD_E, !this->val };
    }
}
