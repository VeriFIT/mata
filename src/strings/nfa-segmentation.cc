/* nfa-segmentation.cc -- Segmentation of NFAs
 *
 * Copyright (c) 2022 David Chocholatý <chocholaty.david@protonmail.com>
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

#include "mata/nfa-strings.hh"

using namespace Mata::Nfa;
using namespace Mata::Strings;

void SegNfa::Segmentation::process_state_depth_pair(const StateDepthTuple& state_depth_pair,
                                                    std::deque<StateDepthTuple>& worklist)
{
    auto outgoing_transitions{automaton.get_moves_from(state_depth_pair.state) };
    for (const auto& state_transitions: outgoing_transitions)
    {
        if (this->epsilons.find(state_transitions.symbol) != this->epsilons.end())
        {
            handle_epsilon_transitions(state_depth_pair, state_transitions, worklist);
        }
        else // Handle other transitions.
        {
            add_transitions_to_worklist(state_depth_pair, state_transitions, worklist);
        }
    }
}

void SegNfa::Segmentation::handle_epsilon_transitions(const StateDepthTuple& state_depth_pair,
                                                      const Move& state_transitions,
                                                      std::deque<StateDepthTuple>& worklist)
{
    /// TODO: Maybe we don't need to keep the transitions in both structures
    this->epsilon_depth_transitions.insert(std::make_pair(state_depth_pair.depth, TransSequence{}));
    this->eps_depth_trans_map.insert({ state_depth_pair.depth, {{state_depth_pair.state, TransSequence{}}} });

    std::map<Symbol, unsigned> visited_eps_aux(state_depth_pair.eps);
    visited_eps_aux[state_transitions.symbol]++; 

    for (State target_state: state_transitions.targets)
    {
        this->epsilon_depth_transitions[state_depth_pair.depth].push_back(
                Trans{ state_depth_pair.state, state_transitions.symbol, target_state }
        );
        this->eps_depth_trans_map[state_depth_pair.depth][state_depth_pair.state].push_back(
            Trans{ state_depth_pair.state, state_transitions.symbol, target_state }
        );
        worklist.push_back(StateDepthTuple{ target_state, state_depth_pair.depth + 1, visited_eps_aux });
        this->visited_eps[target_state] = visited_eps_aux;
    }
}

void SegNfa::Segmentation::add_transitions_to_worklist(const StateDepthTuple& state_depth_pair,
                                                      const Move& state_transitions,
                                                      std::deque<StateDepthTuple>& worklist)
{
    for (State target_state: state_transitions.targets)
    {
        worklist.push_back(StateDepthTuple{ target_state, state_depth_pair.depth, state_depth_pair.eps });
        this->visited_eps[target_state] = state_depth_pair.eps;
    }
}

std::deque<SegNfa::Segmentation::StateDepthTuple> SegNfa::Segmentation::initialize_worklist() const
{
    std::deque<StateDepthTuple> worklist{};
    std::map<Symbol, unsigned> def_eps_map{};
    for(const Symbol& e : this->epsilons) {
        def_eps_map[e] = 0;
    }

    for (State state: automaton.initial)
    {
        worklist.push_back(StateDepthTuple{ state, 0, def_eps_map });
    }
    return worklist;
}

std::unordered_map<State, bool> SegNfa::Segmentation::initialize_visited_map() const
{
    std::unordered_map<State, bool> visited{};
    const size_t state_num = automaton.size();
    for (State state{ 0 }; state < state_num; ++state)
    {
        visited[state] = false;
    }
    return visited;
}

void SegNfa::Segmentation::split_aut_into_segments()
{
    segments_raw = AutSequence{ epsilon_depth_transitions.size() + 1, automaton };
    remove_inner_initial_and_final_states();

    // Construct segment automata.
    std::unique_ptr<const TransSequence> depth_transitions{};
    for (size_t depth{ 0 }; depth < epsilon_depth_transitions.size(); ++depth)
    {
        // Split the left segment from automaton into a new segment.
        depth_transitions = std::make_unique<const TransSequence>(epsilon_depth_transitions[depth]);
        for (const auto& transition: *depth_transitions)
        {
            update_current_segment(depth, transition);
            update_next_segment(depth, transition);
        }
    }
}

void SegNfa::Segmentation::remove_inner_initial_and_final_states() {
    const auto segments_begin{ segments_raw.begin() };
    const auto segments_end{ segments_raw.end() };
    for (auto iter{ segments_begin }; iter != segments_end; ++iter) {
        if (iter != segments_begin) {
            iter->initial.clear();
        }
        if (iter + 1 != segments_end) {
            iter->final.clear();
        }
    }
}

void SegNfa::Segmentation::update_current_segment(const size_t current_depth, const Trans& transition)
{
    assert(this->epsilons.find(transition.symb) != this->epsilons.end());
    assert(segments_raw[current_depth].delta.contains(transition.src, transition.symb, transition.tgt));

    segments_raw[current_depth].final.add(transition.src);
    // we need to remove this transition so that the language of the current segment does not accept too much
    segments_raw[current_depth].delta.remove(transition);
}

void SegNfa::Segmentation::update_next_segment(const size_t current_depth, const Trans& transition)
{
    const size_t next_depth = current_depth + 1;

    assert(this->epsilons.find(transition.symb) != this->epsilons.end());
    assert(segments_raw[next_depth].delta.contains(transition.src, transition.symb, transition.tgt));

    // we do not need to remove epsilon transitions in current_depth from the next segment (or the
    // segments after) as the initial states are after these transitions
    segments_raw[next_depth].initial.add(transition.tgt);
}

const AutSequence& SegNfa::Segmentation::get_segments()
{
    if (segments.empty()) {
        get_untrimmed_segments();
        for (auto& seg_aut: segments_raw) { segments.push_back(seg_aut.get_trimmed_automaton()); }
    }

    return segments;
}

const AutSequence& SegNfa::Segmentation::get_untrimmed_segments()
{
    if (segments_raw.empty()) { split_aut_into_segments(); }

    return segments_raw;
}

void SegNfa::Segmentation::compute_epsilon_depths()
{
    std::unordered_map<State, bool> visited{ initialize_visited_map() };
    std::deque<StateDepthTuple> worklist{ initialize_worklist() };

    while (!worklist.empty())
    {
        StateDepthTuple state_depth_pair{ worklist.front() };
        worklist.pop_front();

        if (!visited[state_depth_pair.state])
        {
            visited[state_depth_pair.state] = true;
            process_state_depth_pair(state_depth_pair, worklist);
        }
    }
}
