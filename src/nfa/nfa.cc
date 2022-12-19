/* nfa.cc -- operations for NFA
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
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

#include <algorithm>
#include <list>
#include <unordered_set>

// MATA headers
#include <mata/nfa.hh>
#include <simlib/explicit_lts.hh>

using std::tie;

using namespace Mata::Util;
using namespace Mata::Nfa;
using Mata::Nfa::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string Mata::Nfa::TYPE_NFA = "NFA";

namespace {
    Simlib::Util::BinaryRelation compute_fw_direct_simulation(const Nfa& aut) {
        Simlib::ExplicitLTS LTSforSimulation;
        Symbol maxSymbol = 0;
        const size_t state_num = aut.states_number();

        for (State stateFrom = 0; stateFrom < state_num; ++stateFrom) {
            for (const Move &t : aut.get_moves_from(stateFrom)) {
                for (State stateTo : t.targets) {
                    LTSforSimulation.add_transition(stateFrom, t.symbol, stateTo);
                }
                if (t.symbol > maxSymbol) {
                    maxSymbol = t.symbol;
                }
            }
        }

        // final states cannot be simulated by nonfinal -> we add new selfloops over final states with new symbol in LTS
        for (State finalState : aut.final) {
            LTSforSimulation.add_transition(finalState, maxSymbol + 1, finalState);
        }

        LTSforSimulation.init();
        return LTSforSimulation.compute_simulation();
    }

	Nfa reduce_size_by_simulation(const Nfa& aut, StateToStateMap &state_map) {
        Nfa result;
        const auto sim_relation = compute_relation(aut, StringMap{{"relation", "simulation"}, {"direction", "forward"}});

        auto sim_relation_symmetric = sim_relation;
        sim_relation_symmetric.restrict_to_symmetric();

        // for State q, quot_proj[q] should be the representative state representing the symmetric class of states in simulation
        std::vector<size_t> quot_proj;
        sim_relation_symmetric.get_quotient_projection(quot_proj);

		const size_t num_of_states = aut.states_number();

		// map each state q of aut to the state of the reduced automaton representing the simulation class of q
		for (State q = 0; q < num_of_states; ++q) {
			const State qReprState = quot_proj[q];
			if (state_map.count(qReprState) == 0) { // we need to map q's class to a new state in reducedAut
				const State qClass = result.add_state();
				state_map[qReprState] = qClass;
				state_map[q] = qClass;
			} else {
				state_map[q] = state_map[qReprState];
			}
		}

        for (State q = 0; q < num_of_states; ++q) {
            const State q_class_state = state_map.at(q);

            if (aut.initial[q]) { // if a symmetric class contains initial state, then the whole class should be initial
                result.initial.add(q_class_state);
            }

            if (quot_proj[q] == q) { // we process only transitions starting from the representative state, this is enough for simulation
                for (const auto &q_trans : aut.get_moves_from(q)) {
                    const StateSet representatives_of_states_to = [&]{
                        StateSet state_set;
                        for (auto s : q_trans.targets) {
                            state_set.insert(quot_proj[s]);
                        }
                        return state_set;
                    }();

                    // get the class states of those representatives that are not simulated by another representative in representatives_of_states_to
                    StateSet representatives_class_states;
                    for (const State s : representatives_of_states_to) {
                        bool is_state_important = true; // if true, we need to keep the transition from q to s
                        for (const State p : representatives_of_states_to) {
                            if (s != p && sim_relation.get(s, p)) { // if p (different from s) simulates s
                                is_state_important = false; // as p simulates s, the transition from q to s is not important to keep, as it is subsumed in transition from q to p
                                break;
                            }
                        }
                        if (is_state_important) {
                            representatives_class_states.insert(state_map.at(s));
                        }
                    }

                    // add the transition 'q_class_state-q_trans.symbol->representatives_class_states' at the end of transition list of transitions starting from q_class_state
                    // as the q_trans.symbol should be largest symbol we saw (as we iterate trough getTransitionsFromState(q) which is ordered)
                    result.delta[q_class_state].insert(Move(q_trans.symbol, representatives_class_states));
                }

                if (aut.final[q]) { // if q is final, then all states in its class are final => we make q_class_state final
                    result.final.add(q_class_state);
                }
            }
        }

        return result;
    }

    /**
     * Compute reachability of states.
     *
     * @param[in] nfa NFA to compute reachability for.
     * @return Bool array for reachable states (from initial states): true for reachable, false for unreachable states.
     */
    StateBoolArray compute_reachability(const Nfa& nfa) {
        std::vector<State> worklist{ nfa.initial.get_elements()};

        StateBoolArray reachable(nfa.states_number(), false);
        for (const State state: nfa.initial)
        {
            reachable.at(state) = true;
        }

        State state{};
        while (!worklist.empty())
        {
            state = worklist.back();
            worklist.pop_back();

            for (const auto& state_transitions: nfa.delta[state])
            {
                for (const State target_state: state_transitions.targets)
                {
                    if (!reachable.at(target_state))
                    {
                        worklist.push_back(target_state);
                        reachable.at(target_state) = true;
                    }
                }
            }
        }

        return reachable;
    }

    /**
     * Compute reachability of states considering only specified states.
     *
     * @param[in] nfa NFA to compute reachability for.
     * @param[in] states_to_consider State to consider as potentially reachable.
     * @return Bool array for reachable states (from initial states): true for reachable, false for unreachable states.
     */
    StateBoolArray compute_reachability(const Nfa& nfa, const StateBoolArray& states_to_consider) {
        std::vector<State> worklist{};
        StateBoolArray reachable(nfa.states_number(), false);
        for (const State state: nfa.initial) {
            if (states_to_consider[state]) {
                worklist.push_back(state);
                reachable.at(state) = true;
            }
        }

        State state;
        while (!worklist.empty()) {
            state = worklist.back();
            worklist.pop_back();

            for (const auto& state_transitions: nfa.delta[state]) {
                for (const State target_state: state_transitions.targets) {
                    if (states_to_consider[target_state] && !reachable[target_state]) {
                        worklist.push_back(target_state);
                        reachable[target_state] = true;
                    }
                }
            }
        }

        return reachable;
    }

    /**
     * Add transitions to the trimmed automaton.
     * @param[in] nfa NFA to add transitions from.
     * @param[in] original_to_new_states_map Map of old states to new trimmed automaton states.
     * @param[out] trimmed_aut The new trimmed automaton.
     */
    void add_trimmed_transitions(const Nfa& nfa, const StateToStateMap& original_to_new_states_map, Nfa& trimmed_aut) {
        // For each reachable original state 's' (which means it is mapped to the state of trimmed automaton)...
        for (const auto& original_state_mapping: original_to_new_states_map)
        {
            // ...add all transitions from 's' to some reachable state to the trimmed automaton.
            for (const auto& state_transitions_with_symbol: nfa.delta[original_state_mapping.first])
            {
                Move new_state_trans_with_symbol(state_transitions_with_symbol.symbol);
                for (State old_state_to: state_transitions_with_symbol.targets)
                {
                    auto iter_to_new_state_to = original_to_new_states_map.find(old_state_to);
                    if (iter_to_new_state_to != original_to_new_states_map.end())
                    {
                        // We can push here, because we assume that new states follow the ordering of original states.
                        new_state_trans_with_symbol.insert(iter_to_new_state_to->second);
                    }
                }
                if (!new_state_trans_with_symbol.empty()) {
                    trimmed_aut.delta[original_state_mapping.second].insert(new_state_trans_with_symbol);
                }
            }
        }
    }

    /**
     * Get a new trimmed automaton.
     * @param[in] nfa NFA to trim.
     * @param[in] original_to_new_states_map Map of old states to new trimmed automaton states (new states should follow the ordering of old states).
     * @return Newly created trimmed automaton.
     */
    Nfa create_trimmed_aut(const Nfa& nfa, const StateToStateMap& original_to_new_states_map) {
        Nfa trimmed_aut{ original_to_new_states_map.size() };

        for (const State old_initial_state: nfa.initial)
        {
            if (original_to_new_states_map.find(old_initial_state) != original_to_new_states_map.end())
            {
                trimmed_aut.initial.add(original_to_new_states_map.at(old_initial_state));
            }
        }
        for (const State old_final_state: nfa.final)
        {
            if (original_to_new_states_map.find(old_final_state) != original_to_new_states_map.end())
            {
                trimmed_aut.final.add(original_to_new_states_map.at(old_final_state));
            }
        }

        add_trimmed_transitions(nfa, original_to_new_states_map, trimmed_aut);
        return trimmed_aut;
    }

    /**
     * Get directed transitions for digraph.
     * @param[in] nfa NFA to get directed transitions from.
     * @param[in] abstract_symbol Abstract symbol to use for transitions in digraph.
     * @param[out] digraph Digraph to add computed transitions to.
     */
    void collect_directed_transitions(const Nfa& nfa, const Symbol abstract_symbol, Nfa& digraph) {
        const State num_of_states{nfa.states_number() };
        for (State src_state{ 0 }; src_state < num_of_states; ++src_state) {
            for (const auto& symbol_transitions: nfa.delta[src_state]) {
                for (const State tgt_state: symbol_transitions.targets) {
                    // Directly try to add the transition. Finding out whether the transition is already in the digraph
                    //  only iterates through transition relation again.
                    digraph.add_trans(src_state, abstract_symbol, tgt_state);
                }
                // FIXME: Alternatively: But it is actually slower...
                //digraph.add_trans(src_state, abstract_symbol, symbol_transitions.targets);
            }
        }
    }
}

