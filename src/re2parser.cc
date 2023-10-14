/* re2parser.cc -- parser transforming re2 regular expressions to our Nfa
 *
 * Copyright (c) 2022 Michal Horky
 */

#include <iostream>

#include "mata/alphabet.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/re2parser.hh"
#include "re2/regexp.h"
#include "re2/prog.h"

namespace {
    using namespace mata::nfa;

    class RegexParser {
    private:
        /**
         * Holds all state cache vectors needed throughout the computation. Vector index is the state number
         * state_mapping for each state (vector index), it holds a vector of states that map to it (cause by epsilon transitions)
         * is_final_state determines if the state is final (true) or not (false)
         * is_state_nop_or_cap determines if the state is of type nop/cap (true) or not (false)
         * is_last determines if the state is last (true), meaning it has epsilon transition to the next state, or not (false)
         * has_state_incoming_edge determines if there is an incoming edge to the state (true) or not (false)
         */
        struct StateCache {
            std::vector<std::vector<mata::nfa::State>> state_mapping;
            std::vector<bool> is_final_state;
            std::vector<bool> is_state_nop_or_cap;
            std::vector<bool> is_last;
            std::vector<bool> has_state_incoming_edge;
        };

    public:
        /**
         * Default RE2 options
         */
        RE2::Options options{};
        StateCache state_cache{};

