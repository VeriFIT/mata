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
#include <mata/nfa-algorithms.hh>
#include <mata/simlib/explicit_lts.hh>

using std::tie;

using namespace Mata::Util;
using namespace Mata::Nfa;
using Mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string Mata::Nfa::TYPE_NFA = "NFA";

namespace {
    Simlib::Util::BinaryRelation compute_fw_direct_simulation(const Nfa& aut) {
        Symbol maxSymbol = 0;
        const size_t state_num = aut.size();
        Simlib::ExplicitLTS LTSforSimulation(state_num);

        for (const auto& transition : aut.delta) {
            LTSforSimulation.add_transition(transition.src, transition.symb, transition.tgt);
            if (transition.symb > maxSymbol) {
                maxSymbol = transition.symb;
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
        const auto sim_relation = Algorithms::compute_relation(
                aut, StringMap{{"relation", "simulation"}, {"direction", "forward"}});

        auto sim_relation_symmetric = sim_relation;
        sim_relation_symmetric.restrict_to_symmetric();

        // for State q, quot_proj[q] should be the representative state representing the symmetric class of states in simulation
        std::vector<size_t> quot_proj;
        sim_relation_symmetric.get_quotient_projection(quot_proj);

		const size_t num_of_states = aut.size();

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
                    // as the q_trans.symbol should be the largest symbol we saw (as we iterate trough getTransitionsFromState(q) which is ordered)
                    result.delta.get_mutable_post(q_class_state).insert(Move(q_trans.symbol, representatives_class_states));
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

        StateBoolArray reachable(nfa.size(), false);
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
        StateBoolArray reachable(nfa.size(), false);
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
                    trimmed_aut.delta.get_mutable_post(original_state_mapping.second).insert(new_state_trans_with_symbol);
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
        const State num_of_states{nfa.size() };
        for (State src_state{ 0 }; src_state < num_of_states; ++src_state) {
            for (const auto& symbol_transitions: nfa.delta[src_state]) {
                for (const State tgt_state: symbol_transitions.targets) {
                    // Directly try to add the transition. Finding out whether the transition is already in the digraph
                    //  only iterates through transition relation again.
                    digraph.delta.add(src_state, abstract_symbol, tgt_state);
                }
                // FIXME: Alternatively: But it is actually slower...
                //digraph.add(src_state, abstract_symbol, symbol_transitions.targets);
            }
        }
    }
}

std::ostream &std::operator<<(std::ostream &os, const Mata::Nfa::Trans &trans) { // {{{
    std::string result = "(" + std::to_string(trans.src) + ", " +
                         std::to_string(trans.symb) + ", " + std::to_string(trans.tgt) + ")";
    return os << result;
} // operator<<(ostream, Trans) }}}

size_t Delta::size() const
{
    size_t size = 0;
    for (State q = 0; q < posts_size(); ++q)
        for (const Move & m: (*this)[q])
            size = size + m.size();

    return size;
}

void Delta::add(State state_from, Symbol symbol, State state_to) {
    const State max_state{ std::max(state_from, state_to) };
    if (max_state >= posts.size()) {
        reserve_on_insert(posts, max_state);
        posts.resize(max_state + 1);
    }

    auto& state_transitions{ posts[state_from] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, state_to });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, state_to });
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{ symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbolOnTransition already used on transitions from stateFrom.
            symbol_transitions->insert(state_to);
        } else {
            // Add transition to a new Move struct with symbolOnTransition yet unused on transitions from stateFrom.
            const Move new_symbol_transitions{ symbol, state_to };
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Delta::remove(State src, Symbol symb, State tgt) {
    if (src >= posts.size()) {
        return;
    }

    auto& state_transitions{ posts[src] };
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
                posts[src].remove(*symbol_transitions);
            }
        }
    }
}

bool Delta::contains(State src, Symbol symb, State tgt) const
{ // {{{
    if (posts.empty()) {
        return false;
    }

    if (posts.size() <= src)
        return false;

    const Post& tl = posts[src];
    if (tl.empty()) {
        return false;
    }
    auto symbol_transitions{ tl.find(Move{symb} ) };
    if (symbol_transitions == tl.cend()) {
        return false;
    }

    return symbol_transitions->targets.find(tgt) != symbol_transitions->targets.end();
}

bool Delta::empty() const
{
    return this->begin() == this->end();
}