std::ostream &std::operator<<(std::ostream &os, const Mata::Nfa::Trans &trans) { // {{{
    std::string result = "(" + std::to_string(trans.src) + ", " +
                         std::to_string(trans.symb) + ", " + std::to_string(trans.tgt) + ")";
    return os << result;
} // operator<<(ostream, Trans) }}}

////// Alphabet related functions

Mata::Util::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
    Util::OrdVector<Symbol> result;
	for (const auto& str_sym_pair : symbol_map) {
		result.insert(str_sym_pair.second);
	}
	return result;
} // OnTheFlyAlphabet::get_alphabet_symbols.

std::list<Symbol> OnTheFlyAlphabet::get_complement(const std::set<Symbol>& syms) const {
    std::list<Symbol> result;
    // TODO: Could be optimized.
    std::set<Symbol> symbols_alphabet;
    for (const auto& str_sym_pair : symbol_map) {
        symbols_alphabet.insert(str_sym_pair.second);
    }

    std::set_difference(
            symbols_alphabet.begin(), symbols_alphabet.end(),
            syms.begin(), syms.end(),
            std::inserter(result, result.end()));
    return result;
} // OnTheFlyAlphabet::get_complement.

void OnTheFlyAlphabet::add_symbols_from(const Nfa& nfa) {
    size_t aut_num_of_states{nfa.states_number() };
    for (State state{ 0 }; state < aut_num_of_states; ++state) {
        for (const auto& state_transitions: nfa.delta[state]) {
            update_next_symbol_value(state_transitions.symbol);
            try_add_new_symbol(std::to_string(state_transitions.symbol), state_transitions.symbol);
        }
    }
}

