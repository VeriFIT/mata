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
#include <mata/nfa-algorithms.hh>

using namespace Mata::Nfa;

namespace Mata {
namespace Nfa {

Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon,
                StateToStateMap* lhs_result_states_map, StateToStateMap* rhs_result_states_map) {
    return Algorithms::concatenate_eps(lhs, rhs, EPSILON, use_epsilon, lhs_result_states_map, rhs_result_states_map);
}

Nfa Algorithms::concatenate_eps(const Nfa& lhs, const Nfa& rhs, const Symbol& epsilon, bool use_epsilon,
                StateToStateMap* lhs_result_states_map, StateToStateMap* rhs_result_states_map) {
    // Compute concatenation of given automata.
    // Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.

    if (lhs.size() == 0 || rhs.size() == 0 || lhs.initial.empty() || lhs.final.empty() ||
        rhs.initial.empty() || rhs.final.empty()) {
        return Nfa{};
    }

    const unsigned long lhs_states_num{lhs.size() };
    const unsigned long rhs_states_num{rhs.size() };
    Nfa result{}; // Concatenated automaton.
    StateToStateMap lhs_result_states_map_internal{}; // Map mapping rhs states to result states.
    StateToStateMap rhs_result_states_map_internal{}; // Map mapping rhs states to result states.
    const bool lhs_accepts_empty_string{ is_in_lang(lhs, Run{{}, {}}) };
    const bool rhs_accepts_empty_string{ is_in_lang(rhs, Run{{}, {}}) };

    const size_t result_num_of_states{lhs_states_num + rhs_states_num};
    if (result_num_of_states == 0) { return Nfa{}; }

    // Map lhs states to result states.
    lhs_result_states_map_internal.reserve(lhs_states_num);
    Symbol result_state_index{ 0 };
    for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state) {
        lhs_result_states_map_internal.insert(std::make_pair(lhs_state, result_state_index));
        ++result_state_index;
    }
    // Map rhs states to result states.
    rhs_result_states_map_internal.reserve(rhs_states_num);
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state) {
        rhs_result_states_map_internal.insert(std::make_pair(rhs_state, result_state_index));
        ++result_state_index;
    }

    result = Nfa();
    result.delta = lhs.delta;
    result.initial = lhs.initial;
    if (rhs_accepts_empty_string) {
        result.final = lhs.final;
    }
    result.add_state(result_num_of_states-1);

    // Add epsilon transitions connecting lhs and rhs automata.
    // The epsilon transitions lead from lhs original final states to rhs original initial states.
    for (const auto& lhs_final_state: lhs.final) {
        for (const auto& rhs_initial_state: rhs.initial) {
            result.delta.add(lhs_final_state, epsilon,
                             rhs_result_states_map_internal[rhs_initial_state]);
        }
    }

    // Make result final states.
    for (const auto& rhs_final_state: rhs.final)
    {
        result.final.add(rhs_result_states_map_internal[rhs_final_state]);
    }

    // Add rhs transitions to the result.
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state)
    {
        for (const auto& symbol_transitions: rhs.get_moves_from(rhs_state))
        {
            for (const auto& rhs_state_to: symbol_transitions.targets)
            {
                result.delta.add(rhs_result_states_map_internal[rhs_state],
                                 symbol_transitions.symbol,
                                 rhs_result_states_map_internal[rhs_state_to]);
            }
        }
    }

    if (!use_epsilon) {
        result.remove_epsilon();
    }
    if (lhs_result_states_map != nullptr) { *lhs_result_states_map = lhs_result_states_map_internal; }
    if (rhs_result_states_map != nullptr) { *rhs_result_states_map = rhs_result_states_map_internal; }
    return result;
}

} // Nfa
} // Mata
