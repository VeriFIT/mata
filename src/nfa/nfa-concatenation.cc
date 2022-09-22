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

namespace Mata
{
namespace Nfa
{

/**
 * Class executing a concatenation of two NFAs.
 */
class Concatenation
{
public:
    /**
     * Initialize and compute concatenation of two NFAs.
     *
     * Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.
     * @param[in] lhs First NFA to concatenate.
     * @param[in] rhs Second NFA to concatenate.
     */
    Concatenation(const Nfa& lhs, const Nfa& rhs)
            : lhs(lhs), rhs(rhs), lhs_states_num(lhs.get_num_of_states()), rhs_states_num(rhs.get_num_of_states()) {
        if (lhs.initialstates.empty() || lhs.finalstates.empty() || rhs.initialstates.empty()) { return; }
        concatenate();
    }

    /**
     * Initialize and compute concatenation of two NFAs concatenating over epsilon transitions.
     *
     * Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.
     * @param[in] lhs First NFA to concatenate.
     * @param[in] rhs Second NFA to concatenate.
     */
    Concatenation(const Nfa& lhs, const Nfa& rhs, Symbol epsilon)
            : lhs(lhs), rhs(rhs), lhs_states_num(lhs.get_num_of_states()), rhs_states_num(rhs.get_num_of_states()),
              epsilon(epsilon) {
        if (lhs.initialstates.empty() || lhs.finalstates.empty() || rhs.initialstates.empty()) { return; }
        concatenate_over_epsilon();
    }

    /**
     * Get result of concatenation.
     * @return Concatenated automaton.
     */
    const Nfa& get_result() { return result; }

    /**
     * Get @c lhs to @c result states map.
     * @return @c lhs to @c result states map.
     */
    const StateToStateMap& get_lhs_result_states_map() { return lhs_result_states_map; }

    /**
     * Get @c rhs to @c result states map.
     * @return @c rhs to @c result states map.
     */
    const StateToStateMap& get_rhs_result_states_map() { return rhs_result_states_map; }

private:
    const Nfa& lhs; ///< First automaton to concatenate.
    const Nfa& rhs; ///< Second automaton to concatenate.
    const unsigned long lhs_states_num; ///< Number of states in @c lhs.
    const unsigned long rhs_states_num; ///< Number of states in @c rhs.
    const Symbol epsilon{}; ///< Symbol to use as an epsilon symbol to concatenate with.
    Nfa result{}; ///< Concatenated automaton.
    StateToStateMap lhs_result_states_map{}; ///< Map mapping @c lhs states to @c result states.
    StateToStateMap rhs_result_states_map{}; ///< Map mapping @c rhs states to @c result states.

    /**
     * Compute concatenation of given automata.
     */
    void concatenate()
    {
        const size_t lhs_num_of_states_in_result{ lhs_states_num - lhs.finalstates.size() };
        const size_t result_num_of_states{lhs_num_of_states_in_result + rhs_states_num};
        if (result_num_of_states == 0) { return; }
        lhs_result_states_map.reserve(lhs_num_of_states_in_result);
        result.increase_size(result_num_of_states);
        map_states_to_result_states();
        make_initial_states();
        add_lhs_transitions();
        make_final_states();
        add_rhs_transitions();
    }

    /**
     * Compute concatenation of given automata concatenating over epsilon transitions.
     */
    void concatenate_over_epsilon() {
        const size_t result_num_of_states{lhs_states_num + rhs_states_num};
        if (result_num_of_states == 0) { return; }
        map_rhs_states_to_result_states(lhs_states_num);
        result = Nfa(lhs.transitionrelation, lhs.initialstates);
        result.increase_size(result_num_of_states);
        add_epsilon_transitions();
        make_final_states();
        add_rhs_transitions();
    }

    /**
     * Add epsilon transitions connecting @c lhs and @c rhs automata.
     *
     * The epsilon transitions lead from @c lhs original final states to @c rhs original initial states.
     */
    void add_epsilon_transitions() {
        for (const auto& lhs_final_state: lhs.finalstates) {
            for (const auto& rhs_initial_state: rhs.initialstates) {
                result.add_trans(lhs_final_state, epsilon, rhs_result_states_map[rhs_initial_state]);
            }
        }
    }

    /**
     * Map @c lhs and @c rhs states to @c result states.
     */
    void map_states_to_result_states() {
        State result_state_index{ 0 };
        for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state) {
            if (!lhs.has_final(lhs_state)) {
                lhs_result_states_map.insert(std::make_pair(lhs_state, result_state_index));
                ++result_state_index;
            }
        }
        map_rhs_states_to_result_states(result_state_index);

        for (const State lhs_initial_state: lhs.initialstates) {
            if (lhs_result_states_map.find(lhs_initial_state) == lhs_result_states_map.end()) {
                for (const State rhs_initial_state: rhs.initialstates) {
                    lhs_result_states_map.insert(std::make_pair(lhs_initial_state, rhs_result_states_map[rhs_initial_state]));
                }
            }
        }
    }