void OnTheFlyAlphabet::add_symbols_from(const StringToSymbolMap& new_symbol_map) {
    for (const auto& symbol_binding: new_symbol_map) {
        update_next_symbol_value(symbol_binding.second);
        try_add_new_symbol(symbol_binding.first, symbol_binding.second);
    }
}

///// Nfa structure related methods

void Nfa::add_trans(State state_from, Symbol symbol, State state_to) {
    // TODO: Define own exception.
    if (!is_state(state_from) || !is_state(state_to)) {
        throw std::out_of_range(std::to_string(state_from) + " or " + std::to_string(state_to) + " is not a state.");
    }

    auto& state_transitions{delta[state_from] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, state_to});
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, state_to});
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbolOnTransition already used on transitions from stateFrom.
            symbol_transitions->insert(state_to);
        } else {
            // Add transition to a new Move struct with symbolOnTransition yet unused on transitions from stateFrom.
            const Move new_symbol_transitions{symbol, state_to };
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Nfa::add_trans(const State state_from, const Symbol symbol, const StateSet& states_to) {
    // TODO: Define own exception.
    if (!is_state(state_from)) {
        throw std::out_of_range(std::to_string(state_from) + " is not a state.");
    }
    for (const State state_to: states_to) {
        if (!is_state(state_to)) {
            throw std::out_of_range(std::to_string(state_to) + " is not a state.");
        }
    }

    auto& state_transitions{delta[state_from] };
    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, states_to });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, states_to });
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transitions with symbol already used on transitions from state_from.
            symbol_transitions->insert(states_to);
            return;
        } else {
            // Add transitions to a new Move struct with symbol yet unused on transitions from state_from.
            const Move new_symbol_transitions {Move{symbol, states_to }};
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Nfa::remove_trans(State src, Symbol symb, State tgt) {
    auto& state_transitions{delta[src] };
    if (state_transitions.empty()) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else if (state_transitions.back().symbol < symb) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{symb }) };
        if (symbol_transitions == state_transitions.end()) {
            throw std::invalid_argument(
                    "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                    std::to_string(tgt) + "] does not exist.");
        } else {
            symbol_transitions->remove(tgt);
            if (symbol_transitions->empty()) {
                delta[src].remove(*symbol_transitions);
            }
        }
    }
}

State Nfa::add_state() {
    delta.emplace_back();
    return delta.size() - 1;
}

void Nfa::remove_epsilon(const Symbol epsilon)
{
    *this = Mata::Nfa::remove_epsilon(*this, epsilon);
}

StateSet Nfa::get_reachable_states() const
{
    StateBoolArray reachable_bool_array{ compute_reachability(*this) };

    StateSet reachable_states{};
    const size_t num_of_states{states_number() };
    for (State original_state{ 0 }; original_state < num_of_states; ++original_state)
    {
        if (reachable_bool_array[original_state])
        {
            reachable_states.insert(original_state);
        }
    }

    return reachable_states;
}

StateSet Nfa::get_terminating_states() const
{
    return revert(*this).get_reachable_states();
}

void Nfa::trim()
{
    *this = get_trimmed_automaton();
}

Nfa Nfa::get_trimmed_automaton() {
    if (initial.empty() || final.empty()) { return Nfa{}; }

    const StateSet original_useful_states{ get_useful_states() };
    StateToStateMap original_to_new_states_map{ original_useful_states.size() };
    size_t new_state_num{ 0 };
    for (const State original_state: original_useful_states) {
        original_to_new_states_map[original_state] = new_state_num;
        ++new_state_num;
    }
    return create_trimmed_aut(*this, original_to_new_states_map);
}

StateSet Nfa::get_useful_states() const
{
    if (initial.empty() || final.empty()) { return StateSet{}; }

    const Nfa digraph{get_one_letter_aut() }; // Compute reachability on directed graph.
    // Compute reachability from the initial states and use the reachable states to compute the reachability from the final states.
    const StateBoolArray useful_states_bool_array{ compute_reachability(revert(digraph), compute_reachability(digraph)) };

    const size_t num_of_states{states_number() };
    StateSet useful_states{};
    for (State original_state{ 0 }; original_state < num_of_states; ++original_state) {
        if (useful_states_bool_array[original_state]) {
            // We can use push_back here, because we are always increasing the value of original_state (so useful_states
            //  will always be ordered).
            useful_states.insert(original_state);
        }
    }
    return useful_states;
}

// General methods for NFA.

