/* nfa-concatenation.cc -- Concatenation of NFAs
 */

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::nfa;

namespace mata::nfa {

Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon,
                StateRenaming* lhs_state_renaming, StateRenaming* rhs_state_renaming) {
    return algorithms::concatenate_eps(lhs, rhs, EPSILON, use_epsilon, lhs_state_renaming, rhs_state_renaming);
}

Nfa& Nfa::concatenate(const Nfa& aut) {
    size_t n = this->num_of_states();
    auto upd_fnc = [&](State st) {
        return st + n;
    };

    // copy the information about aut to save the case when this is the same object as aut.
    utils::SparseSet<mata::nfa::State> aut_initial = aut.initial;
    utils::SparseSet<mata::nfa::State> aut_final = aut.final;
    size_t aut_n = aut.num_of_states();

    this->delta.allocate(n);
    this->delta.append(aut.delta.renumber_targets(upd_fnc));

    // set accepting states
    utils::SparseSet<State> new_fin{};
    new_fin.reserve(n+aut_n);
    for(const State& aut_fin : aut_final) {
        new_fin.insert(upd_fnc(aut_fin));
    }

    // connect both parts
    for(const State& ini : aut_initial) {
        const StatePost& ini_post = this->delta[upd_fnc(ini)];
        // is ini state also final?
        bool is_final = aut_final[ini];
        for(const State& fin : this->final) {
            if(is_final) {
                new_fin.insert(fin);
            }
            for(const SymbolPost& ini_mv : ini_post) {
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

Nfa algorithms::concatenate_eps(const Nfa& lhs, const Nfa& rhs, const Symbol& epsilon, bool use_epsilon,
                                StateRenaming* lhs_state_renaming, StateRenaming* rhs_state_renaming) {
    // Compute concatenation of given automata.
    // Concatenation will proceed in the order of the passed automata: Result is 'lhs . rhs'.

    if (lhs.num_of_states() == 0 || rhs.num_of_states() == 0 || lhs.initial.empty() || lhs.final.empty() ||
        rhs.initial.empty() || rhs.final.empty()) {
        return Nfa{};
    }

    const unsigned long lhs_states_num{lhs.num_of_states() };
    const unsigned long rhs_states_num{rhs.num_of_states() };
    Nfa result{}; // Concatenated automaton.
    StateRenaming _lhs_states_renaming{}; // Map mapping rhs states to result states.
    StateRenaming _rhs_states_renaming{}; // Map mapping rhs states to result states.

    const size_t result_num_of_states{lhs_states_num + rhs_states_num};
    if (result_num_of_states == 0) { return Nfa{}; }

    // Map lhs states to result states.
    _lhs_states_renaming.reserve(lhs_states_num);
    Symbol result_state_index{ 0 };
    for (State lhs_state{ 0 }; lhs_state < lhs_states_num; ++lhs_state) {
        _lhs_states_renaming.insert(std::make_pair(lhs_state, result_state_index));
        ++result_state_index;
    }
    // Map rhs states to result states.
    _rhs_states_renaming.reserve(rhs_states_num);
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state) {
        _rhs_states_renaming.insert(std::make_pair(rhs_state, result_state_index));
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
                             _rhs_states_renaming[rhs_initial_state]);
        }
    }

    // Make result final states.
    for (const auto& rhs_final_state: rhs.final)
    {
        result.final.insert(_rhs_states_renaming[rhs_final_state]);
    }

    // Add rhs transitions to the result.
    for (State rhs_state{ 0 }; rhs_state < rhs_states_num; ++rhs_state)
    {
        for (const SymbolPost& rhs_move: rhs.delta.state_post(rhs_state))
        {
            for (const State& rhs_state_to: rhs_move.targets)
            {
                result.delta.add(_rhs_states_renaming[rhs_state],
                                 rhs_move.symbol,
                                 _rhs_states_renaming[rhs_state_to]);
            }
        }
    }

    if (!use_epsilon) {
        result.remove_epsilon();
    }
    if (lhs_state_renaming != nullptr) { *lhs_state_renaming = _lhs_states_renaming; }
    if (rhs_state_renaming != nullptr) { *rhs_state_renaming = _rhs_states_renaming; }
    return result;
} // concatenate_eps().
} // Namespace mata::nfa.