        std::vector<std::vector<std::pair<mata::Symbol, mata::nfa::State>>> outgoingEdges{};

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
         * Converts re2's prog to Nfa
         * @param prog Prog* to create Nfa from
         * @param use_epsilon whether to create NFA with epsilon transitions or not
         * @param epsilon_value value, that will represent epsilon on transitions
         * @return Nfa created from prog
         */
        void convert_pro_to_nfa(Nfa* output_nfa, re2::Prog* prog, const bool use_epsilon, const mata::Symbol epsilon_value) {
            const auto start_state = static_cast<size_t>(prog->start());
            const auto prog_size = static_cast<size_t>(prog->size());
            // The same symbol in lowercase and uppercase is 32 symbols from each other in ASCII
            const int ascii_shift_value = 32;
            int empty_flag;
            std::vector<mata::Symbol> symbols;
            Nfa explicit_nfa(prog_size);

            // Vectors are saved in this->state_cache after this
            this->create_state_cache(prog, use_epsilon);
            // If there are more potential start states, the one without a self-loop should be chosen as a new start state
            size_t initial_state_index = 0;
            mata::nfa::State out_state;
            bool self_loop;
            if (this->state_cache.state_mapping[start_state].size() > 1) {
              // There are more potential start states, e.g. there are epsilon transitions from the original start state to
              // more than one state. The new start state should be without a self loop if there is such a state. The new
              // start state can't be a state that was originally final, because it does not have any outgoing edges.
              re2::Prog::Inst *inst;
              for (mata::nfa::State potential_start_state: this->state_cache.state_mapping[start_state]) {
                self_loop = false;
                inst = prog->inst(static_cast<int>(potential_start_state));
                if (inst->opcode() == re2::kInstMatch) {
                  initial_state_index++;
                  continue;
                }
                out_state = static_cast<mata::nfa::State>(inst->out());
                for (auto mapped_state: this->state_cache.state_mapping[out_state]) {
                  if (potential_start_state == mapped_state) {
                    self_loop = true;
                    initial_state_index++;
                    break;
                  }
                }
                if (!self_loop) {
                  break;
                }
              }
              if (initial_state_index >= this->state_cache.state_mapping[start_state].size()) {
                initial_state_index = 0;
              }
            }

            explicit_nfa.initial.insert(this->state_cache.state_mapping[start_state][initial_state_index]);
            this->state_cache.has_state_incoming_edge[this->state_cache.state_mapping[start_state][initial_state_index]] = true;

            // Used for epsilon closure, it contains tuples (state_reachable_by_epsilon_transitions, source_state_of_epsilon_transitions)
            std::vector<std::pair<mata::nfa::State, mata::nfa::State >> copyEdgesFromTo;

            // If the start state is nop or cap, and has a transition to more different states. We are creating a new
            // start state as one of the states reachable by epsilon from the start state. We must also include
            // transitions of the other epsilon reachable states to the new start state.
            if (this->state_cache.is_state_nop_or_cap[start_state] && this->state_cache.state_mapping[start_state].size() > 1) {
                for (size_t index = 0; index < this->state_cache.state_mapping[start_state].size(); index++) {
                    for (auto state: this->state_cache.state_mapping[this->state_cache.state_mapping[start_state][index]]) {
                        copyEdgesFromTo.emplace_back(state, this->state_cache.state_mapping[start_state][initial_state_index]);
                    }
                }
            }

            this->outgoingEdges = std::vector<std::vector<std::pair<mata::Symbol, mata::nfa::State>>> (prog_size);

            // We traverse all the states and create corresponding states and edges in Nfa
            for (mata::nfa::State current_state = start_state; current_state < prog_size; current_state++) {
                re2::Prog::Inst *inst = prog->inst(static_cast<int>(current_state));
                // Every type of state can be final (due to epsilon transition), so we check it regardless of its type
                if (this->state_cache.is_final_state[current_state]) {
                    this->make_state_final(current_state, explicit_nfa);
                }
                switch (inst->opcode()) {
                    default:
                        LOG(DFATAL) << "unhandled " << inst->opcode() << " in convertProgToNfa";
                        break;

                    case re2::kInstMatch:
                        // The kInstMatch type of state is a final state,
                        // but all final states are handled before the switch statement above
                        break;

                    case re2::kInstNop:
                    case re2::kInstCapture:
                        if (use_epsilon) {
                            symbols.push_back(epsilon_value);
                            this->create_explicit_nfa_transitions(current_state, inst, symbols, explicit_nfa, use_epsilon, epsilon_value);
                            symbols.clear();
                        }
                        break;
                    case re2::kInstEmptyWidth:
                        empty_flag = static_cast<int>(inst->empty());
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
                        break;
                    // kInstByteRange represents states with a "byte range" on the outgoing transition(s)
                    // (it can also be a single byte)
                    case re2::kInstByteRange:
                        if (symbols.empty()) {
                            // Save all symbols that can be used on the current transition
                            for (auto symbol = static_cast<mata::Symbol>(inst->lo()); symbol <= static_cast<mata::Symbol>(inst->hi()); symbol++) {
                                symbols.push_back(symbol);
                                // Foldcase causes RE2 to do a case-insensitive match, so transitions will be made for
                                // both uppercase and lowercase symbols
                                if (inst->foldcase()) {
                                    symbols.push_back(symbol-ascii_shift_value);
                                }
                            }
                        }
                        this->create_explicit_nfa_transitions(current_state, inst, symbols, explicit_nfa, use_epsilon, epsilon_value);

                        if (!use_epsilon) {
                            // There is an epsilon transition to the currentState+1 we will need to copy transitions of
                            // the currentState+1 to the currentState.
                            if (!this->state_cache.is_last[current_state]) {
                                for (auto state: this->state_cache.state_mapping[current_state + 1]) {
                                    copyEdgesFromTo.emplace_back(state, current_state);
                                }
                            }
                        }
                        symbols.clear();
                        break;
                }
            }
            if (!use_epsilon) {
                // We will traverse the vector in reversed order. Like that, we will also handle chains of epsilon transitions
                // 2 -(eps)-> 3 -(eps)-> 4 -(a)-> 5.... We first need to copy transitions of state 4 to state 3, and then
                // we can copy transition of state 3 (which now have copied transitions of state 4) to state 2
                for (auto copyEdgeFromTo = copyEdgesFromTo.rbegin(); copyEdgeFromTo != copyEdgesFromTo.rend(); copyEdgeFromTo++) {
                    re2::Prog::Inst *inst = prog->inst(static_cast<int>(copyEdgeFromTo->first));
                    // kInstMatch states in RE2 does not have outgoing edges. The other state will also be final
                    if (inst->opcode() == re2::kInstMatch) {
                        this->make_state_final(copyEdgeFromTo->second, explicit_nfa);
                        this->state_cache.is_final_state[copyEdgeFromTo->second] = true;
                        continue;
                    }
                    // The state is final if there are epsilon transition(s) leading to a final state
                    if (this->state_cache.is_final_state[copyEdgeFromTo->first]) {
                        this->make_state_final(copyEdgeFromTo->second, explicit_nfa);
                        this->state_cache.is_final_state[copyEdgeFromTo->second] = true;
                    }
                    for (auto transition: this->outgoingEdges[copyEdgeFromTo->first]) {
                        // We copy transitions only to states that has incoming edge
                        if (this->state_cache.has_state_incoming_edge[copyEdgeFromTo->second]) {
                            explicit_nfa.delta.add(copyEdgeFromTo->second, transition.first, transition.second);
                        }
                        // However, we still need to save the transitions (we could possibly copy them to another state in
                        // the epsilon closure that has incoming edge)
                       if (copyEdgeFromTo->second != copyEdgeFromTo->first) {
                         this->outgoingEdges[copyEdgeFromTo->second].emplace_back(transition.first, transition.second);
                       }
                    }
                }
            }
            RegexParser::renumber_states(output_nfa, prog_size, explicit_nfa);
        }