bool Mata::Nfa::are_state_disjoint(const Nfa& lhs, const Nfa& rhs)
{ // {{{
    // fill lhs_states with all states of lhs
    std::unordered_set<State> lhs_states;
    lhs_states.insert(lhs.initial.begin(), lhs.initial.end());
    lhs_states.insert(lhs.final.begin(), lhs.final.end());

    const size_t delta_size = lhs.delta.size();
    for (size_t i = 0; i < delta_size; i++) {
        lhs_states.insert(i);
        for (const auto& symStates : lhs.delta[i])
        {
            lhs_states.insert(symStates.targets.begin(), symStates.targets.end());
        }
    }

    // for every state found in rhs, check its presence in lhs_states
    for (const auto& rhs_st : rhs.initial) {
        if (haskey(lhs_states, rhs_st)) { return false; }
    }

    for (const auto& rhs_st : rhs.final) {
        if (haskey(lhs_states, rhs_st)) { return false; }
    }

    for (size_t i = 0; i < lhs.delta.size(); i++) {
        if (haskey(lhs_states, i))
            return false;
        for (const auto& symState : lhs.delta[i]) {
            for (const auto& rhState : symState.targets) {
                if (haskey(lhs_states,rhState)) {
                    return false;
                }
            }
        }
    }

    // no common state found
    return true;
} // are_disjoint }}}

void Mata::Nfa::make_complete(
        Nfa&             aut,
        const Alphabet&  alphabet,
        State            sink_state)
{
    std::list<State> worklist(aut.initial.begin(),
                              aut.initial.end());
    std::unordered_set<State> processed(aut.initial.begin(),
                                        aut.initial.end());
    worklist.push_back(sink_state);
    processed.insert(sink_state);
    while (!worklist.empty())
    {
        State state = *worklist.begin();
        worklist.pop_front();

        std::set<Symbol> used_symbols;
        if (!aut.has_no_transitions())
        {
            for (const auto &symb_stateset: aut[state]) {
                // TODO: Possibly fix insert.
                used_symbols.insert(symb_stateset.symbol);

                const StateSet &stateset = symb_stateset.targets;
                for (const auto &tgt_state: stateset) {
                    bool inserted;
                    tie(std::ignore, inserted) = processed.insert(tgt_state);
                    if (inserted) { worklist.push_back(tgt_state); }
                }
            }
        }

        auto unused_symbols = alphabet.get_complement(used_symbols);
        for (Symbol symb : unused_symbols)
        {
            aut.add_trans(state, symb, sink_state);
        }
    }
}

Nfa Mata::Nfa::remove_epsilon(const Nfa& aut, Symbol epsilon)
{
    Nfa result;

    result.clear();
    result.increase_size(aut.states_number());

    // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
    std::unordered_map<State, StateSet> eps_closure;

    // TODO: grossly inefficient
    // first we compute the epsilon closure
    const size_t num_of_states{aut.states_number() };
    for (size_t i{ 0 }; i < num_of_states; ++i)
    {
        for (const auto& trans: aut[i])
        { // initialize
            const auto it_ins_pair = eps_closure.insert({i, {i}});
            if (trans.symbol == epsilon)
            {
                StateSet& closure = it_ins_pair.first->second;
                // TODO: Fix possibly insert to OrdVector. Create list already ordered, then merge (do not need to resize each time);
                closure.insert(trans.targets);
            }
        }
    }

    bool changed = true;
    while (changed) { // compute the fixpoint
        changed = false;
        for (size_t i=0; i < num_of_states; ++i) {
            const auto& state_transitions{ aut[i] };
            const auto state_symbol_transitions{
                state_transitions.find(Move{epsilon}) };
            if (state_symbol_transitions != state_transitions.end()) {
                StateSet &src_eps_cl = eps_closure[i];
                for (const State tgt: state_symbol_transitions->targets) {
                    const StateSet &tgt_eps_cl = eps_closure[tgt];
                    for (const State st: tgt_eps_cl) {
                        if (src_eps_cl.count(st) == 0) {
                            changed = true;
                            break;
                        }
                    }
                    src_eps_cl.insert(tgt_eps_cl);
                }
            }
        }
    }

    // now we construct the automaton without epsilon transitions
    result.initial.add(aut.initial.get_elements());
    result.final.add(aut.initial.get_elements());
    State max_state{};
    for (const auto& state_closure_pair : eps_closure) { // for every state
        State src_state = state_closure_pair.first;
        for (State eps_cl_state : state_closure_pair.second) { // for every state in its eps cl
            if (aut.final[eps_cl_state]) result.final.add(src_state);
            for (const auto& symb_set : aut[eps_cl_state]) {
                if (symb_set.symbol == epsilon) continue;

                // TODO: this could be done more efficiently if we had a better add_trans method
                for (State tgt_state : symb_set.targets) {
                    max_state = std::max(src_state, tgt_state);
                    if (result.states_number() < max_state) {
                        result.increase_size_for_state(max_state);
                    }
                    result.add_trans(src_state, symb_set.symbol, tgt_state);
                }
            }
        }
    }

    return result;
}

Nfa Mata::Nfa::revert(const Nfa& aut) {
    Nfa result;
    result.clear();
    const size_t num_of_states{aut.states_number() };
    result.increase_size(num_of_states);

    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const Move &transition: aut.delta[sourceState]) {
            for (const State targetState: transition.targets) {
                result.add_trans(targetState, transition.symbol, sourceState);
            }
        }
    }

    result.initial = aut.final;
    result.final = aut.initial;

    return result;
}

