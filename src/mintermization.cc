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