    private: // private methods
        /**
         * Creates transitions in the passed ExplicitNFA nfa. Transitions are created for each from statesFrom vector with
         * an incoming edge. Transitions are created for each symbol from symbol vector.
         * @param statesFrom states that will be used as source states
         * @param inst RE2 instruction for the current state, it is used to determine the target state for each transition
         * @param symbols symbols that will be used on each transition
         * @param nfa ExplicitNFA in which the transitions should be created
         * @param use_epsilon whether to create NFA with epsilon transitions or not
         * @param epsilon_value value, that will represent epsilon on transitions
         */
        void create_explicit_nfa_transitions(mata::nfa::State currentState, re2::Prog::Inst *inst,
                                             const std::vector<mata::Symbol>& symbols,
                                             Nfa &nfa, bool use_epsilon, mata::Symbol epsilon_value) {
            for (auto mappedState: this->state_cache.state_mapping[currentState]) {
                for (auto mappedTargetState: this->state_cache.state_mapping[static_cast<unsigned long>(inst->out())]) {
                    // There can be more symbols on the edge
                    for (auto symbol: symbols) {
                        if (!use_epsilon) {
                            // Save all outgoing edges. The vector will be used to get rid of epsilon transitions
                            this->outgoingEdges[mappedState].emplace_back(symbol, mappedTargetState);
                        }
                        if (this->state_cache.has_state_incoming_edge[mappedState]) {
                            this->state_cache.has_state_incoming_edge[mappedTargetState] = true;
                            nfa.delta.add(mappedState, symbol, mappedTargetState);
                        }
                    }
                }
            }
            if (use_epsilon) {
                // There is an epsilon transition to the currentState+1, so we must handle it
                if (!this->state_cache.is_last[currentState]) {
                    nfa.delta.add(currentState, epsilon_value, currentState + 1);
                }
            }
        }

       /**
        * Creates all state cache vectors needed throughout the computation and saves them to the private variable state_cache.
        * It calls appropriate method based on use_epsilon param
        * @param prog RE2 prog corresponding to the parsed regex
        * @param use_epsilon whether to create NFA with epsilon transitions or not
        */
       void create_state_cache(re2::Prog *prog, bool use_epsilon) {
            if (use_epsilon) {
                this->create_state_cache_with_epsilon(prog);
            } else {
                this->create_state_cache_without_epsilon(prog);
            }
        }

        /**
         * Creates all state cache vectors needed throughout the computation and saves them
         * to the private variable state_cache
         * It creates state cache for creating ExplicitNFA without epsilon transitions
         * @param prog RE2 prog corresponding to the parsed regex
         */
        void create_state_cache_without_epsilon(re2::Prog *prog) {
            std::vector<bool> default_false_vec(static_cast<size_t>(prog->size()), false);
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
            };
            const auto start_state = static_cast<size_t>(prog->start());
            const auto prog_size = static_cast<size_t>(prog->size());

            // Used for the first loop through states
            std::vector<mata::nfa::State> tmp_state_mapping(prog_size);
            for (mata::nfa::State state = 0; state < prog_size; state++) {
                tmp_state_mapping[state] = state;
                this->state_cache.state_mapping.push_back({state});
            }

            // When there is nop or capture type of state, we will be appending to it
            mata::nfa::State append_to_state = mata::nfa::Limits::max_state;
            mata::nfa::State mapped_parget_state;
            std::vector<mata::nfa::State> states_for_second_check(prog_size);

