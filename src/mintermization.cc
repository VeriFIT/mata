/*
 * mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
 */

#include <cassert>

#include "mata/parser/mintermization.hh"

#include <ranges>

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
        bdds_.insert(bdd);
        trans_to_bddvar_[&symbol_part] = bdd;
    }
}

void mata::Mintermization::trans_to_bdd_afa(const IntermediateAut &aut)
{
    assert(aut.is_afa());

    for (const auto& [formula_node, formula_graph] : aut.transitions) {
        lhs_to_disjuncts_and_states_[&formula_node] = std::vector<DisjunctStatesPair>();
        if (formula_graph.node.is_state()) { // node from state to state
            lhs_to_disjuncts_and_states_[&formula_node].emplace_back(&formula_graph, &formula_graph);
        }
        // split transition to disjuncts
        const FormulaGraph *act_graph = &formula_graph;

        if (!formula_graph.node.is_state() && act_graph->node.is_operator() && act_graph->node.operator_type != FormulaNode::OperatorType::Or) // there are no disjuncts
            lhs_to_disjuncts_and_states_[&formula_node].emplace_back(act_graph, detect_state_part(
                    act_graph));
        else if (!formula_graph.node.is_state()) {
            while (act_graph->node.is_operator() && act_graph->node.operator_type == FormulaNode::OperatorType::Or) {
                // map lhs to disjunct and its state formula. The content of disjunct is right son of actual graph
                // since the left one is a rest of formula
                lhs_to_disjuncts_and_states_[&formula_node].emplace_back(&act_graph->children[1],
                                                                                       detect_state_part(
                                                                                           &act_graph->children[1]));
                act_graph = &(act_graph->children.front());
            }

            // take care of last disjunct
            lhs_to_disjuncts_and_states_[&formula_node].emplace_back(act_graph,
                                                                                   detect_state_part(
                                                                                           act_graph));
        }

        // Foreach disjunct create a BDD
        for (const auto& [disjunct_lhs, disjunct_rhs] : lhs_to_disjuncts_and_states_[&formula_node]) {
            // create bdd for the whole disjunct
            const auto bdd = (disjunct_lhs == disjunct_rhs)
                                 ? // disjunct contains only states
                                 OptionalBdd(bdd_mng_.bddOne())
                                 : // transition from state to states -> add true as symbol
                                 graph_to_bdd_afa(*disjunct_lhs);
            assert(bdd.type == OptionalBdd::Type::BddE);
            if (bdd.val.IsZero())
                continue;
            trans_to_bddvar_[disjunct_lhs] = bdd.val;
            bdds_.insert(bdd.val);
        }
    }
}

std::unordered_set<BDD> mata::Mintermization::compute_minterms(const std::unordered_set<BDD>& source_bdds) const {
    std::unordered_set<BDD> stack{ bdd_mng_.bddOne() };
    for (const BDD& b: source_bdds) {
        std::unordered_set<BDD> next;
        /**
         * TODO: Possible optimization - we can remember which transition belongs to the currently processed bdds
         * and mintermize automaton somehow directly here. However, it would be better to do such optimization
         * in copy of this function and this one keep clean and straightforward.
         */
        for (const auto& minterm : stack) {
            if (BDD b1 = minterm * b; !b1.IsZero()) { next.insert(b1); }
            if (BDD b0 = minterm * !b; !b0.IsZero()) { next.insert(b0); }
        }
        stack = next;
    }

    return stack;
}

