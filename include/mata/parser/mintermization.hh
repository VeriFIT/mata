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

#include "inter-aut.hh"
#include "bdd-domain.hh"

namespace mata {

template <class MintermizationDomain>
class Mintermization {
private: // data types
    struct OptionalValue {
        enum class TYPE {NOTHING_E, VALUE_E};

        TYPE type;
        MintermizationDomain val;

        OptionalValue() : type(TYPE::NOTHING_E) {}
        explicit OptionalValue(const MintermizationDomain& algebra) : type(TYPE::VALUE_E), val(algebra) {}
        OptionalValue(TYPE t, const MintermizationDomain& algebra) : type(t), val(algebra) {}

        OptionalValue operator*(const OptionalValue& b) const {
            if (this->type == TYPE::NOTHING_E) {
                return b;
            } else if (b.type == TYPE::NOTHING_E) {
                return *this;
            } else {
                return OptionalValue{TYPE::VALUE_E, this->val && b.val };
            }
        }

        OptionalValue operator+(const OptionalValue& b) const {
            if (this->type == TYPE::NOTHING_E) {
                return b;
            } else if (b.type == TYPE::NOTHING_E) {
                return *this;
            } else {
                return OptionalValue{TYPE::VALUE_E, this->val || b.val };
            }
        }

        OptionalValue operator!() const {
            if (this->type == TYPE::NOTHING_E) {
                return OptionalValue();
            } else {
                return OptionalValue{TYPE::VALUE_E, !this->val };
            }
        }
    };

    using DisjunctStatesPair = std::pair<const FormulaGraph *, const FormulaGraph *>;

private: // private data members
    MintermizationDomain domain_base;
    std::unordered_map<std::string, MintermizationDomain> symbol_to_var{};
    std::unordered_map<const FormulaGraph*, MintermizationDomain> trans_to_var{};

    void trans_to_vars_nfa(const IntermediateAut &aut)
    {
        assert(aut.is_nfa());

        for (const auto& trans : aut.transitions) {
            // Foreach transition create a MintermizationDomain
            const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
            assert((symbol_part.node.is_operator() || symbol_part.children.empty()) &&
                   "Symbol part must be either formula or single symbol");
            const MintermizationDomain val = graph_to_vars_nfa(symbol_part);
            if (val.isFalse())
                continue;
            vars.insert(val);
            trans_to_var.insert(std::make_pair(&symbol_part, val));
        }
    }

    std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states{};
    std::unordered_set<MintermizationDomain> vars{}; // vars created from transitions

public:
    /**
     * Takes a set of BDDs and build a minterm tree over it.
     * The leaves of BDDs, which are minterms of input set, are returned
     * @param source_bdds BDDs for which minterms are computed
     * @return Computed minterms
     */
    std::unordered_set<MintermizationDomain> compute_minterms(
            const std::unordered_set<MintermizationDomain>& source_bdds)
    {
        std::unordered_set<MintermizationDomain> stack{ domain_base.getTrue() };
        for (const MintermizationDomain& b: source_bdds) {
            std::unordered_set<MintermizationDomain> next;
            /**
             * TODO: Possible optimization - we can remember which transition belongs to the currently processed vars
             * and mintermize automaton somehow directly here. However, it would be better to do such optimization
             * in copy of this function and this one keep clean and straightforward.
             */
            for (const auto& minterm : stack) {
                MintermizationDomain b1 = minterm && b;
                if (!b1.isFalse()) {
                    next.insert(b1);
                }
                MintermizationDomain b0 = minterm && !b;
                if (!b0.isFalse()) {
                    next.insert(b0);
                }
            }
            stack = next;
        }

        return stack;
    }