Delta::const_iterator::const_iterator(const std::vector<Post>& post_p, bool ise) :
    post(post_p), current_state(0), is_end(ise)
{
    const size_t post_size = post.size();
    for (size_t i = 0; i < post_size; ++i) {
        if (!post[i].empty()) {
            current_state = i;
            post_iterator = post[i].begin();
            targets_position = post_iterator->targets.begin();
            return;
        }
    }

    // no transition found, an empty post
    is_end = true;
}

Delta::const_iterator& Delta::const_iterator::operator++()
{
    assert(post.begin() != post.end());

    ++targets_position;
    if (targets_position != post_iterator->targets.end())
        return *this;

    ++post_iterator;
    if (post_iterator != post[current_state].cend()) {
        targets_position = post_iterator->targets.begin();
        return *this;
    }

    ++current_state;
    while (current_state < post.size() && post[current_state].empty()) // skip empty posts
        current_state++;

    if (current_state >= post.size())
        is_end = true;
    else {
        post_iterator = post[current_state].begin();
        targets_position = post_iterator->targets.begin();
    }

    return *this;
}

State Delta::find_max_state() {
    size_t max = 0;
    State src = 0;
    for (Post & p: posts) {
        if (src > max)
            max = src;
        for (Move & m: p) {
            if (!m.targets.empty())
                if (m.targets.back() > max)
                    max = m.targets.back();
        }
        src++;
    }
    return max;
}

///// Nfa structure related methods

void Nfa::remove_epsilon(const Symbol epsilon)
{
    *this = Mata::Nfa::remove_epsilon(*this, epsilon);
}