            for (mata::nfa::State state = start_state; state < prog_size; state++) {
                re2::Prog::Inst *inst = prog->inst(static_cast<int>(state));
                if (inst->last()) {
                    this->state_cache.is_last[state] = true;
                }

                if (inst->opcode() == re2::kInstCapture || inst->opcode() == re2::kInstNop) {
                    this->state_cache.state_mapping[state] = this->get_mapped_states(prog, state, inst);
                    this->state_cache.is_state_nop_or_cap[state] = true;
                    mapped_parget_state = tmp_state_mapping[static_cast<mata::nfa::State>(inst->out())];
                    tmp_state_mapping[state] = mapped_parget_state;
                    if (append_to_state != mata::nfa::Limits::max_state) {
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
                    if (append_to_state != mata::nfa::Limits::max_state
                        && this->state_cache.has_state_incoming_edge[append_to_state]) {
                      this->state_cache.has_state_incoming_edge[state] = true;
                    }
                    append_to_state = mata::nfa::Limits::max_state;
                } else {
                    // Other types of states will always have an incoming edge so the target state will always have it too
                    this->state_cache.has_state_incoming_edge[static_cast<size_t>(inst->out())] = true;
                    if (static_cast<size_t>(inst->out()) < state) {
                      for (auto mapped_state: this->state_cache.state_mapping[static_cast<size_t>(inst->out())]) {
                        if (mapped_state == state) {
                          this->state_cache.has_state_incoming_edge[state] = true;
                        } else if (prog->inst(static_cast<int>(mapped_state))->opcode() == re2::kInstMatch) {
                          this->state_cache.has_state_incoming_edge[mapped_state] = true;
                        }
                      }
                    } else {
                      states_for_second_check.push_back(state);
                    }
                    append_to_state = mata::nfa::Limits::max_state;
                }
            }
            // There could be epsilon transitions leading back to the same state. In such case, the state
            // should have incoming edge set
            for (auto state_to_check: states_for_second_check) {
              re2::Prog::Inst *inst = prog->inst(static_cast<int>(state_to_check));
              for (auto mapped_state: this->state_cache.state_mapping[static_cast<size_t>(inst->out())]) {
                this->state_cache.has_state_incoming_edge[mapped_state] = true;
              }
            }
        }

        /**
          * Creates all state cache vectors needed throughout the computation and saves them to the private variable state_cache.
          * It creates state cache for creating ExplicitNFA with epsilon transitions
          * @param prog RE2 prog corresponding to the parsed regex
          */
        void create_state_cache_with_epsilon(re2::Prog *prog) {
            std::vector<bool> defaultFalseVec(static_cast<size_t>(prog->size()), false);
            std::vector<bool> defaultTrueVec(static_cast<size_t>(prog->size()), true);
            this->state_cache = {
                    {}, // stateMapping all states are mapped to itself when using epsilon transitions
                    defaultFalseVec, // is_final_state holds true for states that are final, false for the rest
                    defaultFalseVec, // is_state_nop_or_cap not used when using epsilon transition
                    defaultFalseVec, // is_last holds true for states that are last, false for the rest
                    defaultTrueVec, // has_state_incoming_edge holds true all states
            };
            const auto progSize = static_cast<size_t>(prog->size());

            for (mata::nfa::State state = 0; state < progSize; state++) {
                this->state_cache.state_mapping.push_back({state});
                re2::Prog::Inst *inst = prog->inst(static_cast<int>(state));
                if (inst->last()) {
                    this->state_cache.is_last[state] = true;
                }
                if (inst->opcode() == re2::kInstMatch) {
                    this->state_cache.is_final_state[state] = true;
                }
            }
        }

        /**
         * Makes all states mapped to the state parameter final in the Nfa
         * @param state State which should be made final
         * @param nfa Nfa in which the states will be made final
         */
        void make_state_final(mata::nfa::State state, Nfa &nfa) {
            for (auto target_state: this->state_cache.state_mapping[state]) {
                // States without an incoming edge should not be in the automata
                if (!this->state_cache.has_state_incoming_edge[target_state]) {
                    continue;
                }
                nfa.final.insert(target_state);
            }
        }

        /**
         * Renumbers the states of the input_nfa to be from <0, numberOfStates>
         * @param program_size Size of the RE2 prog
         * @param input_nfa Nfa which states should be renumbered
         * @return Same Nfa as input_nfa but with states from interval <0, numberOfStates>
         */
        static Nfa renumber_states(Nfa* output_nfa,
                                              size_t program_size,
                                              Nfa &input_nfa) {
            std::vector<mata::nfa::State> renumbered_states(program_size, mata::nfa::Limits::max_state);
            Nfa& renumbered_explicit_nfa = *output_nfa;
            for (mata::nfa::State state{ 0 }; state < program_size; state++) {
                const auto& transition_list = input_nfa.delta.state_post(state);
                // If the transition list is empty, the state is not used
                if (transition_list.empty()) {
                    continue;
                } else {
                    // addNewState returns next unused state of the new NFA, so we map it to the original state
                    renumbered_states[state] = renumbered_explicit_nfa.add_state();
                }
            }

            for (auto state: input_nfa.final) {
                if (static_cast<int>(renumbered_states[state]) == -1) {
                    renumbered_states[state] = renumbered_explicit_nfa.add_state();
                }
                renumbered_explicit_nfa.final.insert(renumbered_states[state]);
            }

            for (mata::nfa::State state{ 0 }; state < program_size; state++) {
                const auto& transition_list = input_nfa.delta.state_post(state);
                for (const auto& transition: transition_list) {
                    for (auto stateTo: transition.targets) {
                        if (renumbered_states[stateTo] == mata::nfa::Limits::max_state) {
                            renumbered_states[stateTo] = renumbered_explicit_nfa.add_state();
                        }
                        assert(renumbered_states[state] <= renumbered_explicit_nfa.num_of_states());
                        assert(renumbered_states[stateTo] <= renumbered_explicit_nfa.num_of_states());
                        renumbered_explicit_nfa.delta.add(renumbered_states[state], transition.symbol,
                                                          renumbered_states[stateTo]);
                    }
                }
            }


            for (auto state: input_nfa.initial) {
                renumbered_explicit_nfa.initial.insert(renumbered_states[state]);
            }

            return renumbered_explicit_nfa;
        }

