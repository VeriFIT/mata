/* nfa.cc -- reduction on NFA
 */

#include <string>

// MATA headers
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include <mata/simlib/explicit_lts.hh>

using namespace mata::utils;
using namespace mata::nfa;

namespace {
    // Merging based on the first and second rule (From the paper 'On NFA Reduction')
    Nfa merge_in_one_direction(const Nfa& aut, StateRenaming& state_renaming, 
                               const Simlib::Util::BinaryRelation& sim_relation,
                               const bool little_brother){
        Nfa result;

        auto sim_relation_symmetric = sim_relation;
        sim_relation_symmetric.restrict_to_symmetric();

        // for State q, quot_proj[q] should be the representative state representing the symmetric class of states in simulation
        std::vector<size_t> quot_proj;
        sim_relation_symmetric.get_quotient_projection(quot_proj);

        const size_t num_of_states = aut.num_of_states();

        // map each state q of aut to the state of the reduced automaton representing the simulation class of q
        for (State q = 0; q < num_of_states; ++q) {
            const State qReprState = quot_proj[q];
            if (state_renaming.count(qReprState) == 0) { // we need to map q's class to a new state in reducedAut
                const State qClass = result.add_state();
                state_renaming[qReprState] = qClass;
                state_renaming[q] = qClass;
            } else {
                state_renaming[q] = state_renaming[qReprState];
            }
        }

        for (State q = 0; q < num_of_states; ++q) {
            const State q_class_state = state_renaming.at(q);

            if (aut.initial[q]) { // if a symmetric class contains initial state, then the whole class should be initial
                result.initial.insert(q_class_state);
            }

            if (quot_proj[q] == q) { // we process only transitions starting from the representative state, this is enough for simulation
                for (const auto &q_trans : aut.delta.state_post(q)) {
                    const StateSet representatives_of_states_to = [&]{
                        StateSet state_set;
                        for (auto s : q_trans.targets) {
                            state_set.insert(quot_proj[s]);
                        }
                        return state_set;
                    }();

                    // get the class states of those representatives that are not simulated by another representative in representatives_of_states_toA
                    // this technique is known as little brother elimination. It can pose problems to simulation fixpoint
                    StateSet representatives_class_states;
                    for (const State s : representatives_of_states_to) {
                        bool is_state_important = true; // if true, we need to keep the transition from q to s

                        if (little_brother){
                            for (const State p : representatives_of_states_to) {
                                if (s != p && sim_relation.get(s, p)) { // if p (different from s) simulates s
                                    is_state_important = false; // as p simulates s, the transition from q to s is not important to keep, as it is subsumed in transition from q to p
                                    break;
                                }
                            }
                        }
                        if (is_state_important) {
                            representatives_class_states.insert(state_renaming.at(s));
                        }
                    }

                    // add the transition 'q_class_state-q_trans.symbol->representatives_class_states' at the end of transition list of transitions starting from q_class_state
                    // as the q_trans.symbol should be the largest symbol we saw (as we iterate trough getTransitionsFromState(q) which is ordered)
                    result.delta.mutable_state_post(q_class_state).insert(SymbolPost(q_trans.symbol, representatives_class_states));
                }

                if (aut.final[q]) { // if q is final, then all states in its class are final => we make q_class_state final
                    result.final.insert(q_class_state);
                }
            }
        }
        return result;
    }

    // Merging based on the third rule (From the paper 'On NFA Reduction')
    Nfa merge_in_both_directions(const Nfa& aut,
                                StateRenaming& state_renaming, 
                                const Simlib::Util::BinaryRelation& fw_relation,
                                const Simlib::Util::BinaryRelation& bw_relation)
    {
        Nfa result;

        // TODO TODAY!!!

        return result;
    }