bool Mata::Nfa::is_deterministic(const Nfa& aut)
{
    if (aut.initial.size() != 1) { return false; }

    if (aut.has_no_transitions()) { return true; }

    const size_t aut_size = aut.states_number();
    for (size_t i = 0; i < aut_size; ++i)
    {
        for (const auto& symStates : aut[i])
        {
            if (symStates.size() != 1) { return false; }
        }
    }

    return true;
}
bool Mata::Nfa::is_complete(const Nfa& aut, const Alphabet& alphabet)
{
    Util::OrdVector<Symbol> symbs_ls = alphabet.get_alphabet_symbols();
    Util::OrdVector<Symbol> symbs(symbs_ls.cbegin(), symbs_ls.cend());

    // TODO: make a general function for traversal over reachable states that can
    // be shared by other functions?
    std::list<State> worklist(aut.initial.begin(),
                              aut.initial.end());
    std::unordered_set<State> processed(aut.initial.begin(),
                                        aut.initial.end());

    while (!worklist.empty())
    {
        State state = *worklist.begin();
        worklist.pop_front();

        size_t n = 0;      // counter of symbols
        if (!aut.has_no_transitions()) {
            for (const auto &symb_stateset: aut[state]) {
                ++n;
                if (!haskey(symbs, symb_stateset.symbol)) {
                    throw std::runtime_error(std::to_string(__func__) +
                                             ": encountered a symbol that is not in the provided alphabet");
                }

                for (const auto &tgt_state: symb_stateset.targets) {
                    bool inserted;
                    tie(std::ignore, inserted) = processed.insert(tgt_state);
                    if (inserted) { worklist.push_back(tgt_state); }
                }
            }
        }

        if (symbs.size() != n) { return false; }
    }

    return true;
}

std::pair<Run, bool> Mata::Nfa::get_word_for_path(const Nfa& aut, const Run& run)
{
    if (run.path.empty())
    {
        return {{}, true};
    }

    Run word;
    State cur = run.path[0];
    for (size_t i = 1; i < run.path.size(); ++i)
    {
        State newSt = run.path[i];
        bool found = false;

        if (!aut.has_no_transitions())
        {
            for (const auto &symbolMap: aut.delta[cur]) {
                for (State st: symbolMap.targets) {
                    if (st == newSt) {
                        word.word.push_back(symbolMap.symbol);
                        found = true;
                        break;
                    }
                }

                if (found) { break; }
            }
        }

        if (!found)
        {
            return {{}, false};
        }

        cur = newSt;    // update current state
    }

    return {word, true};
}

bool Mata::Nfa::is_in_lang(const Nfa& aut, const Run& run)
{
    StateSet cur(aut.initial);

    for (Symbol sym : run.word)
    {
        cur = aut.post(cur, sym);
        if (cur.empty()) { return false; }
    }

    return !are_disjoint(cur, aut.final);
}

/// Checks whether the prefix of a string is in the language of an automaton
bool Mata::Nfa::is_prfx_in_lang(const Nfa& aut, const Run& run)
{
    StateSet cur = (StateSet(aut.initial));

    for (Symbol sym : run.word)
    {
        if (!are_disjoint(cur, aut.final)) { return true; }
        cur = aut.post(cur, sym);
        if (cur.empty()) { return false; }
    }

    return !are_disjoint(cur, aut.final);
}

/// serializes Nfa into a ParsedSection
Mata::Parser::ParsedSection Mata::Nfa::serialize(
        const Nfa&                aut,
        const SymbolToStringMap*  symbol_map,
        const StateToStringMap*   state_map)
{assert(false);}

bool Mata::Nfa::is_lang_empty(const Nfa& aut, Run* cex)
{ // {{{
    std::list<State> worklist(
            aut.initial.begin(), aut.initial.end());
    std::unordered_set<State> processed(
            aut.initial.begin(), aut.initial.end());

    // 'paths[s] == t' denotes that state 's' was accessed from state 't',
    // 'paths[s] == s' means that 's' is an initial state
    std::map<State, State> paths;
    for (State s : worklist)
    {	// initialize
        paths[s] = s;
    }

    while (!worklist.empty())
    {
        State state = worklist.front();
        worklist.pop_front();

        if (aut.final[state])
        {
            // TODO process the CEX
            if (nullptr != cex)
            {
                cex->path.clear();
                cex->path.push_back(state);
                while (paths[state] != state)
                {
                    state = paths[state];
                    cex->path.push_back(state);
                }

                std::reverse(cex->path.begin(), cex->path.end());
                cex->word = get_word_for_path(aut, *cex).first.word;
            }
            return false;
        }

        if (aut.has_no_transitions())
            continue;

        for (const auto& symb_stateset : aut[state])
        {
            for (const auto& tgt_state : symb_stateset.targets)
            {
                bool inserted;
                tie(std::ignore, inserted) = processed.insert(tgt_state);
                if (inserted)
                {
                    worklist.push_back(tgt_state);
                    // also set that tgt_state was accessed from state
                    paths[tgt_state] = state;
                }
                else
                {
                    // the invariant
                    assert(haskey(paths, tgt_state));
                }
            }
        }
    }

    return true;
} // is_lang_empty }}}

Nfa Mata::Nfa::minimize(const Nfa& aut) {
    //compute the minimal deterministic automaton, Brzozovski algorithm
    Nfa inverted = revert(aut);
    Nfa tmp = determinize(inverted);
    Nfa deter = revert(tmp);
    return determinize(deter);
}

