/* nfa.cc -- reduction on NFA
 */

#include <algorithm>
#include <ostream>
#include <string>

// MATA headers
#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/simlib/util/binary_relation.hh"
#include <mata/simlib/explicit_lts.hh>
#include <unordered_map>
#include <vector>

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

    void rename_states(StateRenaming& state_renaming,
                       State p,
                       State q)
    {
        // States p and q are in relation such that "p is simulated by q"
        state_renaming[p] = q;

        // Now the state renaming must be updated, firstly in the forward direction
        // If (q is simulated by x) and (p is simulated by q) -> (p is simulated by x)
        if (state_renaming.count(q) != 0) {
            state_renaming[p] = state_renaming[q];
        }

        // Now the backwards direction
        // if (x is simulated by p) and (p is simulated as q) -> (x is simulated as q)
        for (auto& pair: state_renaming) {
            if (pair.second == p) {
                state_renaming[pair.first] = state_renaming[p];
            }
        }
    }

    // Merging based on the third rule (From the paper 'On NFA Reduction')
    Nfa merge_in_both_directions(const Nfa& aut,
                                StateRenaming& state_renaming, 
                                const Simlib::Util::BinaryRelation& fw_relation,
                                const Simlib::Util::BinaryRelation& bw_relation)
    {
        Nfa result;

        const size_t num_of_states = aut.num_of_states();

        Simlib::Util::BinaryRelation work_relation;
        work_relation.resize(num_of_states, 0);

        // Fill the work relation
        for (size_t i = 0; i < num_of_states; i++){
            for (size_t j = 0; j < num_of_states; j++){
                work_relation.set(i, j, bw_relation.get(i, j) & fw_relation.get(i, j));
                //std::cerr << work_relation.get(i, j) << " ";
            }
            //std::cerr << std::endl;
        }

        // Set the state renaming
        std::vector<State> simulated_states {};
        for (State p = 0; p < num_of_states; p++) {
            // Find the first occurence of one (ignore diagonal)
            for (State q = 0; q < num_of_states; q++){
                // If the one is before diagonal (symmetric pairs must NOT merge one into another)
                if (work_relation.get(p, q) == 1 && p != q && p > q) {
                    // p is simulated by q, rename p
                    rename_states(state_renaming, p, q);
                    simulated_states.push_back(p);
                    break;
                }
                // If the one is after diagonal
                if (work_relation.get(p, q) == 1 && p != q && p < q) {
                    // Check for symmetry
                    if (work_relation.get(q, p) == 1){
                        continue; // Do nothing
                    }
                    rename_states(state_renaming, p, q);
                    simulated_states.push_back(p);
                    break;
                }
            }
        }

        // Rename the states the will not be deleted
        size_t renamed = 0;
        for (State p = 0; p < num_of_states; p++) {
            // The state is not simulated
            if (state_renaming.count(p) == 0) {
                // Rename the state
                state_renaming[p] = renamed;
    
                // Rename every occurence of p in state_renaming are replace it with its new name
                for (auto& pair : state_renaming) {
                    if (pair.second == p) {
                        state_renaming[pair.first] = renamed;
                    }
                }

                // Increment for the next state
                renamed++;
            }

        }

        // Add the new states 
        result.add_state(num_of_states - simulated_states.size() - 1);

        // Build the resulting automaton based on the simulated_states and state_renaming
        for (State p = 0; p < num_of_states; ++p) {
            // If the state is simulated, its not added to the resulting automaton
            if (std::find(simulated_states.begin(), simulated_states.end(), p) != simulated_states.end()) {
                continue;
            }

            // The current state
            State renamed_state = state_renaming[p];

            // Check for Initial
            if (aut.initial[p]){
                result.initial.insert(renamed_state);
            }

            // Copy transitions
            for (const auto &p_trans_symbol : aut.delta.state_post(p)) {
                for (const auto &target_state :p_trans_symbol.targets){
                    // The transition is translated
                    result.delta.add(renamed_state, p_trans_symbol.symbol, state_renaming[target_state]);
                }
            }

            // Check for Final
            if (aut.final[p]){
                result.final.insert(renamed_state);
            }

        }

        return result;
    }

    Nfa simulation_fixpoint(const Nfa& aut){
        Nfa result;
        Nfa tmp = aut;

        std::unordered_map<State,State> state_map_dummy;

        bool forward_eq = false;
        bool backward_eq = false;
        bool bidirect_eq = false;

        while (!(forward_eq && backward_eq && bidirect_eq)){

            // MERGE IN BOTH DIRECTIONS
            state_map_dummy.clear();

            const auto sim_birelation_fw = algorithms::compute_relation(
                    tmp, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});

            Nfa aut_bir = revert(tmp);
            const auto sim_birelation_bw = algorithms::compute_relation(
                    aut_bir, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});

            result = merge_in_both_directions(tmp, state_map_dummy, sim_birelation_fw, sim_birelation_bw);

            if (tmp.num_of_states() == result.num_of_states()){
                bidirect_eq = true;
            }
            else {
                bidirect_eq = false;
            }

            tmp = result;

            // MERGE IN FORWARD DIRECTION
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

            // MERGE IN BACKWARD DIRECTION
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

        // Compute the simulation based on simulation_direction
        const auto sim_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "backward"}});
        
        // Merge states based on the selected rule
        Nfa aut_r = revert(aut);
        result = merge_in_one_direction(aut_r, state_renaming, sim_relation, true);
        return revert(result);
    }
    else if (simulation_direction == "bidirect"){
        // TODO this does not support state renaming
        // Compute the forward simulation
        const auto sim_fw_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});

        // Compute the backward simulation
        const auto sim_bw_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "backward"}});
        
        // Merge states based on the third rule
        result = merge_in_both_directions(aut, state_renaming, sim_fw_relation, sim_bw_relation);

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
