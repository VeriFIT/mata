/* noodlify.cc -- Noodlification of NFAs
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <mata/nfa.hh>
#include <mata/noodlify.hh>

using namespace Mata::Nfa;

namespace
{

/**
 * Get a number of permutations for computed epsilon depths.
 * @param[in] epsilon_depths Computed list of epsilon transitions for each depth.
 * @return Number of permutations.
 */
size_t get_num_of_permutations(const SegNfa::Segmentation::EpsilonDepthTransitions& epsilon_depths)
{
    size_t num_of_permutations{ 1 };
    for (const auto& segment: epsilon_depths)
    {
        num_of_permutations *= segment.second.size();
    }
    return num_of_permutations;
}

/**
 * Create noodles for computed epsilon depths.
 * @param[in] aut Segment automaton to create noodles for.
 * @param[in] include_empty Whether to also include empty noodles.
 * @param[in] epsilon_depths Computed list of epsilon transitions for each depth.
 * @return A Sequence of noodles (noodle automata).
 */
std::vector<std::vector<Nfa*>> create_noodles(const SegNfa::SegNfa& aut,
                           const SegNfa::Segmentation::EpsilonDepthTransitions& epsilon_depths)
{
    // Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
    size_t num_of_permutations{ get_num_of_permutations(epsilon_depths) };
    int epsilon_depths_size{ static_cast<int>(epsilon_depths.size()) };
    int maximal_depth{ epsilon_depths_size - 1 };
    AutSequence noodles{}; // Store the generated noodles.
    Nfa noodle{}; // Noodle created for each combination of epsilon transitions.
    // Indices of a transition in each depth to compute the noodle for.
    std::vector<size_t> transition_indices(epsilon_depths_size, 0);
    // For each combination of ε-transitions, create the automaton.
    for (size_t index{ 0 }; index < num_of_permutations; ++index)
    {
        for (int depth{ maximal_depth }; depth >= 0; --depth)
        {
            size_t computed_index{ index };
            for (int previous_depth{ 0 }; previous_depth < depth; ++previous_depth)
            {
                computed_index /= epsilon_depths.at(previous_depth).size();
            }

            transition_indices.at(depth) = computed_index % epsilon_depths.at(depth).size();
        }

        noodle = aut;
        for (int depth{ 0 }; depth < epsilon_depths_size; ++depth)
        {
            for (const auto& transition: epsilon_depths.at(depth))
            {
                // Remove all ε-transitions of depth 'i' except for 'noodle_trans[i]'.
                if (epsilon_depths.at(depth).at(transition_indices.at(depth)) != transition)
                {
                    noodle.remove_trans(transition);
                }
            }
        }
        noodle.trim();
        if (noodle.get_num_of_states() > 0) { noodles.push_back(noodle); }
    }
    //return noodles;
    return {};
}

} // namespace

