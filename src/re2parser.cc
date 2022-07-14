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

// VATA2 headers
#include <vata2/re2parser.hh>

// RE2 headers
#include <re2/re2/regexp.h>
#include <re2/re2/prog.h>
#include <re2/util/logging.h>

namespace {
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
            std::vector<std::vector<Vata2::Nfa::State>> state_mapping;
            std::vector<bool> is_final_state;
            std::vector<bool> is_state_nop_or_cap;
            std::vector<bool> is_last;
            std::vector<bool> has_state_incoming_edge;
            std::vector<bool> has_state_outgoing_back_edge;
        };

    public:
        /**
         * Default RE2 options
         */
        RE2::Options options;
        StateCache state_cache;

        RegexParser() = default;

        /**
         * Creates parsed regex (ie. Regexp*) from string regex_string
         * @param regex_string Regex to be parsed as a string
         * @return Parsed regex as RE2 Regexp*
         */
        re2::Regexp* parse_regex_string(const std::string& regex_string) const {
            re2::RegexpStatus status;

            auto parsed_regex = re2::Regexp::Parse(
                    regex_string,
                    static_cast<re2::Regexp::ParseFlags>(options.ParseFlags()),
                    &status);
            if (parsed_regex == nullptr) {
                if (options.log_errors()) {
                    LOG(ERROR) << "Error parsing '" << regex_string << "': "
                               << status.Text();
                }
                exit(EXIT_FAILURE);
            }
            return parsed_regex;
        }

        /**
         * Converts re2's prog to vata2::Nfa::Nfa
         * @param prog Prog* to create vata2::Nfa::Nfa from
         * @return vata2::Nfa::Nfa created from prog
         */
        void convert_pro_to_nfa(Vata2::Nfa::Nfa* output_nfa, re2::Prog* prog) {
            const int start_state = prog->start();
            const int prog_size = prog->size();
            int empty_flag;
            std::vector<Vata2::Nfa::Symbol> symbols;
            Vata2::Nfa::Nfa explicit_nfa(prog_size);

            // We will be appending to nop and capture types of states
            std::vector<Vata2::Nfa::State> append_to_states;
            // It will hold information about outgoing edges -> (symbol, mappedTargetState) pairs. Indexes of the main
            // vector are source states of the edge
            std::vector<std::vector<std::pair<Vata2::Nfa::Symbol, Vata2::Nfa::State>>> back_state_outgoing_edges(
                    prog_size, std::vector<std::pair<Vata2::Nfa::Symbol, Vata2::Nfa::State>>());

            // Vectors are saved in this->state_cache after this
            this->create_state_cache(prog);

            explicit_nfa.add_initial(this->state_cache.state_mapping[start_state][0]);
            this->state_cache.has_state_incoming_edge[this->state_cache.state_mapping[start_state][0]] = true;

            // We traverse all the states and create corresponding states and edges in vata2::Nfa::Nfa
            for (int current_state = start_state; current_state < prog_size; current_state++) {
                re2::Prog::Inst *inst = prog->inst(current_state);
                // Every type of state can be final (due to epsilon transition), so we check it regardless of its type
                if (this->state_cache.is_final_state[current_state]) {
                    this->make_state_final(current_state, explicit_nfa);
                }
                switch (inst->opcode()) {
                    default:
                        LOG(DFATAL) << "unhandled " << inst->opcode() << " in convertProgTovata2::Nfa::Nfa";
                        break;

                    case re2::kInstMatch:
                        // The kInstMatch type of state is a final state,
                        // but all final states are handled before the switch statement above
                        break;

                    case re2::kInstNop:
                    case re2::kInstCapture:
                        // When there is a back edge from the current state and the current state is last,
                        // we will not be using the current append_to_states anymore
                        if (inst->out() < current_state && inst->last()) {
                            append_to_states.clear();
                        }
                        break;
                    case re2::kInstEmptyWidth:
                        empty_flag = static_cast<int>(inst->empty());
                        symbols.clear();
                        // ^ - beginning of line
                        if (empty_flag & re2::kEmptyBeginLine) {
                            // TODO Symbol?
                            symbols.push_back(300);
                        }
                        // $ - end of line
                        if (empty_flag & re2::kEmptyEndLine) {
                            // TODO Symbol?
                            symbols.push_back(10);
                        }
                        // \A - beginning of text
                        if (empty_flag & re2::kEmptyBeginText) {
                            // TODO Symbol?
                            symbols.push_back(301);
                        }
                        // \z - end of text
                        if (empty_flag & re2::kEmptyEndText) {
                            // TODO Symbol?
                            symbols.push_back(302);
                        }
                        // \b - word boundary
                        if (empty_flag & re2::kEmptyWordBoundary) {
                            // TODO Symbol?
                            symbols.push_back(303);
                        }
                        // \B - not \b
                        if (empty_flag & re2::kEmptyNonWordBoundary) {
                            // TODO Symbol?
                            symbols.push_back(304);
                        }
                    // kInstByteRange represents states with a "byte range" on the outgoing transition(s)
                    // (it can also be a single byte)
                    case re2::kInstByteRange:
                        if (symbols.empty()) {
                            // Save all symbols that can be used on the current transition
                            for (long int symbol = inst->lo(); symbol <= inst->hi(); symbol++) {
                                symbols.push_back(symbol);
                            }
                        }
                        // We always add the current state to the appendToState vector.
                        // Even if it will be the only state in the vector.
                        // Like that, we don't have to check the vector for emptiness and work with the current
                        // state and/or append_to_states vector separately
                        append_to_states.push_back(current_state);
                        for (auto stateToAppend: append_to_states) {
                            for (auto mappedState: this->state_cache.state_mapping[stateToAppend]) {
                                // Skip states that haven't any incoming edge, these states aren't reachable
                                if (!this->state_cache.has_state_incoming_edge[mappedState]) {
                                    continue;
                                }
                                for (auto mappedTargetState: this->state_cache.state_mapping[inst->out()]) {
                                    // There can be more symbols on the edge
                                    for (auto symbol: symbols) {
                                        this->state_cache.has_state_incoming_edge[mappedTargetState] = true;
                                        explicit_nfa.add_trans(mappedState, symbol, mappedTargetState);
                                        back_state_outgoing_edges[mappedState].push_back(
                                                {symbol, mappedTargetState});
                                    }
                                }
                            }
                        }
                        // Some states are added to the append_to_states vector only to be processed in
                        // the current iteration, we do not want to append to them in the following iterations
                        if (this->should_delete_last_pushed(
                                prog, current_state, append_to_states.size())) {
                            append_to_states.pop_back();
                        }
                        // There is an epsilon transition to the current_state+1
                        if (!this->state_cache.is_last[current_state]) {
                            re2::Prog::Inst *next = prog->inst(current_state + 1);
                            // The current_state+1 (or any following state accessible with epsilon transition from it)
                            // can have a back edge (edge going to a state with a lower number than the current state).
                            // In such a case, we must also "append" transitions of the back edge
                            // target state to the current state
                            int stateWithBackEdge = this->get_following_state_with_back_edge(prog, next->out());
                            if (stateWithBackEdge != -1) {
                                re2::Prog::Inst *stateWithBackEdgeInst = prog->inst(stateWithBackEdge);
                                for (auto appendToState: this->state_cache.state_mapping[current_state]) {
                                    for (auto targetState:
                                        this->state_cache.state_mapping[stateWithBackEdgeInst->out()]) {
                                        for (auto targetStateOutgoingEdges:
                                            back_state_outgoing_edges[targetState]) {
                                            explicit_nfa.add_trans(appendToState,
                                                                   targetStateOutgoingEdges.first,
                                                                   targetStateOutgoingEdges.second);
                                        }
                                    }
                                }
                            }
                        } else {
                            append_to_states.clear();
                        }
                        symbols.clear();
                        break;
                }
            }

            RegexParser::renumber_states(output_nfa, prog_size, explicit_nfa);
        }

    private: // private methods
        /**
         * Creates all state cache vectors needed throughout the computation and saves them
         * to the private variable state_cache
         * @param prog RE2 prog corresponding to the parsed regex
         */
        void create_state_cache(re2::Prog *prog) {
            std::vector<bool> default_false_vec(prog->size(), false);
            this->state_cache = {
                // state_mapping holds states that map to each state (index) due to epsilon transitions
                {},
                // is_final_state holds true for states that are final, false for the rest
                default_false_vec,
                // is_state_nop_or_cap holds true for states that have type nop or cap, false for the rest
                default_false_vec,
                // is_last holds true for states that are last, false for the rest
                default_false_vec,
                // has_state_incoming_edge holds true for states with an incoming edge, false for the rest
                default_false_vec,
                // has_state_outgoing_back_edge holds true for states with outgoing edge to lower number state,
                // false for the rest
                default_false_vec,
            };
            const int start_state = prog->start();
            const int prog_size = prog->size();

            // Used for the first loop through states
            std::vector<Vata2::Nfa::State> tmp_state_mapping(prog_size);
            for (int state = 0; state < prog_size; state++) {
                tmp_state_mapping[state] = state;
                this->state_cache.state_mapping.push_back({tmp_state_mapping[state]});
            }

            // When there is nop or capture type of state, we will be appending to it
            int append_to_state = -1;
            Vata2::Nfa::State mapped_parget_state;

            for (int state = start_state; state < prog_size; state++) {
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
                    mapped_parget_state = tmp_state_mapping[static_cast<Vata2::Nfa::State>(inst->out())];
                    tmp_state_mapping[state] = mapped_parget_state;
                    if (append_to_state != -1) {
                        // Nop or capture type of state may or may not have an incoming edge, the target state should have
                        // it only if the current state has it
                        if (this->state_cache.has_state_incoming_edge[state]) {
                            this->state_cache.has_state_incoming_edge[mapped_parget_state] = true;
                        }
                        tmp_state_mapping[append_to_state] = mapped_parget_state;
                    } else {
                        append_to_state = state;
                    }
                } else if (inst->opcode() == re2::kInstMatch) {
                    this->state_cache.is_final_state[state] = true;
                    append_to_state = -1;
                } else {
                    // Other types of states will always have an incoming edge so the target state will always have it too
                    this->state_cache.has_state_incoming_edge[inst->out()] = true;
                    append_to_state = -1;
                }
            }

            // If the start state has type nop or capture it would be skipped in the NFA, so we map the out state to it to
            // make it visible
            re2::Prog::Inst *start_state_inst = prog->inst(start_state);
            if (this->state_cache.is_state_nop_or_cap[start_state]) {
                if (this->state_cache.is_last[start_state]) {
                    tmp_state_mapping[start_state] = tmp_state_mapping[start_state_inst->out()];
                } else {
                    tmp_state_mapping[start_state_inst->out()] = tmp_state_mapping[start_state + 1];
                    tmp_state_mapping[start_state] = tmp_state_mapping[start_state + 1];
                }
            }

            // A state can have two states mapped to it, we create those mappings in the second loop
            std::vector<Vata2::Nfa::State> append_to_states = {};
            // We will keep track of potential final states. When there is an epsilon transition from a state, it can lead
            // to a final state, which makes the source state final too
            std::vector<Vata2::Nfa::State> states_to_make_final = {};
            for (int state = start_state; state < prog_size; state++) {
                re2::Prog::Inst *inst = prog->inst(state);
                if (inst->opcode() == re2::kInstCapture || inst->opcode() == re2::kInstNop) {
                    // All states of type nop or capture have epsilon transition
                    states_to_make_final.push_back(state);
                    // If the epsilon transition leads to a final state all states within the current epsilon transition
                    // chain will be final
                    if (this->state_cache.is_final_state[inst->out()]) {
                        for (auto finalState: states_to_make_final) {
                            this->state_cache.is_final_state[finalState] = true;
                        }
                    }
                    mapped_parget_state = tmp_state_mapping[inst->out()];
                    this->state_cache.state_mapping[state] = {mapped_parget_state};
                    if (!append_to_states.empty()) {
                        std::vector<Vata2::Nfa::State> target_states;
                        if (!inst->last()) {
                            target_states.push_back(mapped_parget_state);
                            target_states.push_back(tmp_state_mapping[state + 1]);
                        } else {
                            target_states.push_back(mapped_parget_state);
                        }
                        for (auto appendTo: append_to_states) {
                            this->state_cache.state_mapping[appendTo] = target_states;
                        }
                        // These states are processed now, we can delete them
                        append_to_states.clear();
                    }
                    append_to_states.push_back(state);
                } else if (inst->opcode() == re2::kInstMatch) {
                    for (auto finalState: states_to_make_final) {
                        this->state_cache.is_final_state[finalState] = true;
                    }
                    states_to_make_final.clear();
                    append_to_states.clear();
                } else {
                    if (inst->last()) {
                        // A state that is not of the type nop or capture and is last has no epsilon transition,
                        // so it breaks the chain of epsilon transition possibly leading to the final state
                        states_to_make_final.clear();
                    } else {
                        // State with last() == false has an epsilon transition to state + 1. Otherwise,
                        // it's the same as above
                        states_to_make_final.push_back(state);
                        if (this->state_cache.is_final_state[inst->out()]) {
                            for (auto finalState: states_to_make_final) {
                                this->state_cache.is_final_state[finalState] = true;
                            }
                        }
                    }
                    append_to_states.clear();
                }
            }

            // If the start state type is capture or nop and is also not last
            // (there is an epsilon transition to state + 1), we must update the start state mapping
            if (this->state_cache.is_state_nop_or_cap[start_state] && !this->state_cache.is_last[start_state]) {
                this->state_cache.state_mapping[start_state_inst->out()] =
                        this->state_cache.state_mapping[start_state + 1];
            }
        }

        /**
         * Checks if the state parameter (or any state which could be accessed with epsilon transitions from it)
         * has an outgoing back edge and returns the state
         * @param prog RE2 prog corresponding to the parsed regex
         * @param state State to check
         * @return A state with a back edge if there is such or -1 otherwise
         */
        int get_following_state_with_back_edge(re2::Prog *prog, int state) {
            if(this->state_cache.has_state_outgoing_back_edge[state]) {
                return state;
            }
            re2::Prog::Inst *state_inst = prog->inst(state);
            if (!this->state_cache.is_state_nop_or_cap[state]) {
                return -1;
            }
            // Check all states, which are accessible with epsilon transition from the state,
            // for a potential outgoing back edge
            while (this->state_cache.is_state_nop_or_cap[state_inst->out()]) {
                if (this->state_cache.has_state_outgoing_back_edge[state_inst->out()]) {
                    return state_inst->out();
                }
                state_inst = prog->inst(state_inst->out());
            }
            return -1;
        }

        /**
         * Check if the last pushed state should be deleted from the appendToStates vector when walking RE2 NFA
         * @param prog RE2 prog corresponding to the parsed regex
         * @param current_state State to check
         * @param append_to_states_vector_size Current size of the appendToStates vector
         * @return True if there is a chain of epsilon transitions starting from the current_state, false otherwise
         */
        bool should_delete_last_pushed(
                re2::Prog* prog,
                int current_state,
                std::vector<int>::size_type append_to_states_vector_size) {
            // If the state is not last (i.e., it has an epsilon edge to the state+1), it's not the final state and is
            // currently the only one that we would append to, then we must keep it. If there already was some state, we
            // would be appending to it and the current state would be skipped
            if (!state_cache.is_last[current_state] && !this->state_cache.is_final_state[current_state] &&
                append_to_states_vector_size == 1) {
                return false;
            }
            // There is an epsilon transition from the current_state to the current_state + 1
            // which is of type nop or capture
            if (!this->state_cache.is_last[current_state] && this->state_cache.is_state_nop_or_cap[current_state + 1]) {
                re2::Prog::Inst *next_state_inst = prog->inst(current_state + 1);
                if (this->state_cache.is_last[next_state_inst->out()]) {
                    // There can be a "chain" of nop/capture states, we must check them all
                    while (state_cache.is_state_nop_or_cap[next_state_inst->out()]) {
                        next_state_inst = prog->inst(next_state_inst->out());
                        if (!this->state_cache.is_last[next_state_inst->out()]) {
                            return false;
                        }
                    }
                    // If there is no epsilon transition other than the current_state to current_state+1,
                    // we should delete the last pushed state from the appendToStates vector
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
        void make_state_final(int state, Vata2::Nfa::Nfa &nfa) {
            for (auto target_state: this->state_cache.state_mapping[state]) {
                // States without an incoming edge should not be in the automata
                if (!this->state_cache.has_state_incoming_edge[target_state]) {
                    continue;
                }
                nfa.add_final(target_state);
            }
        }

        /**
         * Renumbers the states of the input_nfa to be from <0, numberOfStates>
         * @param program_size Size of the RE2 prog
         * @param input_nfa vata2::Nfa::Nfa which states should be renumbered
         * @return Same vata2::Nfa::Nfa as input_nfa but with states from interval <0, numberOfStates>
         */
        static Vata2::Nfa::Nfa renumber_states(Vata2::Nfa::Nfa* output_nfa,
                                               int program_size,
                                               Vata2::Nfa::Nfa &input_nfa) {
            std::vector<unsigned long> renumbered_states(program_size, -1);
            Vata2::Nfa::Nfa& renumbered_explicit_nfa = *output_nfa;
            for (int state = 0; state < program_size; state++) {
                const auto& transition_list = input_nfa.get_transitions_from_state(state);
                // If the transition list is empty, the state is not used
                if (transition_list.empty()) {
                    continue;
                } else {
                    // addNewState returns next unused state of the new NFA, so we map it to the original state
                    renumbered_states[state] = renumbered_explicit_nfa.add_new_state();
                }
            }

            for (auto state: input_nfa.finalstates) {
                if (static_cast<int>(renumbered_states[state]) == -1) {
                    renumbered_states[state] = renumbered_explicit_nfa.add_new_state();
                }
                renumbered_explicit_nfa.add_final(renumbered_states[state]);
            }

            for (int state = 0; state < program_size; state++) {
                const auto& transition_list = input_nfa.get_transitions_from_state(state);
                for (const auto& transition: transition_list) {
                    for (auto stateTo: transition.states_to) {
                        renumbered_explicit_nfa.add_trans(renumbered_states[state], transition.symbol,
                                                          renumbered_states[stateTo]);
                    }
                }
            }

            for (auto state: input_nfa.initialstates) {
                renumbered_explicit_nfa.add_initial(renumbered_states[state]);
            }

            return renumbered_explicit_nfa;
        }
        };
}

 /**
 * The main method, it creates NFA from regex
 * @param pattern regex as string
 * @return vata2::Nfa::Nfa corresponding to pattern
 */
void Vata2::RE2Parser::create_nfa(Nfa::Nfa* nfa, const std::string& pattern) {
    if (nfa == NULL) {
        throw std::runtime_error("create_nfa: nfa should not be NULL");
    }

    RegexParser regexParser{};
    auto parsed_regex = regexParser.parse_regex_string(pattern);
    auto program = parsed_regex->CompileToProg(regexParser.options.max_mem() * 2 / 3);
    regexParser.convert_pro_to_nfa(nfa, program);
    delete program;
    // Decrements reference count and deletes object if the count reaches 0
    parsed_regex->Decref();
}