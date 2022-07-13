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

#ifndef AUTOMATA_LIBRARY_PARSER_HPP
#define AUTOMATA_LIBRARY_PARSER_HPP

#include <string>
#include <vata2/nfa.hh>
#include <re2/re2.h>

namespace Vata2 {
    class RegexParser {
    private:
        /**
         * Holds all state cache vectors needed throughout the computation. Vector index is the state number
         * stateMapping for each state (vector index), it holds a vector of states that map to it (cause by epsilon transitions)
         * isFinalState determines if the state is final (true) or not (false)
         * isStateNopOrCap determines if the state is of type nop/cap (true) or not (false)
         * isLast determines if the state is last (true), meaning it has epsilon transition to the next state, or not (false)
         * hasStateIncomingEdge determines if there is an incoming edge to the state (true) or not (false)
         * hasStateOutgoingBackEdge determines if there is any outgoing edge leading to a state with a lower number (true) or not (false)
         */
        struct StateCache {
            std::vector<std::vector<int>> stateMapping;
            std::vector<bool> isFinalState;
            std::vector<bool> isStateNopOrCap;
            std::vector<bool> isLast;
            std::vector<bool> hasStateIncomingEdge;
            std::vector<bool> hasStateOutgoingBackEdge;
        };

        /**
         * Default RE2 options
         */
        RE2::Options options;
        StateCache stateCache;

        re2::Regexp* parseRegexString(const std::string& regexString);

        Vata2::Nfa::Nfa convertProgToNfa(re2::Prog* prog);

        void createStateCache(re2::Prog* prog);

        int getFollowingStateWithBackEdge(re2::Prog* prog, int state);

        bool shouldDeleteLastPushed(re2::Prog* prog, int currentState, std::vector<int>::size_type appendToStatesVectorSize);

        void makeStateFinal(int state, Vata2::Nfa::Nfa &nfa);

        static Vata2::Nfa::Nfa renumberStates(int progSize, Vata2::Nfa::Nfa &inputNFA);

    public:
        RegexParser() = default;
        Vata2::Nfa::Nfa createNFA(const std::string& pattern);
    };
}

#endif //AUTOMATA_LIBRARY_PARSER_HPP
