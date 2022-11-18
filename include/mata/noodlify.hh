/* noodlify.hh -- Noodlification of NFAs
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

#ifndef _MATA_NOODLIFY_HH
#define _MATA_NOODLIFY_HH

#include <memory>

#include <mata/nfa.hh>

namespace Mata
{
namespace Nfa
{
namespace SegNfa
{

/// A noodle is represented as a sequence of segments (a copy of the segment automata) created as if there was exactly
///  one ε-transition between each two consecutive segments.
using Noodle = std::vector<SharedPtrAut>;
using NoodleSequence = std::vector<Noodle>; ///< A sequence of noodles.

/**
 * @brief Create noodles from segment automaton @p aut.
 *
 * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
 * A noodle is a vector of pointers to copy of the segments automata created as if there was exactly one ε-transition
 *  between each two consecutive segments.
 *
 * @param[in] automaton Segment automaton to noodlify.
 * @param[in] epsilon Epsilon symbol to noodlify for.
 * @param[in] include_empty Whether to also include empty noodles.
 * @return A list of all (non-empty) noodles.
 */
NoodleSequence noodlify(const SegNfa& aut, Symbol epsilon, bool include_empty = false);

/**
 * @brief Create noodles for left and right side of equation.
 *
 * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
 * A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.
 *
 * Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
 *  the equation in a way that libMata understands. The left side automata represent the left side of the equation
 *  and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
 *  representing the intersection. That can be achieved by computing a product of both sides. First, the left side
 *  has to be concatenated over an epsilon transitions into a single automaton to compute the intersection on, though.
 *
 * @param[in] left_automata Sequence of segment automata for left side of an equation to noodlify.
 * @param[in] right_automaton Segment automaton for right side of an equation to noodlify.
 * @param[in] include_empty Whether to also include empty noodles.
 * @param[in] params Additional parameters for the noodlification:
 *     - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
 *                 minimization before noodlification.
 * @return A list of all (non-empty) noodles.
 */
NoodleSequence noodlify_for_equation(const AutRefSequence& left_automata, const Nfa& right_automaton,
                                     bool include_empty = false, const StringMap& params = {{"reduce", "false"}});

/**
 * @brief Create noodles for left and right side of equation.
 *
 * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
 * A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.
 *
 * Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
 *  the equation in a way that libMata understands. The left side automata represent the left side of the equation
 *  and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
 *  representing the intersection. That can be achieved by computing a product of both sides. First, the left side
 *  has to be concatenated over an epsilon transitions into a single automaton to compute the intersection on, though.
 *
 * @param[in] left_automata Sequence of pointers to segment automata for left side of an equation to noodlify.
 * @param[in] right_automaton Segment automaton for right side of an equation to noodlify.
 * @param[in] include_empty Whether to also include empty noodles.
 * @param[in] params Additional parameters for the noodlification:
 *     - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
 *                 minimization before noodlification.
 * @return A list of all (non-empty) noodles.
 */
NoodleSequence noodlify_for_equation(const AutPtrSequence& left_automata, const Nfa& right_automaton,
                                     bool include_empty = false, const StringMap& params = {{"reduce", "false"}});
} // SegNfa
} // Nfa
} // Mata

#endif // _MATA_NOODLIFY_HH