mata::Mintermization::OptionalBdd mata::Mintermization::graph_to_bdd_afa(const FormulaGraph &graph)
{
    if (const FormulaNode& node = graph.node; node.is_operand()) {
        if (node.is_state())
            return OptionalBdd(OptionalBdd::Type::NothingE);
        if (symbol_to_bddvar_.contains(node.name)) {
            return OptionalBdd(symbol_to_bddvar_.at(node.name));
        } else {
            const BDD res = (node.is_true())
                                ? bdd_mng_.bddOne()
                                : (node.is_false() ? bdd_mng_.bddZero() : bdd_mng_.bddVar());
            symbol_to_bddvar_[node.name] = res;
            return OptionalBdd(res);
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::And) {
            assert(graph.children.size() == 2);
            const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
            const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OperatorType::Or) {
            assert(graph.children.size() == 2);
            const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
            const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::OperatorType::Neg) {
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
    if (const FormulaNode& node = graph.node; node.is_operand()) {
        if (symbol_to_bddvar_.contains(node.name)) { return symbol_to_bddvar_.at(node.name); } else {
            BDD res = (node.is_true()) ? bdd_mng_.bddOne() : (node.is_false() ? bdd_mng_.bddZero() : bdd_mng_.bddVar());
            symbol_to_bddvar_[node.name] = res;
            return res;
        }
    } else if (node.is_operator()) {
        if (node.operator_type == FormulaNode::OperatorType::And) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
            return op1 * op2;
        } else if (node.operator_type == FormulaNode::OperatorType::Or) {
            assert(graph.children.size() == 2);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
            return op1 + op2;
        } else if (node.operator_type == FormulaNode::OperatorType::Neg) {
            assert(graph.children.size() == 1);
            const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
            return !op1;
        } else
            assert(false);
    }

    assert(false);
    return {};
}

void mata::Mintermization::minterms_to_aut_nfa(IntermediateAut& res, const IntermediateAut& aut,
                                               const std::unordered_set<BDD>& minterms)
{
    for (const auto& [formula_node, formula_graph] : aut.transitions) {
            // for each t=(q1,s,q2)
        const auto &symbol_part = formula_graph.children[0];

        size_t symbol = 0;
        if(!trans_to_bddvar_.contains(&symbol_part))
            continue; // Transition had zero bdd so it was not added to map
        const BDD &bdd = trans_to_bddvar_[&symbol_part];

        for (const auto &minterm: minterms) {
            // for each minterm x:
            if (!((bdd * minterm).IsZero())) {
                // if for symbol s of t is BDD_s < x
                // add q1,x,q2 to transitions
                IntermediateAut::parse_transition(res, {formula_node.raw, std::to_string(symbol),
                                                        formula_graph.children[1].node.raw});
            }
            symbol++;
        }
    }
}

void mata::Mintermization::minterms_to_aut_afa(IntermediateAut& res, const IntermediateAut& aut,
                                               const std::unordered_set<BDD>& minterms)
{
    for (const auto& formula_node : aut.transitions | std::views::keys) {
        for (const auto& [disjunct, formula_graph] : lhs_to_disjuncts_and_states_[&formula_node]) {
            // for each t=(q1,s,q2)
            if (!trans_to_bddvar_.contains(disjunct))
                continue; // Transition had zero bdd so it was not added to map
            const BDD &bdd = trans_to_bddvar_[disjunct];

            size_t symbol = 0;
            for (const auto &minterm: minterms) {
                // for each minterm x:
                if (!((bdd * minterm).IsZero())) {
                    // if for symbol s of t is BDD_s < x
                    // add q1,x,q2 to transitions
                    const auto str_symbol = std::to_string(symbol);
                    FormulaNode node_symbol(FormulaNode::Type::Operand, str_symbol, str_symbol,
                                            FormulaNode::OperandType::Symbol);
                    if (formula_graph != nullptr)
                        res.add_transition(formula_node, node_symbol, *formula_graph);
                    else // transition without state on the right-handed side
                        res.add_transition(formula_node, node_symbol);
                }
                ++symbol;
            }
        }
    }
}

mata::IntermediateAut mata::Mintermization::mintermize(const IntermediateAut& aut) {
    return mintermize(std::vector<const IntermediateAut *> { &aut})[0];
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<const IntermediateAut *> &auts)
{
    for (const IntermediateAut *aut : auts) {
        if ((!aut->is_nfa() && !aut->is_afa()) || aut->alphabet_type != IntermediateAut::AlphabetType::Bitvector) {
            throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
        }

        aut->is_nfa() ? trans_to_bdd_nfa(*aut) : trans_to_bdd_afa(*aut);
    }

    // Build minterm tree over BDDs
    const auto minterms = compute_minterms(bdds_);

    std::vector<IntermediateAut> res;
    for (const IntermediateAut *aut : auts) {
        IntermediateAut mintermized_aut = *aut;
        mintermized_aut.alphabet_type = IntermediateAut::AlphabetType::Explicit;
        mintermized_aut.transitions.clear();

        if (aut->is_nfa())
            minterms_to_aut_nfa(mintermized_aut, *aut, minterms);
        else if (aut->is_afa())
            minterms_to_aut_afa(mintermized_aut, *aut, minterms);

        res.push_back(mintermized_aut);
    }

    return res;
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<IntermediateAut> &auts) {
    std::vector<const IntermediateAut *> auts_pointers;
    for (const IntermediateAut &aut : auts) {
        auts_pointers.push_back(&aut);
    }
    return mintermize(auts_pointers);
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator*(
    const OptionalBdd& b) const {
    if (this->type == Type::NothingE) {
        return b;
    } else if (b.type == Type::NothingE) {
        return *this;
    } else {
        return OptionalBdd{ Type::BddE, this->val * b.val };
    }
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator+(
    const OptionalBdd& b) const {
    if (this->type == Type::NothingE) {
        return b;
    } else if (b.type == Type::NothingE) {
        return *this;
    } else {
        return OptionalBdd{ Type::BddE, this->val + b.val };
    }
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator!() const {
    if (this->type == Type::NothingE) {
        return OptionalBdd(Type::NothingE);
    } else {
        return OptionalBdd{ Type::BddE, !this->val };
    }
}
