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

namespace Mata::Nfa {

Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon,
                StateToStateMap* lhs_result_states_map, StateToStateMap* rhs_result_states_map) {
    return Algorithms::concatenate_eps(lhs, rhs, EPSILON, use_epsilon, lhs_result_states_map, rhs_result_states_map);
}

Nfa& Nfa::concatenate(const Nfa& aut) {
    size_t n = this->size();
    auto upd_fnc = [&](State st) {
        return st + n;
    };

    this->delta.append(aut.delta.transform(upd_fnc));

    // set accepting states
    Util::SparseSet<State> new_fin{};
    new_fin.reserve(n+aut.size());
    for(const State& aut_fin : aut.final) {
        new_fin.insert(upd_fnc(aut_fin));
    }

    // connect both parts
    for(const State& ini : aut.initial) {
        const Post& ini_post = this->delta[upd_fnc(ini)];
        // is ini state also final?
        bool is_final = aut.final[ini];
        for(const State& fin : this->final) {
            if(is_final) {
                new_fin.insert(fin);
            }
            for(const Move& ini_mv : ini_post) {
                // TODO: this should be done efficiently in a delta method
                // TODO: in fact it is not efficient for now
                for(const State& dest : ini_mv.targets) {
                    this->delta.add(fin, ini_mv.symbol, dest);
                }
            }
        }
    }
    this->final = new_fin;
    return *this;
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
        result.final.insert(rhs_result_states_map_internal[rhs_final_state]);
    }

    // Add rhs transitions to the result.
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state)
    {
        for (const Move& rhs_move: rhs.get_moves_from(rhs_state))
        {
            for (const State& rhs_state_to: rhs_move.targets)
            {
                result.delta.add(rhs_result_states_map_internal[rhs_state],
                                 rhs_move.symbol,
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
} // concatenate_eps().
} // Namespace Mata::Nfa.
