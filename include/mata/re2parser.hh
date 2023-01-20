/* re2parser.hh -- parser transforming re2 regular expressions to our Nfa
 *
 * Copyright (c) 2022 Michal Horky
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

#ifndef MATA_RE2PARSER_HH
#define MATA_RE2PARSER_HH

#include <string>
#include <mata/nfa.hh>

namespace Mata {
    namespace RE2Parser {
        void create_nfa(Nfa::Nfa* nfa, const std::string &pattern, bool use_epsilon = false, int epsilon_value = 306, bool use_reduction = true);
    }
}

#endif // MATA_RE2PARSER_HH
