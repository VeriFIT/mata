/* re2parser.hh -- parser transforming re2 regular expressions to our Nfa
 *
 * Copyright (c) 2022 Michal Horky
 *
 * This file is a part of libvata2.
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

#ifndef VATA2_RE2PARSER_HH
#define VATA2_RE2PARSER_HH

#include <string>
#include <vata2/nfa.hh>

namespace Vata2 {
    namespace RE2Parser {
        Vata2::Nfa::Nfa create_nfa(const std::string &pattern);
    }
}

#endif // VATA2_RE2PARSER