        /**
         * Gets all states that are mapped to the state (i.e., states that are within epsilon transitions chain)
         * @param prog RE2 prog corresponding to the parsed regex
         * @param state State for which the mapped states should be computed
         * @param inst RE2 instruction for the state
         * @return All states that are mapped to the state
         */
        std::vector<mata::nfa::State> get_mapped_states(
                re2::Prog* prog, mata::nfa::State state, re2::Prog::Inst *inst) {
            std::vector<mata::nfa::State> mappedStates;
            std::vector<mata::nfa::State> statesToCheck;
            std::set<mata::nfa::State> checkedStates;

            statesToCheck.push_back(state);
            while (!statesToCheck.empty()) {
                state = statesToCheck.back();
                inst = prog->inst(static_cast<int>(state));
                checkedStates.insert(state);
                statesToCheck.pop_back();
                // If the state is not last, it also has an epsilon transition which we must follow
                if (!inst->last()) {
                    re2::Prog::Inst *nextInst = prog->inst(static_cast<int>(state + 1));
                    if (nextInst->last()) {
                        this->state_cache.is_last[state + 1] = true;
                    }
                    if (checkedStates.count(state+1) == 0) {
                        statesToCheck.push_back(state+1);
                    }
                } else if (inst->opcode() != re2::kInstCapture && inst->opcode() != re2::kInstNop) {
                    // It is state with "normal" transition. It is the last state in the epsilon transitions chain
                    mappedStates.push_back(state);
                    continue;
                }
                re2::Prog::Inst *outInst = prog->inst(inst->out());
                if (outInst->opcode() == re2::kInstCapture || outInst->opcode() == re2::kInstNop) {
                    // The state has outgoing epsilon transition which we must follow
                    if (checkedStates.count(static_cast<mata::nfa::State>(inst->out())) == 0) {
                        statesToCheck.push_back(static_cast<mata::nfa::State>(inst->out()));
                    }
                } else {
                    // It is state with "normal" transition. It is the last state in the epsilon transitions chain
                    mappedStates.push_back(static_cast<mata::nfa::State>(inst->out()));
                }
            }
            return mappedStates;
        }
        };
}

 /**
 * The main method, it creates NFA from regex
 * @param pattern regex as string
 * @param use_epsilon whether to create NFA with epsilon transitions or not
 * @param epsilon_value value, that will represent epsilon on transitions
 * @param use_reduce if set to true the result is trimmed and reduced using simulation reduction
 * @return Nfa corresponding to pattern
 */
void mata::parser::create_nfa(nfa::Nfa* nfa, const std::string& pattern, bool use_epsilon, mata::Symbol epsilon_value, bool use_reduce) {
    if (nfa == nullptr) {
        throw std::runtime_error("create_nfa: nfa should not be NULL");
    }

    RegexParser regexParser{};
    auto parsed_regex = regexParser.parse_regex_string(pattern);
    auto program = parsed_regex->CompileToProg(regexParser.options.max_mem() * 2 / 3);
    regexParser.convert_pro_to_nfa(nfa, program, true, epsilon_value);
    delete program;
    // Decrements reference count and deletes object if the count reaches 0
    parsed_regex->Decref();
     //TODO: should this really be done implicitly?
    if(!use_epsilon) {
        *nfa = mata::nfa::remove_epsilon(*nfa, epsilon_value);
    }
    //TODO: in fact, maybe parser should not do trimming and reducing, maybe these operations should be done transparently.
    if(use_reduce) {
        //TODO: trimming might be unnecessary, regex->nfa construction should not produce useless states. Or does it?
        *nfa = mata::nfa::reduce(nfa->trim());
    }
}
