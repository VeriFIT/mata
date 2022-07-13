/* re2parser.cc -- parser transforming re2 regular expressions to our Nfa
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

#include <iostream>
#include <vata2/re2parser.hh>
#include <re2/regexp.h>
#include <re2/prog.h>
#include <util/logging.h>

namespace Vata2 {

    /**
     * The main method, it creates NFA from regex
     * @param pattern regex as string
     * @return vata2::Nfa::Nfa corresponding to pattern
     */
    Vata2::Nfa::Nfa RegexParser::create_nfa(const std::string& pattern) {
        auto parsedRegex = this->parse_regex_string(pattern);
        auto prog = parsedRegex->CompileToProg(options.max_mem() * 2 / 3);
        Vata2::Nfa::Nfa finalNFA = this->convert_pro_to_nfa(prog);
        delete prog;
        // Decrements reference count and deletes object if the count reaches 0
        parsedRegex->Decref();
        return finalNFA;
    }

    /**
     * Creates parsed regex (ie. Regexp*) from string regexString
     * @param regexString Regex to be parsed as a string
     * @return Parsed regex as RE2 Regexp*
     */
    re2::Regexp* RegexParser::parse_regex_string(const std::string& regexString) {
        re2::RegexpStatus status;

        auto parsedRegex = re2::Regexp::Parse(
                regexString,
                static_cast<re2::Regexp::ParseFlags>(options.ParseFlags()),
                &status);
        if (parsedRegex == nullptr) {
            if (options.log_errors()) {
                LOG(ERROR) << "Error parsing '" << regexString << "': "
                           << status.Text();
            }
            exit(EXIT_FAILURE);
        }
        return parsedRegex;
    }

    /**
     * Converts re2's prog to vata2::Nfa::Nfa
     * @param prog Prog* to create vata2::Nfa::Nfa from
     * @return vata2::Nfa::Nfa created from prog
     */
    Vata2::Nfa::Nfa RegexParser::convert_pro_to_nfa(re2::Prog* prog) {
        const int startState = prog->start();
        const int progSize = prog->size();
        int emptyFlag;
        std::vector<int> symbols;
        Vata2::Nfa::Nfa explicitNfa(progSize);

        // We will be appending to nop and capture types of states
        std::vector<long int> appendToStates;
        // It will hold information about outgoing edges -> (symbol, mappedTargetState) pairs. Indexes of the main
        // vector are source states of the edge
        std::vector<std::vector<std::pair<int, int>>> backStateOutgoingEdges(
                progSize, std::vector<std::pair<int, int>>());

        // Vectors are saved in this->state_cache after this
        this->create_state_cache(prog);

        explicitNfa.add_initial(this->state_cache.state_mapping[startState][0]);
        this->state_cache.has_state_incoming_edge[this->state_cache.state_mapping[startState][0]] = true;

        // We traverse all the states and create corresponding states and edges in vata2::Nfa::Nfa
        for (int currentState = startState; currentState < progSize; currentState++) {
            re2::Prog::Inst *inst = prog->inst(currentState);
            // Every type of state can be final (due to epsilon transition), so we check it regardless of its type
            if (this->state_cache.is_final_state[currentState]) {
                this->make_state_final(currentState, explicitNfa);
            }
            switch (inst->opcode()) {
                default:
                    LOG(DFATAL) << "unhandled " << inst->opcode() << " in convertProgTovata2::Nfa::Nfa";
                    break;

                case re2::kInstMatch:
                    // The kInstMatch type of state is a final state, but all final states are handled before the switch
                    // statement above
                    break;

                case re2::kInstNop:
                case re2::kInstCapture:
                    // When there is a back edge from the current state and the current state is last, we will not be
                    // using the current appendToStates anymore
                    if (inst->out() < currentState && inst->last()) {
                        appendToStates.clear();
                    }
                    break;
                case re2::kInstEmptyWidth:
                    emptyFlag = static_cast<int>(inst->empty());
                    symbols.clear();
                    // ^ - beginning of line
                    if (emptyFlag & re2::kEmptyBeginLine) {
                        // TODO Symbol?
                        symbols.push_back(300);
                    }
                    // $ - end of line
                    if (emptyFlag & re2::kEmptyEndLine) {
                        // TODO Symbol?
                        symbols.push_back(10);
                    }
                    // \A - beginning of text
                    if (emptyFlag & re2::kEmptyBeginText) {
                        // TODO Symbol?
                        symbols.push_back(301);
                    }
                    // \z - end of text
                    if (emptyFlag & re2::kEmptyEndText) {
                        // TODO Symbol?
                        symbols.push_back(302);
                    }
                    // \b - word boundary
                    if (emptyFlag & re2::kEmptyWordBoundary) {
                        // TODO Symbol?
                        symbols.push_back(303);
                    }
                    // \B - not \b
                    if (emptyFlag & re2::kEmptyNonWordBoundary) {
                        // TODO Symbol?
                        symbols.push_back(304);
                    }
                // kInstByteRange represents states with a "byte range" on the outgoing transition(s) (it can also be
                // a single byte)
                case re2::kInstByteRange:
                    if (symbols.empty()) {
                        // Save all symbols that can be used on the current transition
                        for (long int symbol = inst->lo(); symbol <= inst->hi(); symbol++) {
                            symbols.push_back(symbol);
                        }
                    }
                    // We always add the current state to the appendToState vector. Even if it will be the only state in
                    // the vector. Like that, we don't have to check the vector for emptiness and work with the current
                    // state and/or appendToStates vector separately
                    appendToStates.push_back(currentState);
                    for (auto stateToAppend: appendToStates) {
                        for (auto mappedState: this->state_cache.state_mapping[stateToAppend]) {
                            // Skip states that haven't any incoming edge, these states aren't reachable
                            if (!this->state_cache.has_state_incoming_edge[mappedState]) {
                                continue;
                            }
                            for (auto mappedTargetState: this->state_cache.state_mapping[inst->out()]) {
                                // There can be more symbols on the edge
                                for (auto symbol: symbols) {
                                    this->state_cache.has_state_incoming_edge[mappedTargetState] = true;
                                    explicitNfa.add_trans(mappedState, symbol, mappedTargetState);
                                    backStateOutgoingEdges[mappedState].push_back({symbol, mappedTargetState});
                                }
                            }
                        }
                    }
                    // Some states are added to the appendToStates vector only to be processed in the current iteration,
                    // we do not want to append to them in the following iterations
                    if (this->should_delete_last_pushed(
                            prog, currentState, appendToStates.size())) {
                        appendToStates.pop_back();
                    }
                    // There is an epsilon transition to the currentState+1
                    if (!this->state_cache.is_last[currentState]) {
                        re2::Prog::Inst *next = prog->inst(currentState+1);
                        // The currentState+1 (or any following state accessible with epsilon transition from it) can
                        // have a back edge (edge going to a state with a lower number than the current state). In such
                        // a case, we must also "append" transitions of the back edge target state to the current state
                        int stateWithBackEdge = this->get_following_state_with_back_edge(prog, next->out());
                        if (stateWithBackEdge != -1) {
                            re2::Prog::Inst *stateWithBackEdgeInst = prog->inst(stateWithBackEdge);
                            for (auto appendToState: this->state_cache.state_mapping[currentState]) {
                                for (auto targetState:
                                    this->state_cache.state_mapping[stateWithBackEdgeInst->out()]) {
                                    for (auto targetStateOutgoingEdges:
                                        backStateOutgoingEdges[targetState]) {
                                        explicitNfa.add_trans(appendToState, targetStateOutgoingEdges.first,
                                                              targetStateOutgoingEdges.second);
                                    }
                                }
                            }
                        }
                    } else {
                        appendToStates.clear();
                    }
                    symbols.clear();
                    break;
            }
        }

        return Vata2::RegexParser::renumber_states(progSize, explicitNfa);
    }

    /**
     * Creates all state cache vectors needed throughout the computation and saves them to the private variable state_cache
     * @param prog RE2 prog corresponding to the parsed regex
     */
    void RegexParser::create_state_cache(re2::Prog *prog) {
        std::vector<bool> defaultFalseVec(prog->size(), false);
        this->state_cache = {
            {}, // state_mapping holds states that map to each state (index) due to epsilon transitions
            defaultFalseVec, // is_final_state holds true for states that are final, false for the rest
            // is_state_nop_or_cap holds true for states that have type nop or cap, false for the rest
            defaultFalseVec,
            defaultFalseVec, // is_last holds true for states that are last, false for the rest
            // has_state_incoming_edge holds true for states with an incoming edge, false for the rest
            defaultFalseVec,
            // has_state_outgoing_back_edge holds true for states with outgoing edge to lower number state,
            // false for the rest
            defaultFalseVec,
        };
        const int startState = prog->start();
        const int progSize = prog->size();

        // Used for the first loop through states
        std::vector<int> tmpStateMapping(progSize);
        for (int state = 0; state < progSize; state++) {
            tmpStateMapping[state] = state;
            this->state_cache.state_mapping.push_back({tmpStateMapping[state]});
        }

        // When there is nop or capture type of state, we will be appending to it
        int appendToState = -1;
        int mappedTargetState;

        for (int state = startState; state < progSize; state++) {
            re2::Prog::Inst *inst = prog->inst(state);
            if (inst->last()) {
                this->state_cache.is_last[state] = true;
            }
            // kInstMatch has out() == 0, so the first part of the condition would be true, but it is not considered as
            // back edge
            if (inst->out() < state && inst->opcode() != re2::kInstMatch) {
                this->state_cache.has_state_outgoing_back_edge[state] = true;
            }
            if (inst->opcode() == re2::kInstCapture || inst->opcode() == re2::kInstNop) {
                this->state_cache.is_state_nop_or_cap[state] = true;
                mappedTargetState = tmpStateMapping[inst->out()];
                tmpStateMapping[state] = mappedTargetState;
                if (appendToState != -1) {
                    // Nop or capture type of state may or may not have an incoming edge, the target state should have
                    // it only if the current state has it
                    if (this->state_cache.has_state_incoming_edge[state]) {
                        this->state_cache.has_state_incoming_edge[mappedTargetState] = true;
                    }
                    tmpStateMapping[appendToState] = mappedTargetState;
                } else {
                    appendToState = state;
                }
            } else if (inst->opcode() == re2::kInstMatch) {
                this->state_cache.is_final_state[state] = true;
                appendToState = -1;
            } else {
                // Other types of states will always have an incoming edge so the target state will always have it too
                this->state_cache.has_state_incoming_edge[inst->out()] = true;
                appendToState = -1;
            }
        }

        // If the start state has type nop or capture it would be skipped in the NFA, so we map the out state to it to
        // make it visible
        re2::Prog::Inst *startStateInst = prog->inst(startState);
        if (this->state_cache.is_state_nop_or_cap[startState]) {
            if (this->state_cache.is_last[startState]) {
                tmpStateMapping[startState] = tmpStateMapping[startStateInst->out()];
            } else {
                tmpStateMapping[startStateInst->out()] = tmpStateMapping[startState+1];
                tmpStateMapping[startState] = tmpStateMapping[startState+1];
            }
        }

        // A state can have two states mapped to it, we create those mappings in the second loop
        std::vector<int> appendToStates = {};
        // We will keep track of potential final states. When there is an epsilon transition from a state, it can lead
        // to a final state, which makes the source state final too
        std::vector<int> statesToMakeFinal = {};
        for (int state = startState; state < progSize; state++) {
            re2::Prog::Inst *inst = prog->inst(state);
            if (inst->opcode() == re2::kInstCapture || inst->opcode() == re2::kInstNop) {
                // All states of type nop or capture have epsilon transition
                statesToMakeFinal.push_back(state);
                // If the epsilon transition leads to a final state all states within the current epsilon transition
                // chain will be final
                if (this->state_cache.is_final_state[inst->out()]) {
                    for (auto finalState: statesToMakeFinal) {
                        this->state_cache.is_final_state[finalState] = true;
                    }
                }
                mappedTargetState = tmpStateMapping[inst->out()];
                this->state_cache.state_mapping[state] = {mappedTargetState};
                if (!appendToStates.empty()) {
                    std::vector<int> targetStates;
                    if (!inst->last()) {
                        targetStates.push_back(mappedTargetState);
                        targetStates.push_back(tmpStateMapping[state+1]);
                    } else {
                        targetStates.push_back(mappedTargetState);
                    }
                    for (auto appendTo: appendToStates) {
                        this->state_cache.state_mapping[appendTo] = targetStates;
                    }
                    // These states are processed now, we can delete them
                    appendToStates.clear();
                }
                appendToStates.push_back(state);
            } else if (inst->opcode() == re2::kInstMatch) {
                for (auto finalState: statesToMakeFinal) {
                    this->state_cache.is_final_state[finalState] = true;
                }
                statesToMakeFinal.clear();
                appendToStates.clear();
            } else {
                if (inst->last()) {
                    // A state that is not of the type nop or capture and is last has no epsilon transition, so it
                    // breaks the chain of epsilon transition possibly leading to the final state
                    statesToMakeFinal.clear();
                } else {
                    // State with last() == false has an epsilon transition to state + 1. Otherwise, it's the same as
                    // above
                    statesToMakeFinal.push_back(state);
                    if (this->state_cache.is_final_state[inst->out()]) {
                        for (auto finalState: statesToMakeFinal) {
                            this->state_cache.is_final_state[finalState] = true;
                        }
                    }
                }
                appendToStates.clear();
            }
        }

        // If the start state type is capture or nop and is also not last (there is an epsilon transition to state + 1),
        // we must update the start state mapping
        if (this->state_cache.is_state_nop_or_cap[startState] && !this->state_cache.is_last[startState]) {
            this->state_cache.state_mapping[startStateInst->out()] = this->state_cache.state_mapping[startState + 1];
        }
    }

    /**
     * Checks if the state parameter (or any state which could be accessed with epsilon transitions from it)
     * has an outgoing back edge and returns the state
     * @param prog RE2 prog corresponding to the parsed regex
     * @param state State to check
     * @return A state with a back edge if there is such or -1 otherwise
     */
    int
    RegexParser::get_following_state_with_back_edge(re2::Prog *prog, int state) {
        if(this->state_cache.has_state_outgoing_back_edge[state]) {
            return state;
        }
        re2::Prog::Inst *stateInst = prog->inst(state);
        if (!this->state_cache.is_state_nop_or_cap[state]) {
            return -1;
        }
        // Check all states, which are accessible with epsilon transition from the state, for a potential outgoing back
        // edge
        while (this->state_cache.is_state_nop_or_cap[stateInst->out()]) {
            if (this->state_cache.has_state_outgoing_back_edge[stateInst->out()]) {
                return stateInst->out();
            }
            stateInst = prog->inst(stateInst->out());
        }
        return -1;
    }

    /**
     * Check if the last pushed state should be deleted from the appendToStates vector when walking RE2 NFA
     * @param prog RE2 prog corresponding to the parsed regex
     * @param currentState State to check
     * @param appendToStatesVectorSize Current size of the appendToStates vector
     * @return True if there is a chain of epsilon transitions starting from the currentState, false otherwise
     */
    bool RegexParser::should_delete_last_pushed(
            re2::Prog* prog,
            int currentState,
            std::vector<int>::size_type appendToStatesVectorSize) {
        // If the state is not last (i.e., it has an epsilon edge to the state+1), it's not the final state and is
        // currently the only one that we would append to, then we must keep it. If there already was some state, we
        // would be appending to it and the current state would be skipped
        if (!state_cache.is_last[currentState] && !this->state_cache.is_final_state[currentState] &&
        appendToStatesVectorSize == 1) {
            return false;
        }
        // There is an epsilon transition from the currentState to the currentState + 1 which is of type nop or capture
        if (!this->state_cache.is_last[currentState] && this->state_cache.is_state_nop_or_cap[currentState + 1]) {
            re2::Prog::Inst *nextStateInst = prog->inst(currentState + 1);
            if (this->state_cache.is_last[nextStateInst->out()]) {
                // There can be a "chain" of nop/capture states, we must check them all
                while (state_cache.is_state_nop_or_cap[nextStateInst->out()]) {
                    nextStateInst = prog->inst(nextStateInst->out());
                    if (!this->state_cache.is_last[nextStateInst->out()]) {
                        return false;
                    }
                }
                // If there is no epsilon transition other than the currentState to currentState+1, we should delete
                // the last pushed state from the appendToStates vector
                return true;
            }
        }
        return false;
    }

    /**
     * Makes all states mapped to the state parameter final in the vata2::Nfa::Nfa
     * @param state State which should be made final
     * @param nfa vata2::Nfa::Nfa in which the states will be made final
     */
    void RegexParser::make_state_final(int state, Vata2::Nfa::Nfa &nfa) {
        for (auto targetState: this->state_cache.state_mapping[state]) {
            // States without an incoming edge should not be in the automata
            if (!this->state_cache.has_state_incoming_edge[targetState]) {
                continue;
            }
            nfa.add_final(targetState);
        }
    }

    /**
     * Renumbers the states of the inputNFA to be from <0, numberOfStates>
     * @param progSize Size of the RE2 prog
     * @param inputNFA vata2::Nfa::Nfa which states should be renumbered
     * @return Same vata2::Nfa::Nfa as inputNFA but with states from interval <0, numberOfStates>
     */
    Vata2::Nfa::Nfa RegexParser::renumber_states(int progSize, Vata2::Nfa::Nfa &inputNFA) {
        std::vector<unsigned long> renumberedStates(progSize, -1);
        Vata2::Nfa::Nfa renumberedExplicitNfa(0);
        for (int state = 0; state < progSize; state++) {
            const auto& transitionList = inputNFA.get_transitions_from_state(state);
            // If the transition list is empty, the state is not used
            if (transitionList.empty()) {
                continue;
            } else {
                // addNewState returns next unused state of the new NFA, so we map it to the original state
                renumberedStates[state] = renumberedExplicitNfa.add_new_state();
            }
        }

        for (auto state: inputNFA.finalstates) {
            if (renumberedStates[state] == -1) {
                renumberedStates[state] = renumberedExplicitNfa.add_new_state();
            }
            renumberedExplicitNfa.add_final(renumberedStates[state]);
        }

        for (int state = 0; state < progSize; state++) {
            const auto& transitionList = inputNFA.get_transitions_from_state(state);
            for (const auto& transition: transitionList) {
                for (auto stateTo: transition.states_to) {
                    renumberedExplicitNfa.add_trans(renumberedStates[state], transition.symbol,
                                                    renumberedStates[stateTo]);
                }
            }
        }

        for (auto state: inputNFA.initialstates) {
            renumberedExplicitNfa.add_initial(renumberedStates[state]);
        }

        return renumberedExplicitNfa;
    }

} // namespace automata_lib