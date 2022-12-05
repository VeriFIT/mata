/* nfa-concatenation.cc -- Concatenation of NFAs
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
#include <mata/nfa.hh>

using namespace Mata::Nfa;

namespace Mata {
namespace Nfa {

Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon,
                StateToStateMap* lhs_result_states_map, StateToStateMap* rhs_result_states_map) {
    // Compute concatenation of given automata.
    // Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.

    if (lhs.initial_states.empty() || lhs.final_states.empty() || rhs.initial_states.empty()) { return Nfa{}; }

    const unsigned long lhs_states_num{lhs.states_number() };
    const unsigned long rhs_states_num{rhs.states_number() };
    Nfa result{}; // Concatenated automaton.
    StateToStateMap lhs_result_states_map_internal{}; // Map mapping rhs states to result states.
    StateToStateMap rhs_result_states_map_internal{}; // Map mapping rhs states to result states.

    if (use_epsilon) {
        const size_t result_num_of_states{lhs_states_num + rhs_states_num};
        if (result_num_of_states == 0) { return Nfa{}; }

        // Map rhs states to result states.
        rhs_result_states_map_internal.reserve(rhs_states_num);
        Symbol result_state_index{ lhs_states_num };
        for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state) {
            rhs_result_states_map_internal.insert(std::make_pair(rhs_state, result_state_index));
            ++result_state_index;
        }

        result = Nfa();
        result.transition_relation = lhs.transition_relation;
        result.initial_states = lhs.initial_states;
        result.increase_size(result_num_of_states);

        // Add epsilon transitions connecting lhs and rhs automata.
        // The epsilon transitions lead from lhs original final states to rhs original initial states.
        for (const auto& lhs_final_state: lhs.final_states) {
            for (const auto& rhs_initial_state: rhs.initial_states) {
                result.add_trans(lhs_final_state, EPSILON,
                                 rhs_result_states_map_internal[rhs_initial_state]);
            }
        }
    } else { // !use_epsilon.
        const size_t lhs_num_of_states_in_result{ lhs_states_num - lhs.final_states.size() };
        const size_t result_num_of_states{lhs_num_of_states_in_result + rhs_states_num};
        if (result_num_of_states == 0) { return Nfa{}; }
        lhs_result_states_map_internal.reserve(lhs_num_of_states_in_result);
        result.increase_size(result_num_of_states);

        // Map both lhs and rhs states to result states.
        State result_state_index{ 0 };
        for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state) {
            if (!lhs.has_final(lhs_state)) {
                lhs_result_states_map_internal.insert(std::make_pair(lhs_state, result_state_index));
                ++result_state_index;
            }
        }

        // Map rhs states to result states.
        rhs_result_states_map_internal.reserve(rhs_states_num);
        for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state) {
            rhs_result_states_map_internal.insert(std::make_pair(rhs_state, result_state_index));
            ++result_state_index;
        }

        for (const State lhs_initial_state: lhs.initial_states) {
            if (lhs_result_states_map_internal.find(lhs_initial_state) == lhs_result_states_map_internal.end()) {
                for (const State rhs_initial_state: rhs.initial_states) {
                    lhs_result_states_map_internal.insert(
                            std::make_pair(lhs_initial_state, rhs_result_states_map_internal[rhs_initial_state])
                    );
                }
            }
        }

        // Make initial states of the result.
        for (const State lhs_initial_state: lhs.initial_states) {
            result.make_initial(lhs_result_states_map_internal[lhs_initial_state]);
        }

        // Add lhs transitions to the result.

        // Reindex all states in transitions in lhs, except for transitions concerning final states (both to and from
        //  final states).
        for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state) {
            if (!lhs.has_final(lhs_state)) {
                for (const auto& symbol_transitions:
                    lhs.get_moves_from(lhs_state)) {
                    for (const State lhs_state_to: symbol_transitions.states_to) {
                        if (!lhs.has_final(lhs_state_to)) {
                            result.add_trans(lhs_result_states_map_internal[lhs_state],
                                             symbol_transitions.symbol,
                                             lhs_result_states_map_internal[lhs_state_to]);
                        }
                    }
                }
            }
        }

        // Add lhs transitions to lhs final states to the result.
        // For all transitions to lhs final states, point them to rhs initial states.
        for (const auto& lhs_final_state: lhs.final_states) {
            for (const auto& lhs_trans_to_final_state: lhs.get_transitions_to(lhs_final_state)) {
                for (const auto& rhs_initial_state: rhs.initial_states) {
                    if (lhs_trans_to_final_state.src == lhs_trans_to_final_state.tgt) {
                        // Handle self-loops on final states as lhs final states will not be present in the result
                        //  automaton.
                        result.add_trans(rhs_result_states_map_internal[rhs_initial_state],
                                         lhs_trans_to_final_state.symb,
                                         rhs_result_states_map_internal[rhs_initial_state]);
                    } else { // All other transitions can be copied with updated initial state number.
                        result.add_trans(lhs_result_states_map_internal[lhs_trans_to_final_state.src],
                                         lhs_trans_to_final_state.symb,
                                         rhs_result_states_map_internal[rhs_initial_state]);
                    }
                }
            }
        }

        // Add lhs transitions from final states to the result.
        // For all lhs final states, copy all their transitions, except for self-loops on final states.
        for (const auto& lhs_final_state: lhs.final_states) {
            for (const auto& transitions_from_lhs_final_state:
                lhs.get_moves_from(lhs_final_state)) {
                for (const auto& lhs_state_to: transitions_from_lhs_final_state.states_to) {
                    if (lhs_state_to != lhs_final_state) { // Self-loops on final states already handled.
                        for (const auto& rhs_initial_state: rhs.initial_states) {
                            result.add_trans(rhs_result_states_map_internal[rhs_initial_state],
                                             transitions_from_lhs_final_state.symbol,
                                             lhs_result_states_map_internal[lhs_state_to]);
                        }
                    }
                }
            }
        }
    }

    // Make result final states.
    for (const auto& rhs_final_state: rhs.final_states)
    {
        result.make_final(rhs_result_states_map_internal[rhs_final_state]);
    }

    // Add rhs transitions to the result.
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state)
    {
        for (const auto& symbol_transitions: rhs.get_moves_from(rhs_state))
        {
            for (const auto& rhs_state_to: symbol_transitions.states_to)
            {
                result.add_trans(rhs_result_states_map_internal[rhs_state],
                                 symbol_transitions.symbol,
                                 rhs_result_states_map_internal[rhs_state_to]);
            }
        }
    }

    if (lhs_result_states_map != nullptr) { *lhs_result_states_map = lhs_result_states_map_internal; }
    if (rhs_result_states_map != nullptr) { *rhs_result_states_map = rhs_result_states_map_internal; }
    return result;
}

} // Nfa
} // Mata
