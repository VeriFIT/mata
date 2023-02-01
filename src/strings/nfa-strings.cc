/* nfa-strings.cc -- Operations on NFAs for string solving.
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

WordSet Mata::Strings::get_shortest_words(const Nfa::Nfa& nfa) {
    // Map mapping states to a set of the shortest words accepted by the automaton from the mapped state.
    // Get the shortest words for all initial states accepted by the whole automaton (not just a part of the automaton).
    return Strings::ShortestWordsMap{ nfa }.get_shortest_words_for(nfa.initial);
}

WordSet ShortestWordsMap::get_shortest_words_for(const StateSet& states) const
{
    WordSet result{};

    if (!shortest_words_map.empty())
    {
        WordLength shortest_words_length{-1};

        for (const State state: states)
        {
            const auto& current_shortest_words_map{shortest_words_map.find(state)};
            if (current_shortest_words_map == shortest_words_map.end()) {
                continue;
            }

            const auto& state_shortest_words_map{current_shortest_words_map->second};
            if (result.empty() || state_shortest_words_map.first < shortest_words_length) // Find a new set of the shortest words.
            {
                result = state_shortest_words_map.second;
                shortest_words_length = state_shortest_words_map.first;
            }
            else if (state_shortest_words_map.first == shortest_words_length)
            {
                // Append the shortest words from other state of the same length to the already found set of the shortest words.
                result.insert(state_shortest_words_map.second.begin(),
                              state_shortest_words_map.second.end());
            }
        }

    }

    return result;
}

WordSet ShortestWordsMap::get_shortest_words_for(State state) const
{
    return get_shortest_words_for(StateSet{ state });
}

void ShortestWordsMap::insert_initial_lengths()
{
    const auto initial_states{ reversed_automaton.initial };
    if (!initial_states.empty())
    {
        for (const State state: initial_states)
        {
            shortest_words_map.insert(std::make_pair(state, std::make_pair(0,
                                                                           WordSet{ std::vector<Symbol>{} })));
        }

        const auto initial_states_begin{ initial_states.begin() };
        const auto initial_states_end{ initial_states.end() };
        processed.insert(initial_states_begin, initial_states_end);
        fifo_queue.insert(fifo_queue.end(), initial_states_begin,
                          initial_states_end);
    }
}

void ShortestWordsMap::compute()
{
    State state{};
    while (!fifo_queue.empty())
    {
        state = fifo_queue.front();
        fifo_queue.pop_front();

        // Compute the shortest words for the current state.
        compute_for_state(state);
    }
}

void ShortestWordsMap::compute_for_state(const State state)
{
    const LengthWordsPair& dst{ map_default_shortest_words(state) };
    const WordLength dst_length_plus_one{ dst.first + 1 };
    LengthWordsPair act;

    for (const Move& transition: reversed_automaton.get_moves_from(state))
    {
        for (const State state_to: transition.targets)
        {
            const LengthWordsPair& orig{ map_default_shortest_words(state_to) };
            act = orig;

            if ((act.first == -1) || (dst_length_plus_one < act.first))
            {
                // Found new shortest words after appending transition symbols.
                act.second.clear();
                update_current_words(act, dst, transition.symbol);
            }
            else if (dst_length_plus_one == act.first)
            {
                // Append transition symbol to increase length of the shortest words.
                update_current_words(act, dst, transition.symbol);
            }

            if (orig.second != act.second)
            {
                shortest_words_map[state_to] = act;
            }

            if (processed.find(state_to) == processed.end())
            {
                processed.insert(state_to);
                fifo_queue.push_back(state_to);
            }
        }
    }
}

void ShortestWordsMap::update_current_words(LengthWordsPair& act, const LengthWordsPair& dst, const Symbol symbol)
{
    for (auto word : dst.second)
    {
        word.insert(word.begin(), symbol);
        act.second.insert(word);
    }
    act.first = dst.first + 1;
}


std::set<std::pair<int, int>> Mata::Strings::get_word_lengths(const Nfa::Nfa& aut) {
    Nfa::Nfa one_letter;
    /// if we are interested in lengths of words, it suffices to forget the different symbols on transitions. 
    /// The lengths of @p aut are hence equivalent to lengths of the NFA taken from @p aut where all symbols on
    /// transitions are renamed to a single symbol (e.g., `a`).
    aut.get_one_letter_aut(one_letter);
    one_letter = determinize(one_letter);
    one_letter.trim();
    if(one_letter.size() == 0) {
        return {};
    } 

    std::set<std::pair<int, int>> ret;
    std::vector<int> handles(one_letter.size(), 0); // initialized to 0
    assert(one_letter.initial.size() == 1);
    std::optional<Nfa::State> curr_state = *one_letter.initial.begin();
    std::set<Nfa::State> visited;
    int cnt = 0; // handle counter
    int loop_size = 0; // loop size
    int loop_start = -1; // cnt where the loop starts

    while(curr_state.has_value()) {
        visited.insert(curr_state.value());
        handles[curr_state.value()] = cnt++;
        Nfa::Post post = one_letter.delta[curr_state.value()];

        curr_state.reset();
        assert(post.size() <= 1);
        for(const Move& move : post) {
            assert(move.targets.size() == 1);
            Nfa::State tgt = *move.targets.begin();

            if(visited.find(tgt) == visited.end()) {
                curr_state = tgt;
            } else {
                curr_state.reset();
                loop_start = handles[tgt];
                loop_size = cnt - handles[tgt];
            }
        }
    }
    for(const Nfa::State& fin : one_letter.final) {
        if(handles[fin] >= loop_start) {
            ret.insert({handles[fin], loop_size});
        } else {
            ret.insert({handles[fin], 0});
        }
    }

    return ret;
}