    /**
     * Transforms a graph representing formula at transition to bdd.
     * @param graph Graph to be transformed
     * @return Resulting BDD
     */
    MintermizationDomain graph_to_vars_nfa(const FormulaGraph& graph) {
        const FormulaNode& node = graph.node;

        if (node.is_operand()) {
            if (symbol_to_var.count(node.name)) {
                return symbol_to_var.at(node.name);
            } else {
                MintermizationDomain res = (node.is_true()) ? domain_base.getTrue() :
                                           (node.is_false() ? domain_base.getFalse() :
                                            domain_base.getVar());
                symbol_to_var.insert(std::make_pair(node.name, res));
                return res;
            }
        } else if (node.is_operator()) {
            if (node.operator_type == FormulaNode::OperatorType::AND) {
                assert(graph.children.size() == 2);
                const MintermizationDomain op1 = graph_to_vars_nfa(graph.children[0]);
                const MintermizationDomain op2 = graph_to_vars_nfa(graph.children[1]);
                return op1 && op2;
            } else if (node.operator_type == FormulaNode::OperatorType::OR) {
                assert(graph.children.size() == 2);
                const MintermizationDomain op1 = graph_to_vars_nfa(graph.children[0]);
                const MintermizationDomain op2 = graph_to_vars_nfa(graph.children[1]);
                return op1 || op2;
            } else if (node.operator_type == FormulaNode::OperatorType::NEG) {
                assert(graph.children.size() == 1);
                const MintermizationDomain op1 = graph_to_vars_nfa(graph.children[0]);
                return !op1;
            } else
                assert(false);
        }

        assert(false);
    }

    /**
     * Method mintermizes given automaton which has bitvector alphabet.
     * It transforms its transitions to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automaton to explicit one.
     * @param aut Automaton to be mintermized.
     * @return Mintermized automaton
     */
    mata::IntermediateAut mintermize(const mata::IntermediateAut& aut) {
        return mintermize(std::vector<const mata::IntermediateAut *>{&aut})[0];
    }

    /**
     * Methods mintermize given automata which have bitvector alphabet.
     * It transforms transitions of all automata to BDDs, then build a minterm tree over the BDDs
     * and finally transforms automata to explicit one (sharing the same minterms).
     * @param auts Automata to be mintermized.
     * @return Mintermized automata corresponding to the input autamata
     */
    std::vector<mata::IntermediateAut> mintermize(const std::vector<mata::IntermediateAut> &auts) {
        std::vector<const mata::IntermediateAut *> auts_pointers;
        for (const mata::IntermediateAut &aut : auts) {
            auts_pointers.push_back(&aut);
        }
        return mintermize(auts_pointers);
    }

    std::vector<mata::IntermediateAut> mintermize(const std::vector<const mata::IntermediateAut *> &auts) {
            for (const mata::IntermediateAut *aut : auts) {
                if (!aut->is_nfa() || aut->alphabet_type != IntermediateAut::AlphabetType::BITVECTOR) {
                    throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
                }

                trans_to_vars_nfa(*aut);
            }

            // Build minterm tree over MintermizationDomains
            auto minterms = compute_minterms(vars);

            std::vector<mata::IntermediateAut> res;
            for (const mata::IntermediateAut *aut : auts) {
                IntermediateAut mintermized_aut = *aut;
                mintermized_aut.alphabet_type = IntermediateAut::AlphabetType::EXPLICIT;
                mintermized_aut.transitions.clear();

                minterms_to_aut_nfa(mintermized_aut, *aut, minterms);

                res.push_back(mintermized_aut);
            }

            return res;
    }

    /**
     * The method performs the mintermization over @aut with given @minterms.
     * It is method specialized for NFA.
     * @param res The resulting mintermized automaton
     * @param aut Automaton to be mintermized
     * @param minterms Set of minterms for mintermization
     */
    void minterms_to_aut_nfa(mata::IntermediateAut& res, const mata::IntermediateAut& aut,
                             const std::unordered_set<MintermizationDomain>& minterms)
    {
        for (const auto& trans : aut.transitions) {
            // for each t=(q1,s,q2)
            const auto &symbol_part = trans.second.children[0];

            size_t symbol = 0;
            if(trans_to_var.count(&symbol_part) == 0)
                continue; // Transition had zero var so it was not added to map
            const MintermizationDomain &var = trans_to_var.at(&symbol_part);

            for (const auto &minterm: minterms) {
                // for each minterm x:
                if (!((var && minterm).isFalse())) {
                    // if for symbol s of t is MintermizationDomain_s < x
                    // add q1,x,q2 to transitions
                    IntermediateAut::parse_transition(res, {trans.first.raw, std::to_string(symbol),
                                                            trans.second.children[1].node.raw});
                }
                symbol++;
            }
        }
    }

    Mintermization() : domain_base(), symbol_to_var{}, trans_to_var() {
    }
}; // class Mintermization.
} // namespace mata
#endif //MATA_MINTERM_HH
