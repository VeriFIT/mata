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

/**
 * @brief Create noodles from segment automaton @p aut.
 *
 * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
 * A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.
 *
 * @param[in] automaton Segment automaton to noodlify.
 * @param[in] epsilon Epsilon symbol to noodlify for.
 * @param[in] include_empty Whether to also include empty noodles.
 * @return A list of all (non-empty) noodles.
 */
std::vector<std::vector<std::shared_ptr<Nfa>>> noodlify(const SegNfa& aut, Symbol epsilon);

} // SegNfa
} // Nfa
} // Mata

#endif // _MATA_NOODLIFY_HH