void Nfa::print_to_DOT(std::ostream &outputStream) const {
    outputStream << "digraph finiteAutomaton {" << std::endl
                 << "node [shape=circle];" << std::endl;

    for (State finalState: final) {
        outputStream << finalState << " [shape=doublecircle];" << std::endl;
    }

    const size_t delta_size = delta.size();
    for (State s = 0; s != delta_size; ++s) {
        for (const Move &t: delta[s]) {
            outputStream << s << " -> {";
            for (State sTo: t.targets) {
                outputStream << sTo << " ";
            }
            outputStream << "} [label=" << t.symbol << "];" << std::endl;
        }
    }

    outputStream << "node [shape=none, label=\"\"];" << std::endl;
    for (State initialState: initial) {
        outputStream << "i" << initialState << " -> " << initialState << ";" << std::endl;
    }

    outputStream << "}" << std::endl;
}

TransSequence Nfa::get_trans_as_sequence() const
{
    TransSequence trans_sequence{};

    for (State state_from{ 0 }; state_from < delta.size(); ++state_from)
    {
        for (const auto& transition_from_state: delta[state_from])
        {
            for (State state_to: transition_from_state.targets)
            {
                trans_sequence.push_back(Trans{ state_from, transition_from_state.symbol, state_to });
            }
        }
    }

    return trans_sequence;
}

TransSequence Nfa::get_trans_from_as_sequence(State state_from) const
{
    TransSequence trans_sequence{};

    for (const auto& transition_from_state: delta[state_from])
    {
        for (State state_to: transition_from_state.targets)
        {
            trans_sequence.push_back(Trans{ state_from, transition_from_state.symbol, state_to });
        }
    }

    return trans_sequence;
}


size_t Nfa::get_num_of_trans() const
{
    size_t num_of_transitions{};

    for (const auto& state_transitions: delta)
    {
        ++num_of_transitions;
    }

    return num_of_transitions;
}

Nfa Nfa::get_one_letter_aut(Symbol abstract_symbol) const {
    Nfa digraph{states_number(), StateSet(initial), StateSet(final)};
    collect_directed_transitions(*this, abstract_symbol, digraph);
    return digraph;
}

void Nfa::get_one_letter_aut(Nfa& result) const {
    result = get_one_letter_aut();
}

bool Mata::Nfa::Nfa::has_no_transitions() const
{
    return delta.begin() == delta.end();
}

TransSequence Nfa::get_transitions_to(State state_to) const {
    TransSequence transitions_to_state{};
    const size_t num_of_states{states_number() };
    for (State state_from{ 0 }; state_from < num_of_states; ++state_from) {
        for (const auto& symbol_transitions: delta[state_from]) {
            const auto& symbol_states_to{ symbol_transitions.targets };
            const auto target_state{ symbol_states_to.find(state_to) };
            if (target_state != symbol_states_to.end()) {
                transitions_to_state.push_back({ state_from, symbol_transitions.symbol, state_to });
            }
        }
    }
    return transitions_to_state;
}

StateSet Nfa::post(const StateSet& states, const Symbol& symbol) const {
    StateSet res{};
    if (has_no_transitions()) {
        return res;
    }

    for (const auto state : states) {
        const auto& state_transitions{delta[state] };
        const auto state_symbol_transitions{ state_transitions.find(Move{symbol }) };
        if (state_symbol_transitions != state_transitions.end()) {
            res.insert(state_symbol_transitions->targets);
        }
    }
    return res;
}

Post::const_iterator Nfa::Nfa::get_epsilon_transitions(const State state, const Symbol epsilon) const {
    assert(is_state(state));
    return get_epsilon_transitions(get_moves_from(state), epsilon);
}

Post::const_iterator Nfa::Nfa::get_epsilon_transitions(const Post& state_transitions, const Symbol epsilon) {
    if (!state_transitions.empty()) {
        if (epsilon == EPSILON) {
            const auto& back = state_transitions.back();
            if (back.symbol == epsilon) {
                return std::prev(state_transitions.end());
            }
        } else {
            return state_transitions.find(Move(epsilon));
        }
    }

    return state_transitions.end();
}

Nfa Mata::Nfa::uni(const Nfa &lhs, const Nfa &rhs) {
    Nfa unionAutomaton = rhs;

    StateToStateMap thisStateToUnionState;
    const size_t delta_size = lhs.delta.size();
    for (State thisState = 0; thisState < delta_size; ++thisState) {
        thisStateToUnionState[thisState] = unionAutomaton.add_state();
    }

    for (State thisInitialState : lhs.initial) {
        unionAutomaton.initial.add(thisStateToUnionState[thisInitialState]);
    }

    for (State thisFinalState : lhs.final) {
        unionAutomaton.final.add(thisStateToUnionState[thisFinalState]);
    }

    for (State thisState = 0; thisState < delta_size; ++thisState) {
        State unionState = thisStateToUnionState[thisState];
        for (const Move &transitionFromThisState : lhs.delta[thisState]) {

            Move transitionFromUnionState(transitionFromThisState.symbol, StateSet{});

            for (State stateTo : transitionFromThisState.targets) {
                transitionFromUnionState.insert(thisStateToUnionState[stateTo]);
            }

            unionAutomaton.delta[unionState].insert(transitionFromUnionState);
        }
    }

    return unionAutomaton;
}

