/* nfa-intersection.cc -- Intersection of NFAs
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

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::nfa;

namespace {

/**
 * Add transition to the product.
 * @param[out] product Created product automaton.
 * @param[out] product_map Created product map.
 * @param[in] pair_to_process Currently processed pair of original states.
 * @param[in] intersection_transition State transitions to add to the product.
 */
void add_product_transition(Nfa& product, std::unordered_map<std::pair<State,State>, State>& product_map,
                            const std::pair<State,State>& pair_to_process, SymbolPost& intersection_transition) {
    if (intersection_transition.empty()) { return; }

    auto& intersect_state_transitions{ product.delta.mutable_state_post(product_map[pair_to_process]) };
    auto intersection_move_iter{ intersect_state_transitions.find(intersection_transition) };
    if (intersection_move_iter == intersect_state_transitions.end()) {
        intersect_state_transitions.insert(intersection_transition);
    } else {
        // Product already has some target states for the given symbol from the current product state.
        intersection_move_iter->insert(intersection_transition.targets);
    }
}

/**
 * Create product state and its transitions.
 * @param[out] product Created product automaton.
 * @param[out] product_map Created product map.
 * @param[out] pairs_to_process Set of product states to process
 * @param[in] lhs_state_to Target state in NFA @c lhs.
 * @param[in] rhs_state_to Target state in NFA @c rhs.
 * @param[out] intersect_transitions Transitions of the product state.
 */
void create_product_state_and_trans(
            Nfa& product,
            std::unordered_map<std::pair<State,State>, State>& product_map,
            const Nfa& lhs,
            const Nfa& rhs,
            std::deque<std::pair<State,State>>& pairs_to_process,
            const State lhs_state_to,
            const State rhs_state_to,
            SymbolPost& intersect_transitions
) {
    const std::pair<State,State> intersect_state_pair_to(lhs_state_to, rhs_state_to);
    State intersect_state_to;
    if (product_map.find(intersect_state_pair_to) == product_map.end()) {
        intersect_state_to = product.add_state();
        product_map[intersect_state_pair_to] = intersect_state_to;
        pairs_to_process.push_back(intersect_state_pair_to);

        if (lhs.final[lhs_state_to] && rhs.final[rhs_state_to]) {
            product.final.insert(intersect_state_to);
        }
    } else {
        intersect_state_to = product_map[intersect_state_pair_to];
    }
    intersect_transitions.insert(intersect_state_to);
}

} // Anonymous namespace.

namespace mata::nfa {

Nfa intersection(const Nfa& lhs, const Nfa& rhs, bool preserve_epsilon,
                 std::unordered_map<std::pair<State,State>, State> *prod_map) {

    const std::set<Symbol> epsilons({EPSILON});
    return algorithms::intersection_eps(lhs, rhs, preserve_epsilon, epsilons, prod_map);
}

Nfa mata::nfa::algorithms::intersection_eps(
        const Nfa& lhs, const Nfa& rhs, bool preserve_epsilon, const std::set<Symbol>& epsilons,
        std::unordered_map<std::pair<State, State>, State> *prod_map) {
    Nfa product{}; // Product of the intersection.
    // Product map for the generated intersection mapping original state pairs to new product states.
    std::unordered_map<std::pair<State,State>, State> product_map{};
    std::pair<State,State> pair_to_process{}; // State pair of original states currently being processed.
    std::deque<std::pair<State,State>> pairs_to_process{}; // Set of state pairs of original states to process.

    // Initialize pairs to process with initial state pairs.
    for (const State lhs_initial_state : lhs.initial) {
        for (const State rhs_initial_state : rhs.initial) {
            // Update product with initial state pairs.
            const std::pair<State,State> this_and_other_initial_state_pair(lhs_initial_state, rhs_initial_state);
            const State new_intersection_state = product.add_state();

            product_map[this_and_other_initial_state_pair] = new_intersection_state;
            pairs_to_process.push_back(this_and_other_initial_state_pair);

            product.initial.insert(new_intersection_state);
            if (lhs.final[lhs_initial_state] && rhs.final[rhs_initial_state]) {
                product.final.insert(new_intersection_state);
            }
        }
    }

    while (!pairs_to_process.empty()) {
        pair_to_process = pairs_to_process.back();
        pairs_to_process.pop_back();
        // Compute classic product for current state pair.

        mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
        mata::utils::push_back(sync_iterator, lhs.delta[pair_to_process.first]);
        mata::utils::push_back(sync_iterator, rhs.delta[pair_to_process.second]);

        while (sync_iterator.advance()) {
            std::vector<StatePost::const_iterator> moves = sync_iterator.get_current();
            assert(moves.size() == 2); // One move per state in the pair.

            // Compute product for state transitions with same symbols.
            // Find all transitions that have the same symbol for first and the second state in the pair_to_process.
            // Create transition from the pair_to_process to all pairs between states to which first transition goes
            //  and states to which second one goes.
            SymbolPost intersection_transition{ moves[0]->symbol };
            for (const State this_state_to: moves[0]->targets)
            {
                for (const State other_state_to: moves[1]->targets)
                {
                    create_product_state_and_trans(
                            product, product_map, lhs, rhs, pairs_to_process,
                            this_state_to, other_state_to, intersection_transition
                    );
                }
            }
            add_product_transition(product, product_map, pair_to_process, intersection_transition);
        }

        if (preserve_epsilon) {
            // Add transitions of the current state pair for an epsilon preserving product.

            // Check for lhs epsilon transitions.
            const StatePost& lhs_post{ lhs.delta[pair_to_process.first] };
            if (!lhs_post.empty()) {
                const auto& lhs_state_last_transitions{ lhs_post.back() };
                if (epsilons.find(lhs_state_last_transitions.symbol) != epsilons.end()) {
                    // Compute product for state transitions with lhs state epsilon transition.
                    // Create transition from the pair_to_process to all pairs between states to which first transition
                    //  goes and states to which second one goes.
                    SymbolPost intersection_transition{ lhs_state_last_transitions.symbol };
                    for (const State lhs_state_to: lhs_state_last_transitions.targets) {
                        create_product_state_and_trans(product, product_map, lhs, rhs, pairs_to_process,
                                                       lhs_state_to, pair_to_process.second,
                                                       intersection_transition);
                    }
                    add_product_transition(product, product_map, pair_to_process, intersection_transition);
                }
            }

            // Check for rhs epsilon transitions in case only rhs has any transitions and add them.
            const StatePost& rhs_post{ rhs.delta[pair_to_process.second]};
            if (!rhs_post.empty()) {
                const auto& rhs_state_last_transitions{ rhs_post.back()};
                if (epsilons.find(rhs_state_last_transitions.symbol) != epsilons.end()) {
                    // Compute product for state transitions with rhs state epsilon transition.
                    // Create transition from the pair_to_process to all pairs between states to which first transition
                    //  goes and states to which second one goes.
                    SymbolPost intersection_transition{ rhs_state_last_transitions.symbol };
                    for (const State rhs_state_to: rhs_state_last_transitions.targets) {
                        create_product_state_and_trans(product, product_map, lhs, rhs, pairs_to_process,
                                                       pair_to_process.first, rhs_state_to,
                                                       intersection_transition);
                    }
                    add_product_transition(product, product_map, pair_to_process, intersection_transition);
                }
            }
        }
    }

    if (prod_map != nullptr) { *prod_map = product_map; }
    return product;
} // intersection().

} // namespace mata::nfa.