StateSet Nfa::get_reachable_states() const
{
    StateBoolArray reachable_bool_array{ compute_reachability(*this) };

    StateSet reachable_states{};
    const size_t num_of_states{size() };
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

//TODO: probably can be removed, trim_reverting faster.
void Nfa::trim_inplace(StateToStateMap* state_map)
{
    if (!state_map) {
        StateToStateMap tmp_state_map{};
        *this = get_trimmed_automaton(&tmp_state_map);
    } else {
        state_map->clear();
        *this = get_trimmed_automaton(state_map);
    }
}

void Nfa::trim_reverting(StateToStateMap* state_map)
{
#ifdef _STATIC_STRUCTURES_
    static NumberPredicate<State> useful_states(false);
    useful_states.clear();
    useful_states = get_useful_states();
#else
    NumberPredicate<State> useful_states{ get_useful_states() };
#endif

    initial.defragment(useful_states);
    final.defragment(useful_states);
    //this renaming can be used instead of the state map, it is computed there anyway, so far not used.
    std::vector<State> renaming = delta.defragment(useful_states);

    // TODO : this is actually ony used in one test, remove state map?
    if (state_map) {
        state_map->clear();
        state_map->reserve(useful_states.domain_size());
        StateSet us = StateSet(useful_states.get_elements());
        std::vector<State> usv = us.ToVector();
        for (State q=0;q<useful_states.size();q++)
            (*state_map)[usv[q]] = q;

        // Attempt to use the renaming for this, did not work ... but should work somehow ...
        // I think we should just return the renaming in fact, it would need a little more refactoring.
        //for (State q=0;q<useful_states.size();q++)
        //    (*state_map)[q] = renaming[q];
    }
}

Nfa Nfa::get_trimmed_automaton(StateToStateMap* state_map) const {
    if (initial.empty() || final.empty()) { return Nfa{}; }

    StateToStateMap tmp_state_map{};
    if (!state_map) {
        state_map = &tmp_state_map;
    }
    state_map->clear();

    const StateSet original_useful_states{get_useful_states_old() };
    state_map->reserve(original_useful_states.size());

    size_t new_state_num{ 0 };
    for (const State original_state: original_useful_states) {
        (*state_map)[original_state] = new_state_num;
        ++new_state_num;
    }
    return create_trimmed_aut(*this, *state_map);
}

// A data structure to store things in the depth first search within dfs in the trim.
// It stores a state and the state of the iteration through the successors of the state.
struct StackLevel {
    State state;
    Post::const_iterator post_it;
    Post::const_iterator post_end;
    StateSet::const_iterator targets_it;
    StateSet::const_iterator targets_end;

    StackLevel(State q, const Delta & delta):
            state(q),
            post_it(delta[q].cbegin()),
            post_end(delta[q].cend())
    {
        if (post_it != post_end) {
            targets_it = post_it->cbegin();
            targets_end = post_it->cend();
        }
    };
};

const NumberPredicate<State> Nfa::get_useful_states() const
{
#ifdef _STATIC_STRUCTURES_
    // STATIC SEEMS TO GIVE LIKE 5-10% SPEEDUP
    static std::vector<StackLevel> stack;
    //tracking elements seems to cost more than it saves, switching it off
    static NumberPredicate<State> reached(false);
    static NumberPredicate<State> reached_and_reaching(false);
    stack.clear();
    reached.clear();
    reached_and_reaching.clear();
#else
    std::vector<StackLevel> stack;//the DFS stack
    //tracking elements seems to cost more than it saves, switching it off
    NumberPredicate<State> reached(size(),false,false); //what has been reached from the initial states
    NumberPredicate<State> reached_and_reaching(size(),false,false);//what is reaching final states

    //std::vector<bool> reached(size(),false);
    //std::vector<bool> reached_and_reaching(size(),false);
    //std::vector<StackLevel> stack;
#endif

    for (const State q0: initial) {

        stack.emplace_back(q0,delta);
        reached.add(q0);
        if (final[q0])
            reached_and_reaching.add(q0);
        while (!stack.empty()) {
            StackLevel & level = stack.back();
            //Continue the iteration through the successors of q (a shitty code. Is there a better way? What would be the needed interface for mata?)
            while (level.post_it != level.post_end && level.targets_it == level.targets_end) {
                if (level.targets_it == level.targets_end) {
                    ++level.post_it;
                    if (level.post_it != level.post_end) {
                        level.targets_it = level.post_it->cbegin();
                        level.targets_end = level.post_it->cend();
                    }
                } else
                    ++level.targets_it;
            }
            if (level.post_it == level.post_end) {
                stack.pop_back();
            }
            else {
                State succ_state = *(level.targets_it);
                ++level.targets_it;
                if (final[succ_state])
                    reached_and_reaching.add(succ_state);
                if (reached_and_reaching[succ_state])
                {
                    //A major trick, because of which one DFS is enough for reached as well as reaching.
                    //On touching a state which reaches finals states, everything in the stack below reaches a final state.
                    //An invariant of the stack is that everything below a reaching state is reaching.
                    for (auto it = stack.crbegin(); it != stack.crend() && !reached_and_reaching[it->state]; it++) {
                            reached_and_reaching.add(it->state);
                    }
                }
                if (!reached[succ_state]) {
                    reached.add(succ_state);
                    stack.emplace_back(succ_state,delta);
                }
            }
        }
    }
    return reached_and_reaching;
}

StateSet Nfa::get_useful_states_old() const
{
    if (initial.empty() || final.empty()) { return StateSet{}; }

    const Nfa digraph{get_one_letter_aut() }; // Compute reachability on directed graph.
    // Compute reachability from the initial states and use the reachable states to compute the reachability from the final states.
    const StateBoolArray useful_states_bool_array{ compute_reachability(revert(digraph), compute_reachability(digraph)) };

    const size_t num_of_states{size() };
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

    const size_t delta_size = lhs.delta.posts_size();
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

    const size_t lhs_post_size = lhs.delta.posts_size();
    for (size_t i = 0; i < lhs_post_size; i++) {
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

bool Mata::Nfa::make_complete(Nfa& aut, const Alphabet& alphabet, State sink_state) {
    return Mata::Nfa::make_complete(aut, alphabet.get_alphabet_symbols(), sink_state);
}

bool Mata::Nfa::make_complete(Nfa& aut, const Mata::Util::OrdVector<Symbol>& symbols, State sink_state) {
    bool was_something_added{ false };

    size_t num_of_states{ aut.size() };
    for (State state = 0; state < num_of_states; ++state) {
        OrdVector<Symbol> used_symbols{};
        for (auto const &move : aut.delta[state]) {
            used_symbols.insert(move.symbol);
        }
        Mata::Util::OrdVector<Symbol> unused_symbols{ symbols.difference(used_symbols) };
        for (Symbol symb : unused_symbols) {
            aut.delta.add(state, symb, sink_state);
            was_something_added = true;
        }
    }

    if (was_something_added && num_of_states <= sink_state) {
        for (Symbol symbol : symbols) {
            aut.delta.add(sink_state, symbol, sink_state);
        }
    }

    return was_something_added;
}

//TODO: based on the comments inside, this function needs to be rewritten in a more optimal way.
Nfa Mata::Nfa::remove_epsilon(const Nfa& aut, Symbol epsilon) {
    Nfa result{ aut.delta.posts_size() };

    // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
    std::unordered_map<State, StateSet> eps_closure;

    // TODO: grossly inefficient
    // first we compute the epsilon closure
    const size_t num_of_states{aut.size() };
    for (size_t i{ 0 }; i < num_of_states; ++i)
    {
        for (const auto& trans: aut.delta[i])
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
            const auto& state_transitions{ aut.delta[i] };
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
    //TODO: this is a stupid way of initialising it ...
    result.initial.add(aut.initial.get_elements());
    result.final.add(aut.final.get_elements());
    State max_state{};
    for (const auto& state_closure_pair : eps_closure) { // for every state
        State src_state = state_closure_pair.first;
        for (State eps_cl_state : state_closure_pair.second) { // for every state in its eps cl
            if (aut.final[eps_cl_state]) result.final.add(src_state);
            for (const auto& symb_set : aut.delta[eps_cl_state]) {
                if (symb_set.symbol == epsilon) continue;

                // TODO: this could be done more efficiently if we had a better add method
                for (State tgt_state : symb_set.targets) {
                    result.delta.add(src_state, symb_set.symbol, tgt_state);
                }
            }
        }
    }

    return result;
}

Nfa Mata::Nfa::fragile_revert(const Nfa& aut) {
    const size_t num_of_states{ aut.size() };

    Nfa result(num_of_states);

    result.initial = aut.final;
    result.final = aut.initial;

    //COMPUTE NON-e SYMBOLS
    //This stuff is ugly because of our handling of epsilon, what about negative numbers?
    OrdVector<Symbol> symbols = aut.get_used_symbols();
    if (symbols.empty())
        return result;
    if (symbols.back() == EPSILON)
        symbols.pop_back();
    // size of the "used alphabet", i.e. max symbol+1 or 0
    Symbol alphasize =  (symbols.empty()) ? 0 : (symbols.back()+1);

#ifdef _STATIC_STRUCTURES_
    //STATIC DATA STRUCTURES:
    // Not sure that it works ideally, whether the space for the inner vectors stays there.
    static std::vector<std::vector<State>> sources;
    static std::vector<std::vector<State>> targets;
    static std::vector<State> e_sources;
    static std::vector<State> e_targets;
    if (alphasize>sources.size()) {
        sources.resize(alphasize);
        targets.resize(alphasize);
    }

    e_sources.clear();
    e_targets.clear();

    //WHEN ONLY MAX SYMBOL IS COMPUTED
    // for (int i = 0;i<alphasize;i++) {
    //     for (int i = 0;i<alphasize;i++) {
    //         if (!sources[i].empty())
    //         {
    //             sources[i].resize(0);
    //             targets[i].resize(0);
    //         }
    //     }
    // }

    //WHEN ALL SYMBOLS ARE COMPUTED
    for (Symbol symbol: symbols) {
        if(!sources[symbol].empty()) {
            sources[symbol].clear();
            targets[symbol].clear();
        }
    }
#else
    // NORMAL, NON STATIC DATA STRUCTURES:
    //All transition of delta are to be copied here, into two arrays of transition sources and targets indexed by the transition symbol.
    // There is a special treatment for epsilon, since we want the arrays to be only as long as the largest symbol in the automaton,
    // and epsilon is the maximum (so we don't want to have the maximum array lenght whenever epsilon is present)
    std::vector<std::vector<State>> sources (alphasize);
    std::vector<std::vector<State>> targets (alphasize);
    std::vector<State> e_sources;
    std::vector<State> e_targets;
#endif

    //Copy all transition with non-e symbols to the arrays of sources and targets indexed by symbols.
    //Targets and sources of e-transitions go to the special place.
    //Important: since we are going through delta in order of sources, the sources arrays are all ordered.
    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const Move &move: aut.delta[sourceState]) {
            if (move.symbol == EPSILON) {
                for (const State targetState: move.targets) {
                    //reserve_on_insert(e_sources);
                    e_sources.push_back(sourceState);
                    //reserve_on_insert(e_targets);
                    e_targets.push_back(targetState);
                }
            }
            else {
                for (const State targetState: move.targets) {
                    //reserve_on_insert(sources[move.symbol]);
                    sources[move.symbol].push_back(sourceState);
                    //reserve_on_insert(targets[move.symbol]);
                    targets[move.symbol].push_back(targetState);
                }
            }
        }
    }

    //Now make the delta of the reversed automaton.
    //Important: since sources are ordered, when adding them as targets, we can just push them back.
    result.delta.reserve(num_of_states);

    // adding non-e transitions
    for (const Symbol symbol: symbols) {
        for (int i = 0; i<sources[symbol].size(); ++i) {
            State tgt_state =sources[symbol][i];
            State src_state =targets[symbol][i];
            Post & src_post = result.delta.get_mutable_post(src_state);
            if (src_post.empty() || src_post.back().symbol != symbol) {
                src_post.push_back(Move(symbol));
            }
            src_post.back().push_back(tgt_state);
        }
    }

    // adding e-transitions
    for (int i = 0; i<e_sources.size(); ++i) {
        State tgt_state =e_sources[i];
        State src_state =e_targets[i];
        Post & src_post = result.delta.get_mutable_post(src_state);
        if (src_post.empty() || src_post.back().symbol != EPSILON) {
            src_post.push_back(Move(EPSILON));
        }
        src_post.back().push_back(tgt_state);
    }

    //sorting the targets
    //Hm I don't know why I put this here, but it should not be needed ...
    //for (State q = 0, states_num = result.delta.post_size(); q<states_num;++q) {
    //    for (auto m = result.delta.get_mutable_post(q).begin(); m != result.delta.get_mutable_post(q).end(); ++m) {
    //        sort_and_rmdupl(m->targets);
    //    }
    //}

    return result;
}

Nfa Mata::Nfa::simple_revert(const Nfa& aut) {
    Nfa result;
    result.clear();

    const size_t num_of_states{ aut.size() };
    result.delta.increase_size(num_of_states);

    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const Move &transition: aut.delta[sourceState]) {
            for (const State targetState: transition.targets) {
                result.delta.add(targetState, transition.symbol, sourceState);
            }
        }
    }

    result.initial = aut.final;
    result.final = aut.initial;

    return result;
}