std::vector<std::vector<std::shared_ptr<Nfa>>> SegNfa::noodlify(const SegNfa& aut, const Symbol epsilon)
{
    // For each depth, get a list of epsilon transitions.
    Segmentation segmentation{ aut, epsilon };
    const auto &segments{ segmentation.get_segments_raw() };

    if (segments.size() == 1) {
        std::shared_ptr<Nfa> segment = std::make_shared<Nfa>(segments[0]);
        segment->trim();
        return {{segment}};
    }

    State unused_state = aut.get_num_of_states();

    // segments_one_initial_final[init, final] is the pointer to automaton created from one of
    // the segments such that init and final are the initial and final states of the segment and
    // the created automaton takes this segment, sets initialstates={init}, finalstates={final}
    // and trims it; also segments_one_initial_final[unused_state, final] is used for the first
    // segment (where we always want all initial states, only final changes) and
    // segments_one_initial_final[init, unused_state] is similarly for the last segment
    // TODO: should we use unordered_map? then we need hash
    std::map<std::pair<State,State>,std::shared_ptr<Nfa>> segments_one_initial_final;

    // TODO this could probably be written better
    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        if (iter == segments.begin()) {
            for (const State final_state : iter->finalstates) {
                std::shared_ptr<Nfa> segment_one_final = std::make_shared<Nfa>(*iter);
                segment_one_final->finalstates = {final_state};
                segment_one_final->trim();

                if (segment_one_final->get_num_of_states() > 0) {
                    segments_one_initial_final[std::make_pair(unused_state,final_state)] = segment_one_final;
                }
            }
        } else if (iter + 1 == segments.end()) {
            for (const State init_state : iter->initialstates) {
                std::shared_ptr<Nfa> segment_one_init = std::make_shared<Nfa>(*iter);
                segment_one_init->initialstates = {init_state};
                segment_one_init->trim();

                if (segment_one_init->get_num_of_states() > 0) {
                    segments_one_initial_final[std::make_pair(init_state,unused_state)] = segment_one_init;
                }
            }
        } else {
            for (const State init_state : iter->initialstates) {
                for (const State final_state : iter->finalstates) {
                    std::shared_ptr<Nfa> segment_one_init_final = std::make_shared<Nfa>(*iter);
                    segment_one_init_final->initialstates = {init_state};
                    segment_one_init_final->finalstates = {final_state};
                    segment_one_init_final->trim();

                    if (segment_one_init_final->get_num_of_states() > 0) {
                        segments_one_initial_final[std::make_pair(init_state,final_state)] = segment_one_init_final;
                    }
                }
            }
        }
    }

    const auto &epsilon_depths{ segmentation.get_epsilon_depths() };

    // Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
    size_t num_of_permutations{ get_num_of_permutations(epsilon_depths) };
    size_t epsilon_depths_size{  epsilon_depths.size() };

    std::vector<std::vector<std::shared_ptr<Nfa>>> noodles{};
    // noodle of epsilon transitions (each from different depth)
    std::vector<Trans> epsilon_noodle(epsilon_depths_size);
    // for each combination of ε-transitions, create the automaton.
    // based on https://stackoverflow.com/questions/48270565/create-all-possible-combinations-of-multiple-vectors
    for (size_t index{ 0 }; index < num_of_permutations; ++index)
    {
        size_t temp{ index };
        for (size_t depth{ 0 }; depth < epsilon_depths_size; ++depth) {
            size_t num_of_trans_at_cur_depth = epsilon_depths.at(depth).size();
            size_t computed_index = temp % num_of_trans_at_cur_depth;
            temp /= num_of_trans_at_cur_depth;
            epsilon_noodle[depth] = epsilon_depths.at(depth)[computed_index];
        }

        std::vector<std::shared_ptr<Nfa>> noodle;

        // epsilon_noodle[0] for sure exists, as we sorted out the case of only one segment at the beginning
        auto first_segment_iter = segments_one_initial_final.find(std::make_pair(unused_state,epsilon_noodle[0].src));
        if (first_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(first_segment_iter->second);
        } else {
            continue;
        }
        
        bool all_segments_exist = true;
        for (auto iter = epsilon_noodle.begin(); iter + 1 != epsilon_noodle.end(); ++iter) {
            auto next_iter = iter + 1;
            auto segment_iter = segments_one_initial_final.find(std::make_pair(iter->tgt,next_iter->src));
            if (segment_iter != segments_one_initial_final.end()) {
                noodle.push_back(segment_iter->second);
            } else {
                all_segments_exist = false;
                break;
            }
        }

        if (!all_segments_exist) {
            continue;
        }

        auto last_segment_iter = segments_one_initial_final.find(std::make_pair(epsilon_noodle.back().tgt, unused_state));
        if (last_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(last_segment_iter->second);
        } else {
            continue;
        }

        noodles.push_back(noodle);
    }
    return noodles;

    // Create noodles for computed epsilon depths.
    //return create_noodles(aut, epsilon_depths);
}