    /**
     * Map @c lhs and @c rhs states to @c result states.
     */
    void map_rhs_states_to_result_states(State result_state_index) {
        rhs_result_states_map.reserve(rhs_states_num);
        for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state) {
            rhs_result_states_map.insert(std::make_pair(rhs_state, result_state_index));
            ++result_state_index;
        }
    }

    /**
     * Make @c result initial states.
     */
    void make_initial_states() {
        for (const State lhs_initial_state: lhs.initialstates) {
            result.make_initial(lhs_result_states_map[lhs_initial_state]);
        }
    }

    /**
     * Make @c result final states.
     */
    void make_final_states()
    {
        for (const auto& rhs_final_state: rhs.finalstates)
        {
            result.make_final(rhs_result_states_map[rhs_final_state]);
        }
    }

    /**
     * Add @c rhs transitions to the @c result.
     */
    void add_rhs_transitions()
    {
        for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state)
        {
            for (const auto& symbol_transitions: rhs.get_transitions_from(rhs_state))
            {
                for (const auto& rhs_state_to: symbol_transitions.states_to)
                {
                    result.add_trans(rhs_result_states_map[rhs_state],
                                     symbol_transitions.symbol,
                                     rhs_result_states_map[rhs_state_to]);
                }
            }
        }
    }

    /**
     * Add @c lhs transitions from final states to the @c result.
     */
    void add_lhs_final_states_transitions()
    {
        // For all lhs final states, copy all their transitions, except for self-loops on final states.
        for (const auto& lhs_final_state: lhs.finalstates)
        {
            for (const auto& transitions_from_lhs_final_state: lhs.get_transitions_from(lhs_final_state))
            {
                for (const auto& lhs_state_to: transitions_from_lhs_final_state.states_to) {
                    if (lhs_state_to != lhs_final_state) // Self-loops on final states already handled.
                    {
                        for (const auto& rhs_initial_state: rhs.initialstates) {
                            result.add_trans(rhs_result_states_map[rhs_initial_state],
                                             transitions_from_lhs_final_state.symbol,
                                             lhs_result_states_map[lhs_state_to]);
                        }
                    }
                }
            }
        }
    }

    /**
     * Add @c lhs transitions to @c lhs final states to the @c result.
     */
    void add_lhs_transitions_to_final_states()
    {
        // For all transitions to lhs final states, point them to rhs initial states.
        for (const auto& lhs_final_state: lhs.finalstates)
        {
            for (const auto& lhs_trans_to_final_state: lhs.get_transitions_to(lhs_final_state))
            {
                for (const auto& rhs_initial_state: rhs.initialstates)
                {
                    if (lhs_trans_to_final_state.src == lhs_trans_to_final_state.tgt)
                    {
                        // Handle self-loops on final states as lhs final states will not be present in the result automaton.
                        result.add_trans(rhs_result_states_map[rhs_initial_state], lhs_trans_to_final_state.symb,
                                         rhs_result_states_map[rhs_initial_state]);
                    }
                    else // All other transitions can be copied with updated initial state number.
                    {
                        result.add_trans(lhs_result_states_map[lhs_trans_to_final_state.src], lhs_trans_to_final_state.symb,
                                         rhs_result_states_map[rhs_initial_state]);
                    }
                }
            }
        }
    }

    /**
     * Add @c lhs transitions from @c lhs final states.
     */
    void add_lhs_non_final_states_transitions()
    {
        // Reindex all states in transitions in lhs, except for transitions concerning final states (both to and form final states).
        for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state)
        {
            if (!lhs.has_final(lhs_state))
            {
                for (const auto& symbol_transitions: lhs.get_transitions_from(lhs_state))
                {
                    for (const State lhs_state_to: symbol_transitions.states_to)
                    {
                        if (!lhs.has_final(lhs_state_to))
                        {
                            result.add_trans(lhs_result_states_map[lhs_state],
                                             symbol_transitions.symbol,
                                             lhs_result_states_map[lhs_state_to]);
                        }
                    }
                }
            }
        }
    }

    /**
     * Add @c lhs transitions to the @c result.
     */
    void add_lhs_transitions()
    {
        add_lhs_non_final_states_transitions();
        add_lhs_transitions_to_final_states();
        add_lhs_final_states_transitions();
    }
}; // Concatenation

void concatenate(Nfa* res, const Nfa& lhs, const Nfa& rhs)
{
    *res = Concatenation(lhs, rhs).get_result();
}

Nfa concatenate(const Nfa& lhs, const Nfa& rhs)
{
    return Concatenation(lhs, rhs).get_result();
}

void concatenate_over_epsilon(Nfa* res, const Nfa& lhs, const Nfa& rhs, Symbol epsilon) {
    *res = Concatenation(lhs, rhs, epsilon).get_result();
}

Nfa concatenate_over_epsilon(const Nfa& lhs, const Nfa& rhs, Symbol epsilon) {
    return Concatenation(lhs, rhs, epsilon).get_result();
}

} // Nfa
} // Mata