//not so great, can be removed
Nfa Mata::Nfa::somewhat_simple_revert(const Nfa& aut) {
    const size_t num_of_states{ aut.size() };

    Nfa result(num_of_states);

    result.initial = aut.final;
    result.final = aut.initial;

    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const Move &transition: aut.delta[sourceState]) {
            for (const State targetState: transition.targets) {
                Post & post = result.delta.get_mutable_post(targetState);
                //auto move = std::find(post.begin(),post.end(),Move(transition.symbol));
                auto move = post.find(Move(transition.symbol));
                if (move == post.end()) {
                    //post.push_back(Move(transition.symbol,sourceState));
                    post.insert(Move(transition.symbol,sourceState));
                }
                else
                    move->push_back(sourceState);
                    //move->insert(sourceState);
            }
        }
    }

    //sorting the targets
    for (State q = 0, states_num = result.delta.posts_size(); q < states_num; ++q) {
        Post & post = result.delta.get_mutable_post(q);
        //Util::sort_and_rmdupl(post);
        for (auto m = result.delta.get_mutable_post(q).begin(); m != result.delta.get_mutable_post(q).end(); ++m) {
            sort_and_rmdupl(m->targets);
        }
    }

    return result;
}

Nfa Mata::Nfa::revert(const Nfa& aut) {
    return simple_revert(aut);
    //return fragile_revert(aut);
    //return somewhat_simple_revert(aut);
}

