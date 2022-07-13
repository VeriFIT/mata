//
// Created by HOM on 21.01.2022.
//

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