Simlib::Util::BinaryRelation Mata::Nfa::compute_relation(const Nfa& aut, const StringMap& params) {
    if (!haskey(params, "relation")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"relation\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }
    if (!haskey(params, "direction")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"direction\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    const std::string& relation = params.at("relation");
    const std::string& direction = params.at("direction");
    if ("simulation" == relation && direction == "forward") {
        return compute_fw_direct_simulation(aut);
    }
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"relation\" key: " + relation);
    }
}

Nfa Mata::Nfa::reduce(const Nfa &aut, StateToStateMap *state_map, const StringMap& params) {
    Nfa result;

    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algorithm\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    result.clear();
    const std::string& algorithm = params.at("algorithm");
    if ("simulation" == algorithm) {
        if (state_map == nullptr) {
            std::unordered_map<State,State> tmp_state_map;
            return reduce_size_by_simulation(aut, tmp_state_map);
        } else {
            return reduce_size_by_simulation(aut, *state_map);
        }
    } else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algorithm\" key: " + algorithm);
    }
}

Nfa Mata::Nfa::determinize(
        const Nfa&  aut,
        std::unordered_map<StateSet, State> *subset_map)
{

    Nfa result;
    //assuming all sets targets are non-empty
    std::vector<std::pair<State, StateSet>> worklist;
    bool deallocate_subset_map = false;
    if (subset_map == nullptr) {
        subset_map = new std::unordered_map<StateSet,State>();
        deallocate_subset_map = true;
    }

    result.clear();

    const StateSet S0 =  StateSet(aut.initial);
    const State S0id = result.add_state();
    result.initial.add(S0id);

    if (!are_disjoint(S0, aut.final)) {
        result.final.add(S0id);
    }
    worklist.emplace_back(std::make_pair(S0id, S0));

    (*subset_map)[Mata::Util::OrdVector<State>(S0)] = S0id;

    if (aut.has_no_transitions())
        return result;

    using Iterator = Mata::Util::OrdVector<Move>::const_iterator;
    Mata::Util::SynchronizedExistentialIterator<Iterator> synchronized_iterator;

    while (!worklist.empty()) {
        const auto Spair = worklist.back();
        worklist.pop_back();
        const StateSet S = Spair.second;
        const State Sid = Spair.first;
        if (S.empty()) {
            break;//this should not happen assuming all sets targets are non empty
        }

        // add moves of S to the sync ex iterator
        // TODO: shouldn't we also reset first?
        for (State q: S) {
            Mata::Util::push_back(synchronized_iterator, aut.delta[q]);
        }

        while (synchronized_iterator.advance()) {

            // extract post from the sychronized_iterator iterator
            std::vector<Iterator> moves = synchronized_iterator.get_current();
            Symbol currentSymbol = (*moves.begin())->symbol;
            StateSet T;
            for (auto m: moves){
                T = T.Union(m->targets);
            }

            const auto existingTitr = subset_map->find(T);
            State Tid;
            if (existingTitr != subset_map->end()) {
                Tid = existingTitr->second;
            } else {
                Tid = result.add_state();
                (*subset_map)[Mata::Util::OrdVector<State>(T)] = Tid;
                if (!are_disjoint(T, aut.final)) {
                    result.final.add(Tid);
                }
                worklist.emplace_back(std::make_pair(Tid, T));
            }
            result.delta[Sid].insert(Move(currentSymbol, Tid));
        }
    }

    if (deallocate_subset_map) { delete subset_map; }

    return result;
}

Nfa Mata::Nfa::construct(
        const Mata::Parser::ParsedSection&   parsec,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map)
{ // {{{
    Nfa aut;
    assert(nullptr != alphabet);

    if (parsec.type != Mata::Nfa::TYPE_NFA) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 Mata::Nfa::TYPE_NFA + "\"");
    }

    bool remove_state_map = false;
    if (nullptr == state_map) {
        state_map = new StringToStateMap();
        remove_state_map = true;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->count(str)) {
            State state = aut.add_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    // a lambda for cleanup
    auto clean_up = [&]() {
        if (remove_state_map) { delete state_map; }
    };


    auto it = parsec.dict.find("Initial");
    if (parsec.dict.end() != it)
    {
        for (const auto& str : it->second)
        {
            State state = get_state_name(str);
            aut.initial.add(state);
        }
    }


    it = parsec.dict.find("Final");
    if (parsec.dict.end() != it)
    {
        for (const auto& str : it->second)
        {
            State state = get_state_name(str);
            aut.final.add(state);
        }
    }

    for (const auto& body_line : parsec.body)
    {
        if (body_line.size() != 3)
        {
            // clean up
            clean_up();

            if (body_line.size() == 2)
            {
                throw std::runtime_error("Epsilon transitions not supported: " +
                                         std::to_string(body_line));
            }
            else
            {
                throw std::runtime_error("Invalid transition: " +
                                         std::to_string(body_line));
            }
        }

        State src_state = get_state_name(body_line[0]);
        Symbol symbol = alphabet->translate_symb(body_line[1]);
        State tgt_state = get_state_name(body_line[2]);

        aut.add_trans(src_state, symbol, tgt_state);
    }

    // do the dishes and take out garbage
    clean_up();

    return aut;
} // construct }}}