bool Mata::Nfa::is_deterministic(const Nfa& aut)
{
    if (aut.initial.size() != 1) { return false; }

    if (aut.delta.empty()) { return true; }

    const size_t aut_size = aut.size();
    for (size_t i = 0; i < aut_size; ++i)
    {
        for (const auto& symStates : aut.delta[i])
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
        if (!aut.delta.empty()) {
            for (const auto &symb_stateset: aut.delta[state]) {
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

        if (!aut.delta.empty())
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

        if (aut.delta.empty())
            continue;

        for (const auto& symb_stateset : aut.delta[state])
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


Nfa Mata::Nfa::Algorithms::minimize_brzozowski(const Nfa& aut) {
    //compute the minimal deterministic automaton, Brzozovski algorithm
    return determinize(revert(determinize(revert(aut))));
}

Nfa Mata::Nfa::minimize(
                const Nfa& aut,
                const StringMap& params)
{
	Nfa result;
	// setting the default algorithm
	decltype(Algorithms::minimize_brzozowski)* algo = Algorithms::minimize_brzozowski;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(std::to_string(__func__) +
			" requires setting the \"algo\" key in the \"params\" argument; "
			"received: " + std::to_string(params));
	}

	const std::string& str_algo = params.at("algorithm");
	if ("brzozowski" == str_algo) {  /* default */ }
	else {
		throw std::runtime_error(std::to_string(__func__) +
			" received an unknown value of the \"algo\" key: " + str_algo);
	}

	return algo(aut);
}

void Nfa::print_to_DOT(std::ostream &outputStream) const {
    outputStream << "digraph finiteAutomaton {" << std::endl
                 << "node [shape=circle];" << std::endl;

    for (State finalState: final) {
        outputStream << finalState << " [shape=doublecircle];" << std::endl;
    }

    const size_t delta_size = delta.posts_size();
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

    for (State state_from{ 0 }; state_from < delta.posts_size(); ++state_from)
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
    Nfa digraph{size(), StateSet(initial), StateSet(final) };
    collect_directed_transitions(*this, abstract_symbol, digraph);
    return digraph;
}

void Nfa::get_one_letter_aut(Nfa& result) const {
    result = get_one_letter_aut();
}

TransSequence Nfa::get_transitions_to(State state_to) const {
    TransSequence transitions_to_state{};
    const size_t num_of_states{ delta.posts_size() };
    for (State state_from{ 0 }; state_from < num_of_states; ++state_from) {
        for (const auto& symbol_transitions: delta[state_from]) {
            const auto& symbol_states_to{ symbol_transitions.targets };
            const auto target_state{ symbol_states_to.find(state_to) };
            if (target_state != symbol_states_to.end()) {
                transitions_to_state.emplace_back( state_from, symbol_transitions.symbol, state_to );
            }
        }
    }
    return transitions_to_state;
}

StateSet Nfa::post(const StateSet& states, const Symbol& symbol) const {
    StateSet res{};
    if (delta.empty()) {
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
    const size_t size = lhs.size();
    for (State thisState = 0; thisState < size; ++thisState) {
        thisStateToUnionState[thisState] = unionAutomaton.add_state();
    }

    for (State thisInitialState : lhs.initial) {
        unionAutomaton.initial.add(thisStateToUnionState[thisInitialState]);
    }

    for (State thisFinalState : lhs.final) {
        unionAutomaton.final.add(thisStateToUnionState[thisFinalState]);
    }

    for (State thisState = 0; thisState < size; ++thisState) {
        State unionState = thisStateToUnionState[thisState];
        for (const Move &transitionFromThisState : lhs.delta[thisState]) {

            Move transitionFromUnionState(transitionFromThisState.symbol, StateSet{});

            for (State stateTo : transitionFromThisState.targets) {
                transitionFromUnionState.insert(thisStateToUnionState[stateTo]);
            }

            unionAutomaton.delta.get_mutable_post(unionState).insert(transitionFromUnionState);
        }
    }

    return unionAutomaton;
}

Simlib::Util::BinaryRelation Mata::Nfa::Algorithms::compute_relation(const Nfa& aut, const StringMap& params) {
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

Nfa Mata::Nfa::reduce(const Nfa &aut, bool trim_input, StateToStateMap *state_map, const StringMap& params) {
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algorithm\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    Nfa aut_to_reduce{ aut };
    StateToStateMap trimmed_state_map{};
    if (trim_input) {
        aut_to_reduce.trim(&trimmed_state_map);
    }

    Nfa result;
    std::unordered_map<State,State> reduced_state_map;
    const std::string& algorithm = params.at("algorithm");
    if ("simulation" == algorithm) {
        result = reduce_size_by_simulation(aut_to_reduce, reduced_state_map);
    } else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algorithm\" key: " + algorithm);
    }

    if (state_map) {
        state_map->clear();
        if (trim_input) {
            for (const auto& trimmed_mapping : trimmed_state_map) {
                const auto reduced_mapping{ reduced_state_map.find(trimmed_mapping.second) };
                if (reduced_mapping != reduced_state_map.end()) {
                    (*state_map)[trimmed_mapping.first] = reduced_mapping->second;
                }
            }
        } else { // Input has not been trimmed, the reduced state map is the actual input to result state map.
            *state_map = reduced_state_map;
        }
    }

    return result;
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

    if (aut.delta.empty())
        return result;

    using Iterator = Mata::Util::OrdVector<Move>::const_iterator;
    Mata::Util::SynchronizedExistentialIterator<Iterator> synchronized_iterator;

    while (!worklist.empty()) {
        const auto Spair = worklist.back();
        worklist.pop_back();
        const StateSet S = Spair.second;
        const State Sid = Spair.first;
        if (S.empty()) {
            // This should not happen assuming all sets targets are non-empty.
            break;
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
            result.delta.get_mutable_post(Sid).insert(Move(currentSymbol, Tid));
        }
    }

    if (deallocate_subset_map) { delete subset_map; }

    return result;
}

// TODO this function should the same thing as the one taking IntermediateAut or be deleted
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

        aut.delta.add(src_state, symbol, tgt_state);
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

    StringToStateMap tmp_state_map;
    if (nullptr == state_map) {
        state_map = &tmp_state_map;
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

    for (const auto& str : inter_aut.initial_formula.collect_node_names())
    {
        State state = get_state_name(str);
        aut.initial.add(state);
    }

    for (const auto& trans : inter_aut.transitions)
    {
        if (trans.second.children.size() != 2)
        {
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

        aut.delta.add(src_state, symbol, tgt_state);
    }

    std::unordered_set<std::string> final_formula_nodes;
    if (!(inter_aut.final_formula.node.is_constant())) {
        // we do not want to parse true/false (constant) as a state so we do not collect it
        final_formula_nodes = inter_aut.final_formula.collect_node_names();
    }
    // for constant true, we will pretend that final nodes are negated with empty final_formula_nodes
    bool final_nodes_are_negated = (inter_aut.final_formula.node.is_true() || inter_aut.are_final_states_conjunction_of_negation());

    if (final_nodes_are_negated) {
        // we add all states NOT in final_formula_nodes to final states
        for (const auto &state_name_and_id : *state_map) {
            if (!final_formula_nodes.count(state_name_and_id.first)) {
                aut.final.add(state_name_and_id.second);
            }
        }
    } else {
        // we add all states in final_formula_nodes to final states
        for (const auto& str : final_formula_nodes)
        {
            State state = get_state_name(str);
            aut.final.add(state);
        }
    }

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

    for (size_t trIt{ 0 }; trIt < nfa->delta.posts_size(); ++trIt) {
        auto& moves{ nfa->get_moves_from(trIt) };
        if (!moves.empty()) {
            auto move{ moves.begin() };
            while (move != moves.end()) {
                if (!move->targets.empty()) {
                    result.trIt = trIt;
                    result.tlIt = moves.begin();
                    result.ssIt = result.tlIt->targets.begin();
                    break;
                }
                ++move;
            }
            break;
        }
    }

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

    while (this->trIt < this->nfa->delta.posts_size() &&
           this->nfa->get_moves_from(this->trIt).empty())
    {
        ++this->trIt;
    }

    if (this->trIt < this->nfa->delta.posts_size())
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

// Other versions, maybe an interesting experiment with speed of data structures.
// returns symbols appearing in Delta, pushes back to vector and then sorts
Mata::Util::OrdVector<Symbol> Nfa::get_used_symbols_vec() const {
#ifdef _STATIC_STRUCTURES_
    static std::vector<Symbol>  symbols{};
    symbols.clear();
#else
    std::vector<Symbol>  symbols{};
#endif
    for (State q = 0; q< delta.posts_size(); ++q) {
        const Post & post = delta[q];
        for (const Move & move: post) {
            Util::reserve_on_insert(symbols);
            symbols.push_back(move.symbol);
        }
    }
    Util::OrdVector<Symbol>  sorted_symbols(symbols);
    return sorted_symbols;
}

// returns symbols appearing in Delta, inserts to a std::set
std::set<Symbol> Nfa::get_used_symbols_set() const {
    //static should prevent reallocation, seems to speed things up a little
#ifdef _STATIC_STRUCTURES_
    static std::set<Symbol>  symbols;
    symbols.clear();
#else
    static std::set<Symbol>  symbols{};
#endif
    for (State q = 0; q< delta.posts_size(); ++q) {
        const Post & post = delta[q];
        for (const Move & move: post) {
            symbols.insert(move.symbol);
        }
    }
    return symbols;
    //Util::OrdVector<Symbol>  sorted_symbols(symbols.begin(),symbols.end());
    //return sorted_symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
Mata::Util::NumberPredicate<Symbol> Nfa::get_used_symbols_np() const {
#ifdef _STATIC_STRUCTURES_
    //static seems to speed things up a little
    static Util::NumberPredicate<Symbol>  symbols(64,false,false);
    symbols.clear();
#else
    Util::NumberPredicate<Symbol>  symbols(64,false,false);
#endif
    //symbols.dont_track_elements();
    for (State q = 0; q< delta.posts_size(); ++q) {
        const Post & post = delta[q];
        for (const Move & move: post) {
            symbols.add(move.symbol);
        }
    }
    //TODO: is it neccessary toreturn ordered vector? Would the number predicate suffice?
    return symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
std::vector<bool> Nfa::get_used_symbols_bv() const {
#ifdef _STATIC_STRUCTURES_
    //static seems to speed things up a little
    static std::vector<bool>  symbols(64,false);
    symbols.clear();
#else
    std::vector<bool> symbols(64,false);
#endif
    //symbols.dont_track_elements();
    for (State q = 0; q< delta.posts_size(); ++q) {
        const Post & post = delta[q];
        for (const Move & move: post) {
            reserve_on_insert(symbols,move.symbol);
            symbols[move.symbol]=true;
        }
    }
    //TODO: is it neccessary toreturn ordered vector? Would the number predicate suffice?
    return symbols;
}

// returns max non-e symbol in Delta
Symbol Nfa::get_max_symbol() const {
    Symbol max = 0;
    for (State q = 0; q< delta.posts_size(); ++q) {
        const Post & post = delta[q];
        for (const Move & move: post) {
            if (move.symbol > max)
                max = move.symbol;
        }
    }
    return max;
}

 void Nfa::unify_initial() {
    if (initial.empty() || initial.size() == 1) { return; }
    const State new_initial_state{add_state() };
    for (const auto& orig_initial_state: initial) {
        const auto moves{ get_moves_from(orig_initial_state) };
        for (const auto& transitions: moves) {
            for (const State state_to: transitions.targets) {
                delta.add(new_initial_state, transitions.symbol, state_to);
            }
        }
        if (final[orig_initial_state]) { final.add(new_initial_state); }
    }
    initial.clear();
    initial.add(new_initial_state);
}

void Nfa::unify_final() {
    if (final.empty() || final.size() == 1) { return; }
    const State new_final_state{ add_state() };
    for (const auto& orig_final_state: final) {
        const auto transitions_to{ get_transitions_to(orig_final_state) };
        for (const auto& transitions: transitions_to) {
            delta.add(transitions.src, transitions.symb, new_final_state);
        }
        if (initial[orig_final_state]) { initial.add(new_final_state); }
    }
    final.clear();
    final.add(new_final_state);
}

void Mata::Nfa::fill_alphabet(const Mata::Nfa::Nfa& nfa, OnTheFlyAlphabet& alphabet) {
    size_t nfa_num_of_states{nfa.size() };
    for (Mata::Nfa::State state{ 0 }; state < nfa_num_of_states; ++state) {
        for (const auto state_transitions: nfa.delta) {
            alphabet.update_next_symbol_value(state_transitions.symb);
            alphabet.try_add_new_symbol(std::to_string(state_transitions.symb), state_transitions.symb);
        }
    }
}

void Nfa::add_symbols_to(OnTheFlyAlphabet& alphabet) const {
    size_t aut_num_of_states{size() };
    for (Mata::Nfa::State state{ 0 }; state < aut_num_of_states; ++state) {
        for (const auto& state_transitions: delta[state]) {
            alphabet.update_next_symbol_value(state_transitions.symbol);
            alphabet.try_add_new_symbol(std::to_string(state_transitions.symbol), state_transitions.symbol);
        }
    }
}

Mata::OnTheFlyAlphabet Mata::Nfa::create_alphabet(const ConstAutRefSequence& nfas) {
    Mata::OnTheFlyAlphabet alphabet{};
    for (const auto& nfa: nfas) {
        fill_alphabet(nfa, alphabet);
    }
    return alphabet;
}

Mata::OnTheFlyAlphabet Mata::Nfa::create_alphabet(const AutRefSequence& nfas) {
    Mata::OnTheFlyAlphabet alphabet{};
    for (const auto& nfa: nfas) {
        fill_alphabet(nfa, alphabet);
    }
    return alphabet;
}

Mata::OnTheFlyAlphabet Mata::Nfa::create_alphabet(const ConstAutPtrSequence& nfas) {
    Mata::OnTheFlyAlphabet alphabet{};
    for (const Mata::Nfa::Nfa* const nfa: nfas) {
        fill_alphabet(*nfa, alphabet);
    }
    return alphabet;
}

Mata::OnTheFlyAlphabet Mata::Nfa::create_alphabet(const AutPtrSequence& nfas) {
    Mata::OnTheFlyAlphabet alphabet{};
    for (const Mata::Nfa::Nfa* const nfa: nfas) {
        fill_alphabet(*nfa, alphabet);
    }
    return alphabet;
}

Nfa Mata::Nfa::create_empty_string_nfa() {
    return Nfa{ 1, StateSet{ 0 }, StateSet{ 0 } };
}

Nfa Mata::Nfa::create_sigma_star_nfa(Mata::Alphabet* alphabet) {
    Nfa nfa{ 1, StateSet{ 0 }, StateSet{ 0 }, alphabet };
    for (const Mata::Symbol& symbol : alphabet->get_alphabet_symbols()) {
        nfa.delta.add(0, symbol, 0);
    }
    return nfa;
}
