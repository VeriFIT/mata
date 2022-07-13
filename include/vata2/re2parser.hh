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
#include <re2/re2.h>

namespace Vata2 {
    class RegexParser {
    private:
        /**
         * Holds all state cache vectors needed throughout the computation. Vector index is the state number
         * state_mapping for each state (vector index), it holds a vector of states that map to it (cause by epsilon transitions)
         * is_final_state determines if the state is final (true) or not (false)
         * is_state_nop_or_cap determines if the state is of type nop/cap (true) or not (false)
         * is_last determines if the state is last (true), meaning it has epsilon transition to the next state, or not (false)
         * has_state_incoming_edge determines if there is an incoming edge to the state (true) or not (false)
         * has_state_outgoing_back_edge determines if there is any outgoing edge leading to a state with a lower number (true) or not (false)
         */
        struct StateCache {
            std::vector<std::vector<int>> state_mapping;
            std::vector<bool> is_final_state;
            std::vector<bool> is_state_nop_or_cap;
            std::vector<bool> is_last;
            std::vector<bool> has_state_incoming_edge;
            std::vector<bool> has_state_outgoing_back_edge;
        };

        /**
         * Default RE2 options
         */
        RE2::Options options;
        StateCache state_cache;

        re2::Regexp* parse_regex_string(const std::string& regexString);

        Vata2::Nfa::Nfa convert_pro_to_nfa(re2::Prog* prog);

        void create_state_cache(re2::Prog* prog);

        int get_following_state_with_back_edge(re2::Prog* prog, int state);

        bool should_delete_last_pushed(
                re2::Prog* prog,
                int currentState,
                std::vector<int>::size_type appendToStatesVectorSize);

        void make_state_final(int state, Vata2::Nfa::Nfa &nfa);

        static Vata2::Nfa::Nfa renumber_states(int progSize, Vata2::Nfa::Nfa &inputNFA);

    public:
        RegexParser() = default;
        Vata2::Nfa::Nfa create_nfa(const std::string& pattern);
    };
}

#endif // VATA2_RE2PARSER