    Nfa simulation_fixpoint(const Nfa& aut){
        Nfa result;
        Nfa tmp = aut;

        std::unordered_map<State,State> state_map_dummy;

        bool forward_eq = false;
        bool backward_eq = false;

        while (!(forward_eq && backward_eq)){
            
            state_map_dummy.clear();
            
            // Compute the forward simulation TODO take out into a function
            const auto sim_relation_fw = algorithms::compute_relation(
                    tmp, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});
            result = merge_in_one_direction(tmp, state_map_dummy, sim_relation_fw, false);

            if (tmp.num_of_states() == result.num_of_states()){
                forward_eq = true;
            }
            else {
                forward_eq = false;
            }
                
            tmp = result;

            state_map_dummy.clear();

            // Compute the backward simulation TODO take out into a function
            Nfa aut_r = revert(tmp);
            const auto sim_relation_bw = algorithms::compute_relation(
                    aut_r, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});
            
            result = merge_in_one_direction(aut_r, state_map_dummy, sim_relation_bw, false);
            result = revert(result);

            if (tmp.num_of_states() == result.num_of_states()){
                backward_eq = true;
            }
            else {
                backward_eq = false;
            }

            tmp = result;
        }
    
        result = tmp;
        return result;
    }
}

Nfa mata::nfa::algorithms::reduce_size_by_simulation(const Nfa& aut, StateRenaming &state_renaming, const std::string& simulation_direction) {
    Nfa result;
    
    if (simulation_direction == "forward"){
        // Compute the simulation based on simulation_direction
        const auto sim_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});
        
        // Merge states based on the selected rule
        result = merge_in_one_direction(aut, state_renaming, sim_relation, true);
        return result;
    }
    else if (simulation_direction == "backward"){
        Nfa aut_r = revert(aut);

        // Compute the simulation based on simulation_direction
        const auto sim_relation = algorithms::compute_relation(
                aut_r, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});
        
        // Merge states based on the selected rule
        result = merge_in_one_direction(aut_r, state_renaming, sim_relation, true);
        return revert(result);
    }
    else if (simulation_direction == "bidirect"){
        // Compute the forward simulation
        const auto sim_fw_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});

        // Compute the backward simulation
        const auto sim_bw_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "backward"}});
        
        // Merge states based on the third rule
        result = merge_in_both_directions(aut, state_renaming, sim_fw_relation, sim_bw_relation);
        std::cerr << "ERROR: bidirect not implemented yet";

        return result;
    }
    else if (simulation_direction == "fixpoint"){
        // TODO this does not support state renaming
        result = simulation_fixpoint(aut);
        return result;
    }
    else {
        // TODO throw some error
        std::cerr << "TODO error" << std::endl;
    }

    return result;
}

Simlib::Util::BinaryRelation mata::nfa::algorithms::compute_direct_simulation(const Nfa& aut) {
    OrdVector<mata::Symbol> used_symbols = aut.delta.get_used_symbols();
    mata::Symbol unused_symbol = 0;
    if (!used_symbols.empty() && *used_symbols.begin() == 0) {
        auto it = used_symbols.begin();
        unused_symbol = *it + 1;
        ++it;
        const auto used_symbols_end = used_symbols.end();
        while (it != used_symbols_end && unused_symbol == *it) {
            unused_symbol = *it + 1;
            ++it;
        }
        if (unused_symbol == 0) { // sanity check to see if we did not use the full range of mata::Symbol
            throw std::runtime_error("all symbols are used, we cannot compute simulation reduction");
        }
    }

    const size_t state_num{ aut.num_of_states() };
    Simlib::ExplicitLTS lts_for_simulation(state_num);

    for (const Transition& transition : aut.delta.transitions()) {
        lts_for_simulation.add_transition(transition.source, transition.symbol, transition.target);
    }

    // final states cannot be simulated by nonfinal -> we add new selfloops over final states with new symbol in LTS
    for (State final_state : aut.final) {
        lts_for_simulation.add_transition(final_state, unused_symbol, final_state);
    }

    lts_for_simulation.init();
    return lts_for_simulation.compute_simulation();
}