Nfa Mata::Nfa::construct(
        const Mata::IntermediateAut&         inter_aut,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map)
{ // {{{
    Nfa aut;
    assert(nullptr != alphabet);

    if (!inter_aut.is_nfa()) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 Mata::Nfa::TYPE_NFA + "\"");
    }

    bool remove_state_map = false;
    if (nullptr == state_map) {
        state_map = new StringToStateMap();
        remove_state_map = true;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->count(str)) {
            State state = aut.add_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    // a lambda for cleanup
    auto clean_up = [&]() {
        if (remove_state_map) { delete state_map; }
    };

    for (const auto& str : inter_aut.initial_formula.collect_node_names())
    {
        State state = get_state_name(str);
        aut.initial.add(state);
    }

    const auto final_states = inter_aut.are_final_states_conjunction_of_negation() ?
            inter_aut.get_positive_finals() : inter_aut.final_formula.collect_node_names();
    for (const auto& str : final_states)
    {
        State state = get_state_name(str);
        aut.final.add(state);
    }

    for (const auto& trans : inter_aut.transitions)
    {
        if (trans.second.children.size() != 2)
        {
            // clean up
            clean_up();

            if (trans.second.children.size() == 1)
            {
                throw std::runtime_error("Epsilon transitions not supported");
            }
            else
            {
                throw std::runtime_error("Invalid transition");
            }
        }

        State src_state = get_state_name(trans.first.name);
        Symbol symbol = alphabet->translate_symb(trans.second.children[0].node.name);
        State tgt_state = get_state_name(trans.second.children[1].node.name);

        aut.add_trans(src_state, symbol, tgt_state);
    }

    // do the dishes and take out garbage
    clean_up();

    return aut;
} // construct }}}

Nfa::const_iterator Nfa::const_iterator::for_begin(const Nfa* nfa)
{ // {{{
    assert(nullptr != nfa);

    const_iterator result;

    if (nfa->delta.begin() == nfa->delta.end()) {
        result.is_end = true;
        return result;
    }

    result.nfa = nfa;
    result.trIt = 0;
    assert(!nfa->get_moves_from(0).empty());
    result.tlIt = nfa->get_moves_from(0).begin();
    assert(!nfa->get_moves_from(0).begin()->empty());
    result.ssIt = result.tlIt->targets.begin();

    result.refresh_trans();

    return result;
} // for_begin }}}

Nfa::const_iterator Nfa::const_iterator::for_end(const Nfa* /* nfa*/)
{ // {{{
    const_iterator result;
    result.is_end = true;
    return result;
} // for_end }}}

Nfa::const_iterator& Nfa::const_iterator::operator++()
{ // {{{
    assert(nullptr != nfa);

    ++(this->ssIt);
    const StateSet& state_set = this->tlIt->targets;
    assert(!state_set.empty());
    if (this->ssIt != state_set.end())
    {
        this->refresh_trans();
        return *this;
    }

    // out of state set
    ++(this->tlIt);
    const Post& tlist = this->nfa->get_moves_from(this->trIt);
    assert(!tlist.empty());
    if (this->tlIt != tlist.end())
    {
        this->ssIt = this->tlIt->targets.begin();

        this->refresh_trans();
        return *this;
    }

    // out of transition list
    ++this->trIt;
    assert(this->nfa->delta.begin() != this->nfa->delta.end());

    while (this->trIt < this->nfa->delta.size() &&
           this->nfa->get_moves_from(this->trIt).empty())
    {
        ++this->trIt;
    }

    if (this->trIt < this->nfa->delta.size())
    {
        this->tlIt = this->nfa->get_moves_from(this->trIt).begin();
        assert(!this->nfa->get_moves_from(this->trIt).empty());
        const StateSet& new_state_set = this->tlIt->targets;
        assert(!new_state_set.empty());
        this->ssIt = new_state_set.begin();

        this->refresh_trans();
        return *this;
    }

    // out of transitions
    this->is_end = true;

    return *this;
} // operator++ }}}

std::ostream& std::operator<<(std::ostream& os, const Mata::Nfa::Nfa& nfa) {
    os << "{ NFA: " << std::to_string(serialize(nfa));
    if (nfa.alphabet != nullptr) {
        os << "|alphabet: " << *(nfa.alphabet);
    }
    // TODO: If the default implementation for state_dict is implemented, consider printing the state dictionary with
    //  std::to_string(<state_dict_object>);

    os << " }";
}

std::ostream &std::operator<<(std::ostream &os, const Alphabet& alphabet) {
    return os << std::to_string(alphabet);
}

Mata::Util::OrdVector<Symbol> Nfa::get_used_symbols() const {
     Util::OrdVector<Symbol>  symbols{};
     for (const auto& transition: delta) {
        symbols.insert(transition.symb);
     }
     return symbols;
 }

 void Nfa::unify_initial() {
    if (initial.empty() || initial.size() == 1) { return; }
    const State new_initial_state{add_state() };
    for (const auto& orig_initial_state: initial) {
        for (const auto& transitions: get_moves_from(orig_initial_state)) {
            for (const State state_to: transitions.targets) {
                add_trans(new_initial_state, transitions.symbol, state_to);
            }
        }
        if (final[orig_initial_state]) { final.add(new_initial_state); }
    }
    initial.clear();
    initial.add(new_initial_state);
}

void Nfa::unify_final() {
    if (final.empty() || final.size() == 1) { return; }
    const State new_final_state{add_state() };
    for (const auto& orig_final_state: final) {
        for (const auto& transitions: get_transitions_to(orig_final_state)) {
            add_trans(transitions.src, transitions.symb, new_final_state);
        }
        if (initial[orig_final_state]) { initial.add(new_final_state); }
    }
    final.clear();
    final.add(new_final_state);
}
