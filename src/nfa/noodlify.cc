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

AutSequence SegNfa::noodlify(SegNfa& aut, const Symbol epsilon, bool include_empty)
{
    AutSequence noodles{}; // Store the generated noodles.

    // For each depth, get a list of epsilon transitions.
    Segmentation segmentation{ aut, epsilon };
    const auto& epsilon_depths{ segmentation.get_epsilon_depths() };

    Nfa noodle{}; // Noodle created for each combination of epsilon transitions.

    // Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
    size_t num_of_permutations{ 1 };
    for (const auto& segment: epsilon_depths)
    {
        num_of_permutations *= segment.second.size();
    }

    // Indices of a transition in each depth to compute the noodle for.
    std::vector<size_t> transition_indices(epsilon_depths.size(), 0);

    // For each combination of ε-transitions, create the automaton.
    for (size_t index{ 0 }; index < num_of_permutations; ++index) {
        for (int depth{ static_cast<int>(epsilon_depths.size()) - 1 }; depth >= 0; --depth)
        {
            size_t computed_index{ index };
            for (int previous_depth{0}; previous_depth < depth; ++previous_depth)
            {
                computed_index /= epsilon_depths.at(previous_depth).size();
            }

            transition_indices.at(depth) = computed_index % epsilon_depths.at(depth).size();
        }

        noodle = aut;

        for (int depth{ 0 }; depth < static_cast<int>(epsilon_depths.size()); ++depth)
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

        if (include_empty || noodle.get_num_of_states() > 0)
        {
            noodles.push_back(noodle);
        }
    }

    return noodles;
}
