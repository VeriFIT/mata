/* nfa.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <unordered_set>
#include <iterator>

// MATA headers
#include "mata/nfa/delta.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/builder.hh"
#include <mata/simlib/explicit_lts.hh>

using std::tie;

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

namespace {
    Simlib::Util::BinaryRelation compute_fw_direct_simulation(const Nfa& aut) {
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

    bool vec_contains(std::vector<std::pair<State, State>> vec, std::pair<State, State> par){
        for (size_t i = 0; i < vec.size(); i++){
            if (vec[i] == par){
                return true;
            }
        }
        return false;
    }

    Simlib::Util::BinaryRelation compute_iny_direct_simulation(const Nfa& aut) {
        // ! Preprocessing
        Nfa reverted_nfa;
        std::vector<std::vector<bool>> result_sim_tmp {}; // R_tmp

        std::vector<std::pair<State, State>> result_sim {}; // R
        std::vector<std::pair<State, State>> worklist {}; // Worklist

        // Alphabet extraction
        mata::OnTheFlyAlphabet alph;
        aut.fill_alphabet(alph);
        std::vector<Symbol> alph_syms = alph.get_alphabet_symbols().to_vector();

        size_t no_states = aut.num_of_states();
        size_t ***matrix;

        result_sim_tmp.resize(no_states);
        for (int i = 0; i < no_states; i++){
            result_sim_tmp[i].resize(no_states, true);
        }

        // Matrix allocation
        matrix = static_cast<size_t ***>(malloc(sizeof(size_t **) * alph_syms.size()));
        for (size_t i = 0; i < alph_syms.size(); i++) {
            matrix[i] = static_cast<size_t **>(malloc(sizeof(size_t *) * no_states));
            for (size_t j = 0; j < no_states; j++) {
                matrix[i][j] = static_cast<size_t *>(malloc(sizeof(size_t) * no_states));
            }
        }

        reverted_nfa = revert(aut); // Reverted NFA
        // ! End of preprocessing

        // ! Initial refinement
        for (size_t x = 0; x < alph_syms.size(); x++) {
            for (size_t p = 0; p < no_states; p++) {
                for (size_t q = 0; q < no_states; q++) {
                    size_t q_size;
                    size_t p_size;

                    auto symbol_q = aut.delta[q].find(alph_syms[x]);
                    if (symbol_q == aut.delta[q].end()) {
                        matrix[x][p][q] = 0;
                        q_size = 0;
                    } else {
                        q_size = (*symbol_q).num_of_targets();
                        matrix[x][p][q] = q_size;
                    }

                    auto symbol_p = aut.delta[p].find(alph_syms[x]);
                    if (symbol_p == aut.delta[p].end()) {
                        p_size = 0;
                    } else {
                        p_size = (*symbol_p).num_of_targets();
                    }
                    if ((p_size != 0 && q_size == 0) || (aut.final.contains(p) && !aut.final.contains(q))) {
                        if (!vec_contains(result_sim, std::pair(p,q))) {
                            result_sim.push_back(std::pair(p,q)); // R append 
                            worklist.push_back(std::pair(p,q)); // worklist append
                            result_sim_tmp[p][q] = false;
                        }
                    }
                }
            }
        }
        // ! End of initional refinement

        // ! Propagate until fixpoint
        size_t worklist_size;
        std::pair<State, State> working_pair;
        while ((worklist_size = worklist.size()) != 0) {
            working_pair = worklist[worklist_size - 1];
            worklist.pop_back();

            for (size_t x = 0; x < alph_syms.size(); x++) {
                auto symbol_q_ = reverted_nfa.delta[working_pair.second].find(alph_syms[x]);
                if (symbol_q_ == reverted_nfa.delta[working_pair.second].end()) {
                    continue;
                } 
                for (State q: (*symbol_q_).targets.to_vector()) {
                    matrix[x][working_pair.first][q]--;
                    if (matrix[x][working_pair.first][q] == 0) {
                        auto symbol_p_ = reverted_nfa.delta[working_pair.first].find(alph_syms[x]);
                        if (symbol_p_ == reverted_nfa.delta[working_pair.first].end()) {
                            continue;
                        }
                        for (State p: (*symbol_p_).targets.to_vector()) {
                            if (!vec_contains(result_sim, std::pair(p,q))) {
                                result_sim.push_back(std::pair(p,q)); // R append 
                                worklist.push_back(std::pair(p,q)); // worklist append
                                result_sim_tmp[p][q] = false;
                            }
                        }
                    }
                }
            }
        }
        // ! End of Propagate until fixpoint

        //Free the matrix
        for (size_t i = 0; i < alph_syms.size(); i++){
            for (size_t j = 0; j < no_states; j++){
                free(matrix[i][j]);
            }
            free(matrix[i]);
        }
        free(matrix);

        //Printig of the final relation:
        /*
        std::cout << "The final relation is:" << std::endl;
        for (std::pair final: result_sim){
            std::cout << final.first << " , " << final.second << std::endl;
        }
        */
        Simlib::Util::BinaryRelation tmp = Simlib::Util::BinaryRelation(result_sim_tmp);
        return tmp; // This does not work yet, only the calculation of the relation is done
        // The result is stored in result_sim vector
    }

    Nfa reduce_size_by_simulation(const Nfa& aut, StateRenaming &state_renaming) {
        Nfa result;
        const auto sim_relation = algorithms::compute_relation(
                aut, ParameterMap{{ "relation", "simulation"}, { "direction", "forward"}});

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

                    // get the class states of those representatives that are not simulated by another representative in representatives_of_states_to
                    StateSet representatives_class_states;
                    for (const State s : representatives_of_states_to) {
                        bool is_state_important = true; // if true, we need to keep the transition from q to s
                        for (const State p : representatives_of_states_to) {
                            if (s != p && sim_relation.get(s, p)) { // if p (different from s) simulates s
                                is_state_important = false; // as p simulates s, the transition from q to s is not important to keep, as it is subsumed in transition from q to p
                                break;
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
}

namespace {
    void remove_covered_state(const StateSet& covering_set, const State remove, Nfa& nfa) {
        StateSet tmp_targets;           // help set to store elements to remove
        auto delta_begin = nfa.delta[remove].begin();
        auto remove_size = nfa.delta[remove].size();
        for (size_t i = 0; i < remove_size; i++) {        // remove trans from covered state
            tmp_targets = delta_begin->targets;
            for (const State target: tmp_targets) {
                nfa.delta.remove(remove, delta_begin->symbol, target);
            }
        }

        auto remove_transitions = nfa.delta.get_transitions_to(remove);
        for (const auto& move: remove_transitions) {                                // transfer transitions from covered state to covering set
            for (const State switch_target: covering_set) {
                nfa.delta.add(move.source, move.symbol, switch_target);
            }
            nfa.delta.remove(move);
        }

        // check final  and initial states
        nfa.final.erase(remove);
        if (nfa.initial.contains(remove)) {
            nfa.initial.erase(remove);
            for (const State new_init: covering_set) {
                nfa.initial.insert(new_init);
            }
        }
    }

    void check_covered_and_covering(std::vector<StateSet>& covering_states,                 // covering sets for each state
                                    std::vector<StateSet>& covering_indexes,                // indexes of covering states
                                    std::unordered_map<StateSet, State>& covered,           // map of covered states
                                    std::unordered_map<StateSet, State>& subset_map,        // map of non-covered states
                                    const State Tid, const StateSet& T,                      // current state to check
                                    Nfa& result) {

        auto it = subset_map.begin();

        // initiate with empty StateSets
        covering_states.emplace_back();
        covering_indexes.emplace_back();

        while (it != subset_map.end()) {               // goes through all found states
            if (it->first.is_subset_of(T)) {
                // check if T is covered
                // if so add covering state to its covering StateSet

                covering_states[Tid].insert(it->first);
                covering_indexes[Tid].insert(it->second);
            }
            else if (T.is_subset_of(it->first)) {
                // check if state in map is covered
                // if so add covering state to its covering StateSet

                covering_states[it->second].insert(T);
                covering_indexes[it->second].insert(Tid);

                // check is some already existing state that had a new covering state added turned fully covered
                if (it->first == covering_states[it->second]) {
                    // if any covered state is in the covering set of newly turned covered state,
                    // then it has to be replaced by its covering set
                    //
                    // same applies for any covered state, if it contains newly turned state in theirs
                    // covering set, then it has to be updated
                    State erase_state = it->second;      // covered state to remove
                    for (const auto& covered_pair: covered) {
                        if (covering_indexes[covered_pair.second].contains(erase_state)) {
                            covering_indexes[covered_pair.second].erase(erase_state);
                            covering_indexes[covered_pair.second].insert(covering_indexes[erase_state]);
                        }
                        if (covering_indexes[erase_state].contains(covered_pair.second)) {
                            covering_indexes[erase_state].erase(covered_pair.second);
                            covering_indexes[erase_state].insert(covering_indexes[covered_pair.second]);
                        }
                    }

                    // remove covered state from the automaton, replace with covering set
                    remove_covered_state(covering_indexes[erase_state], erase_state, result);

                    auto temp = it++;
                    // move state from subset_map to covered
                    auto transfer = subset_map.extract(temp);
                    covered.insert(std::move(transfer));
                    continue;           // skip increasing map pointer
                }
            }
            ++it;
        }
    }

    Nfa residual_with(const Nfa& aut) {         // modified algorithm of determinization

        Nfa result;

        //assuming all sets targets are non-empty
        std::vector<std::pair<State, StateSet>> worklist;
        std::unordered_map<StateSet, State> subset_map;

        std::vector<StateSet> covering_states;          // check covering set
        std::vector<StateSet> covering_indexes;         // indexes of covering macrostates
        std::unordered_map<StateSet, State> covered;    // map of covered states for transfering new transitions

        result.clear();
        const StateSet S0 =  StateSet(aut.initial);
        const State S0id = result.add_state();
        result.initial.insert(S0id);

        if (aut.final.intersects_with(S0)) {
            result.final.insert(S0id);
        }
        worklist.emplace_back(S0id, S0);

        (subset_map)[mata::utils::OrdVector<State>(S0)] = S0id;
        covering_states.emplace_back();
        covering_indexes.emplace_back();

        if (aut.delta.empty()){
            return result;
        }

        using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
        SynchronizedExistentialSymbolPostIterator synchronized_iterator;

        while (!worklist.empty()) {
            const auto Spair = worklist.back();
            worklist.pop_back();
            const StateSet S = Spair.second;
            const State Sid = Spair.first;
            if (S.empty()) {
                // This should not happen assuming all sets targets are non-empty.
                break;
            }

            // add moves of S to the sync ex iterator
            synchronized_iterator.reset();
            for (State q: S) {
                mata::utils::push_back(synchronized_iterator, aut.delta[q]);
            }
  
            while (synchronized_iterator.advance()) {
                bool add = false;               // check whether to add transitions

                // extract post from the sychronized_iterator iterator
                const std::vector<Iterator>& moves = synchronized_iterator.get_current();
                Symbol currentSymbol = (*moves.begin())->symbol;
                StateSet T = synchronized_iterator.unify_targets(); // new state unify

                auto existingTitr = subset_map.find(T);        // check if state was alredy discovered
                State Tid;
                if (existingTitr != subset_map.end()) {        // already visited state
                    Tid = existingTitr->second;
                    add = true;
                }
                else if ((existingTitr = covered.find(T)) != covered.end()) {
                    Tid = existingTitr->second;
                } else {                                        // add new state
                    Tid = result.add_state();
                    check_covered_and_covering(covering_states, covering_indexes, covered, subset_map, Tid, T, result);

                    if (T != covering_states[Tid]){     // new state is not covered, replace transitions
                        subset_map[mata::utils::OrdVector<State>(T)] = Tid;      // add to map

                        if (aut.final.intersects_with(T))                      // add to final
                            result.final.insert(Tid);

                        worklist.emplace_back(Tid, T);
                        add  = true;

                    } else {            // new state is covered
                        covered[mata::utils::OrdVector<State>(T)] = Tid;
                    }
                }

                if (covered.find(S) != covered.end()) {
                    continue;           // skip generationg any transitions as the source state was covered right now
                }

                if (add) {
                    result.delta.mutable_state_post(Sid).insert(SymbolPost(currentSymbol, Tid));
                } else {
                    for (State switch_target: covering_indexes[Tid]){
                            result.delta.add(Sid, currentSymbol, switch_target);
                    }
                }
            }
        }

        return result;
    }

    void residual_recurse_coverable(const std::vector <StateSet>& macrostate_vec,   // vector of nfa macrostates
                                    const std::vector <State>& covering_indexes,    // sub-vector of macrostates indexes
                                    std::vector <bool>& covered,                    // flags of covered states
                                    std::vector <bool>& visited,                    // flags fo visited states
                                    size_t start_index,                      // starting index for covering_indexes vec
                                    std::unordered_map<StateSet, State> *subset_map,    // mapping of indexes to macrostates
                                    Nfa& nfa) {

        StateSet check_state = macrostate_vec[covering_indexes[start_index]];
        StateSet covering_set;                      // doesn't contain duplicates
        std::vector<State> sub_covering_indexes;    // // indexes of covering states

        for (auto i = covering_indexes.begin() + static_cast<long int>(start_index+1), e = covering_indexes.end(); i != e; i++) {
            if (covered[*i])           // was aready processed
                continue;

            if (macrostate_vec[*i].is_subset_of(check_state)) {
                covering_set.insert(macrostate_vec[*i]);                // is never covered
                sub_covering_indexes.push_back(*i);
            }
        }

        if (covering_set == check_state) {       // can recurse even without covered :thinking:

            size_t covering_size = sub_covering_indexes.size()-1;
            for (size_t k = 0; k < covering_size; k++) {
                if (macrostate_vec[sub_covering_indexes[k]].size() == 1)            // end on single-sized states
                    break;

                if (visited[sub_covering_indexes[k]])                               // already processed
                    continue;

                visited[sub_covering_indexes[k]] = true;

                residual_recurse_coverable(macrostate_vec, sub_covering_indexes, covered, visited, k, subset_map, nfa);
            }

            covering_set.clear();                 // clear variable to store only needed macrostates
            for (auto index : sub_covering_indexes) {
                if (covered[index] == 0) {
                    auto macrostate_ptr = subset_map->find(macrostate_vec[index]);
                        if (macrostate_ptr == subset_map->end())        // should never happen
                             throw std::runtime_error(std::to_string(__func__) + " couldn't find expected element in a map.");

                    covering_set.insert(macrostate_ptr->second);
                }
            }

            remove_covered_state(covering_set, subset_map->find(check_state)->second, nfa);
            covered[covering_indexes[start_index]] = true;
        }


    }

    Nfa residual_after(const Nfa&  aut) {
        std::unordered_map<StateSet, State> subset_map{};
        Nfa result;
        result = determinize(aut, &subset_map);

        std::vector <StateSet> macrostate_vec;              // ordered vector of macrostates
        macrostate_vec.reserve(subset_map.size());
        for (const auto& pair: subset_map) {                   // order by size from largest to smallest
            macrostate_vec.insert(std::upper_bound(macrostate_vec.begin(), macrostate_vec.end(), pair.first,
                                [](const StateSet & a, const StateSet & b){ return a.size() > b.size(); }), pair.first);
        }

        std::vector <bool> covered(subset_map.size(), false);          // flag of covered states, removed from nfa
        std::vector <bool> visited(subset_map.size(), false);          // flag of processed state

        StateSet covering_set;                // doesn't contain duplicates
        std::vector<State> covering_indexes;        // indexes of covering states
        size_t macrostate_size = macrostate_vec.size();
        for (size_t i = 0; i < macrostate_size-1; i++) {
            if (macrostate_vec[i].size() == 1)      // end searching on single-sized macrostates
                break;

            if (visited[i])                         // was already processed
                continue;

            covering_set.clear();
            covering_indexes.clear();
            visited[i] = true;

            for (size_t j = i+1; j < macrostate_size; j++) {        // find covering macrostates
                if (covered[j])     // if covered there are smaller macrostates, skip
                    continue;

                if (macrostate_vec[j].is_subset_of(macrostate_vec[i])) {           // found covering state
                    covering_set.insert(macrostate_vec[j]);               // is not covered
                    covering_indexes.push_back(j);
                }
            }

            if (covering_set == macrostate_vec[i]) {
                size_t covering_size = covering_indexes.size()-1;
                for (size_t k = 0; k < covering_size; k++) {      // check resurse coverability
                    if (macrostate_vec[covering_indexes[k]].size() == 1)            // end on single-sized
                        break;

                    if (visited[covering_indexes[k]])                               // already processed
                        continue;

                    visited[covering_indexes[k]] = true;

                    residual_recurse_coverable(macrostate_vec, covering_indexes, covered, visited, k, &subset_map, result);
                }

                covering_set.clear();                 // clear variable to store only needed macrostates
                for (auto index : covering_indexes) {
                    if (covered[index] == 0) {
                        auto macrostate_ptr = subset_map.find(macrostate_vec[index]);
                        if (macrostate_ptr == subset_map.end())        // should never happen
                             throw std::runtime_error(std::to_string(__func__) + " couldn't find expected element in a map.");

                        covering_set.insert(macrostate_ptr->second);
                    }
                }

                remove_covered_state(covering_set, subset_map.find(macrostate_vec[i])->second, result);
                covered[i] = true;
            }
        }

        return result;
    }

    Nfa reduce_size_by_residual(const Nfa& aut, StateRenaming &state_renaming, const std::string& type, const std::string& direction){
        Nfa back_determinized = aut;
        Nfa result;

        if (direction != "forward" && direction != "backward"){
            throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"direction\" key: " + direction);
        }

        // forward canonical residual automaton is firstly backward determinized and
        // then the residual construction is done forward, for backward residual automaton
        // is it the opposite, so the automaton is reverted once more before and after
        // construction, however the first two reversion negate each other out
        if (direction == "forward")
            back_determinized = revert(back_determinized);
        back_determinized = revert(determinize(back_determinized));          // backward deteminization

        // not relly sure how to handle state_renaming
        (void) state_renaming;

        // two different implementations of the same algorithm, for type "after" the
        // residual automaton and removal of covering states is done after the final
        // determinization if finished, for type "with" this residual construction is
        // done during the last determinization, both types had similar results in
        // effectivity, their output is almost the same expect the transitions, those
        // may slightly differ, but number of states is the same for both types
        if (type == "with") {
            result = residual_with(back_determinized);
        }
        else if (type == "after") {
            result = residual_after(back_determinized);
        } else {
            throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"type\" key: " + type);
        }

        if (direction == "backward")
            result = revert(result);

        return result.trim();
    }
}

std::ostream &std::operator<<(std::ostream &os, const mata::nfa::Transition &trans) { // {{{
    std::string result = "(" + std::to_string(trans.source) + ", " +
                         std::to_string(trans.symbol) + ", " + std::to_string(trans.target) + ")";
    return os << result;
}

bool mata::nfa::Nfa::make_complete(const Alphabet* const alphabet, const std::optional<State> sink_state) {
    return make_complete(get_symbols_to_work_with(*this, alphabet), sink_state);
}

bool mata::nfa::Nfa::make_complete(const OrdVector<Symbol>& symbols, const std::optional<State> sink_state) {
    bool transition_added{ false };
    const size_t num_of_states{ this->num_of_states() };
    const State sink_state_val{ sink_state.value_or(num_of_states) };

    OrdVector<Symbol> used_symbols{};
    for (State state{ 0 }; state < num_of_states; ++state) {
        for (const SymbolPost& symbol_post: delta[state]) {
            used_symbols.insert(symbol_post.symbol);
        }
        const OrdVector<Symbol> unused_symbols{ symbols.difference(used_symbols) };
        for (const Symbol symbol: unused_symbols) {
            delta.add(state, symbol, sink_state_val);
            transition_added = true;
        }
        used_symbols.clear();
    }

    if (transition_added && num_of_states <= sink_state_val) {
        for (const Symbol symbol: symbols) {
            delta.add(sink_state_val, symbol, sink_state_val);
        }
    }

    return transition_added;
}

//TODO: based on the comments inside, this function needs to be rewritten in a more optimal way.
Nfa mata::nfa::remove_epsilon(const Nfa& aut, Symbol epsilon) {
    // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
    std::unordered_map<State, StateSet> eps_closure;

    // TODO: grossly inefficient
    // first we compute the epsilon closure
    const size_t num_of_states{aut.num_of_states() };
    for (size_t i{ 0 }; i < num_of_states; ++i)
    {
        for (const auto& trans: aut.delta[i])
        { // initialize
            const auto it_ins_pair = eps_closure.insert({i, {i}});
            if (trans.symbol == epsilon)
            {
                StateSet& closure = it_ins_pair.first->second;
                // TODO: Fix possibly insert to OrdVector. Create list already ordered, then merge (do not need to resize each time);
                closure.insert(trans.targets);
            }
        }
    }

    bool changed = true;
    while (changed) { // Compute the fixpoint.
        changed = false;
        for (size_t i = 0; i < num_of_states; ++i) {
            const StatePost& post{ aut.delta[i] };
            const auto eps_move_it { post.find(epsilon) };//TODO: make faster if default epsilon
            if (eps_move_it != post.end()) {
                StateSet& src_eps_cl = eps_closure[i];
                for (const State tgt: eps_move_it->targets) {
                    const StateSet& tgt_eps_cl = eps_closure[tgt];
                    for (const State st: tgt_eps_cl) {
                        if (src_eps_cl.count(st) == 0) {
                            changed = true;
                            break;
                        }
                    }
                    src_eps_cl.insert(tgt_eps_cl);
                }
            }
        }
    }

    // Construct the automaton without epsilon transitions.
    Nfa result{ Delta{}, aut.initial, aut.final, aut.alphabet };
    for (const auto& state_closure_pair : eps_closure) { // For every state.
        State src_state = state_closure_pair.first;
        for (State eps_cl_state : state_closure_pair.second) { // For every state in its epsilon closure.
            if (aut.final[eps_cl_state]) result.final.insert(src_state);
            for (const SymbolPost& move : aut.delta[eps_cl_state]) {
                if (move.symbol == epsilon) continue;
                // TODO: this could be done more efficiently if we had a better add method
                for (State tgt_state : move.targets) {
                    result.delta.add(src_state, move.symbol, tgt_state);
                }
            }
        }
    }
    return result;
}

Nfa mata::nfa::fragile_revert(const Nfa& aut) {
    const size_t num_of_states{ aut.num_of_states() };

    Nfa result(num_of_states);

    result.initial = aut.final;
    result.final = aut.initial;

    // Compute non-epsilon symbols.
    OrdVector<Symbol> symbols = aut.delta.get_used_symbols();
    if (symbols.empty()) { return result; }
    if (symbols.back() == EPSILON) { symbols.pop_back(); }
    // size of the "used alphabet", i.e. max symbol+1 or 0
    Symbol alphasize =  (symbols.empty()) ? 0 : (symbols.back()+1);

#ifdef _STATIC_STRUCTURES_
    //STATIC DATA STRUCTURES:
    // Not sure that it works ideally, whether the space for the inner vectors stays there.
    static std::vector<std::vector<State>> sources;
    static std::vector<std::vector<State>> targets;
    static std::vector<State> e_sources;
    static std::vector<State> e_targets;
    if (alphasize>sources.size()) {
        sources.resize(alphasize);
        targets.resize(alphasize);
    }

    e_sources.clear();
    e_targets.clear();

    //WHEN ONLY MAX SYMBOL IS COMPUTED
    // for (int i = 0;i<alphasize;i++) {
    //     for (int i = 0;i<alphasize;i++) {
    //         if (!sources[i].empty())
    //         {
    //             sources[i].resize(0);
    //             targets[i].resize(0);
    //         }
    //     }
    // }

    //WHEN ALL SYMBOLS ARE COMPUTED
    for (Symbol symbol: symbols) {
        if(!sources[symbol].empty()) {
            sources[symbol].clear();
            targets[symbol].clear();
        }
    }
#else
    // NORMAL, NON STATIC DATA STRUCTURES:
    //All transition of delta are to be copied here, into two arrays of transition sources and targets indexed by the transition symbol.
    // There is a special treatment for epsilon, since we want the arrays to be only as long as the largest symbol in the automaton,
    // and epsilon is the maximum (so we don't want to have the maximum array lenght whenever epsilon is present)
    std::vector<std::vector<State>> sources (alphasize);
    std::vector<std::vector<State>> targets (alphasize);
    std::vector<State> e_sources;
    std::vector<State> e_targets;
#endif

    //Copy all transition with non-e symbols to the arrays of sources and targets indexed by symbols.
    //Targets and sources of e-transitions go to the special place.
    //Important: since we are going through delta in order of sources, the sources arrays are all ordered.
    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const SymbolPost &move: aut.delta[sourceState]) {
            if (move.symbol == EPSILON) {
                for (const State targetState: move.targets) {
                    //reserve_on_insert(e_sources);
                    e_sources.push_back(sourceState);
                    //reserve_on_insert(e_targets);
                    e_targets.push_back(targetState);
                }
            }
            else {
                for (const State targetState: move.targets) {
                    //reserve_on_insert(sources[move.symbol]);
                    sources[move.symbol].push_back(sourceState);
                    //reserve_on_insert(targets[move.symbol]);
                    targets[move.symbol].push_back(targetState);
                }
            }
        }
    }

    //Now make the delta of the reversed automaton.
    //Important: since sources are ordered, when adding them as targets, we can just push them back.
    result.delta.reserve(num_of_states);

    // adding non-e transitions
    for (const Symbol symbol: symbols) {
        for (size_t i{ 0 }; i < sources[symbol].size(); ++i) {
            State tgt_state =sources[symbol][i];
            State src_state =targets[symbol][i];
            StatePost & src_post = result.delta.mutable_state_post(src_state);
            if (src_post.empty() || src_post.back().symbol != symbol) {
                src_post.push_back(SymbolPost(symbol));
            }
            src_post.back().push_back(tgt_state);
        }
    }

    // adding e-transitions
    for (size_t i{ 0 }; i < e_sources.size(); ++i) {
        State tgt_state =e_sources[i];
        State src_state =e_targets[i];
        StatePost & src_post = result.delta.mutable_state_post(src_state);
        if (src_post.empty() || src_post.back().symbol != EPSILON) {
            src_post.push_back(SymbolPost(EPSILON));
        }
        src_post.back().push_back(tgt_state);
    }

    //sorting the targets
    //Hm I don't know why I put this here, but it should not be needed ...
    //for (State q = 0, states_num = result.delta.post_size(); q<states_num;++q) {
    //    for (auto m = result.delta.get_mutable_post(q).begin(); m != result.delta.get_mutable_post(q).end(); ++m) {
    //        sort_and_rmdupl(m->targets);
    //    }
    //}

    return result;
}

Nfa mata::nfa::simple_revert(const Nfa& aut) {
    Nfa result;
    result.clear();

    const size_t num_of_states{ aut.num_of_states() };
    result.delta.allocate(num_of_states);

    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const SymbolPost &transition: aut.delta[sourceState]) {
            for (const State targetState: transition.targets) {
                result.delta.add(targetState, transition.symbol, sourceState);
            }
        }
    }

    result.initial = aut.final;
    result.final = aut.initial;

    return result;
}

//not so great, can be removed
Nfa mata::nfa::somewhat_simple_revert(const Nfa& aut) {
    const size_t num_of_states{ aut.num_of_states() };

    Nfa result(num_of_states);

    result.initial = aut.final;
    result.final = aut.initial;

    for (State sourceState{ 0 }; sourceState < num_of_states; ++sourceState) {
        for (const SymbolPost &transition: aut.delta[sourceState]) {
            for (const State targetState: transition.targets) {
                StatePost & post = result.delta.mutable_state_post(targetState);
                //auto move = std::find(post.begin(),post.end(),Move(transition.symbol));
                auto move = post.find(SymbolPost(transition.symbol));
                if (move == post.end()) {
                    //post.push_back(Move(transition.symbol,sourceState));
                    post.insert(SymbolPost(transition.symbol, sourceState));
                }
                else
                    move->push_back(sourceState);
                    //move->insert(sourceState);
            }
        }
    }

    //sorting the targets
    for (State q = 0, states_num = result.delta.num_of_states(); q < states_num; ++q) {
        //Post & post = result.delta.get_mutable_post(q);
        //utils::sort_and_rmdupl(post);
        for (SymbolPost& m: result.delta.mutable_state_post(q)) { sort_and_rmdupl(m.targets); }
    }

    return result;
}

Nfa mata::nfa::revert(const Nfa& aut) {
    return simple_revert(aut);
    //return fragile_revert(aut);
    //return somewhat_simple_revert(aut);
}

bool mata::nfa::Nfa::is_deterministic() const {
    if (initial.size() != 1) { return false; }

    if (delta.empty()) { return true; }

    const size_t aut_size = num_of_states();
    for (size_t i = 0; i < aut_size; ++i) {
        for (const auto& symStates : delta[i]) {
            if (symStates.num_of_targets() != 1) { return false; }
        }
    }

    return true;
}
bool mata::nfa::Nfa::is_complete(Alphabet const* alphabet) const {
    utils::OrdVector<Symbol> symbols{ get_symbols_to_work_with(*this, alphabet) };
    utils::OrdVector<Symbol> symbs_ls{ symbols };

    // TODO: make a general function for traversal over reachable states that can be shared by other functions?
    std::list<State> worklist(initial.begin(), initial.end());
    std::unordered_set<State> processed(initial.begin(), initial.end());

    while (!worklist.empty()) {
        State state = *worklist.begin();
        worklist.pop_front();

        size_t n = 0;      // counter of symbols
        if (!delta.empty()) {
            for (const auto &symb_stateset: delta[state]) {
                ++n;
                if (!haskey(symbols, symb_stateset.symbol)) {
                    throw std::runtime_error(std::to_string(__func__) +
                                             ": encountered a symbol that is not in the provided alphabet");
                }

                for (const auto &tgt_state: symb_stateset.targets) {
                    bool inserted;
                    tie(std::ignore, inserted) = processed.insert(tgt_state);
                    if (inserted) { worklist.push_back(tgt_state); }
                }
            }
        }

        if (symbols.size() != n) { return false; }
    }

    return true;
}

std::pair<Run, bool> mata::nfa::Nfa::get_word_for_path(const Run& run) const {
    if (run.path.empty()) { return {{}, true}; }

    Run word;
    State cur = run.path[0];
    for (size_t i = 1; i < run.path.size(); ++i) {
        State newSt = run.path[i];
        bool found = false;
        if (!this->delta.empty()) {
            for (const auto &symbolMap: this->delta[cur]) {
                for (State st: symbolMap.targets) {
                    if (st == newSt) {
                        word.word.push_back(symbolMap.symbol);
                        found = true;
                        break;
                    }
                }
                if (found) { break; }
            }
        }
        if (!found) { return {{}, false}; }
        cur = newSt;    // update current state
    }
    return {word, true};
}

//TODO: this is not efficient
bool mata::nfa::Nfa::is_in_lang(const Run& run) const {
    StateSet current_post(this->initial);
    for (const Symbol sym : run.word) {
        current_post = this->post(current_post, sym);
        if (current_post.empty()) { return false; }
    }
    return this->final.intersects_with(current_post);
}

/// Checks whether the prefix of a string is in the language of an automaton
// TODO: slow and it should share code with is_in_lang
bool mata::nfa::Nfa::is_prfx_in_lang(const Run& run) const {
    StateSet current_post{ this->initial };
    for (const Symbol sym : run.word) {
        if (this->final.intersects_with(current_post)) { return true; }
        current_post = this->post(current_post, sym);
        if (current_post.empty()) { return false; }
    }
    return this->final.intersects_with(current_post);
}

bool mata::nfa::Nfa::is_lang_empty(Run* cex) const {
    //TOOD: hot fix for performance reasons for TACAS.
    // Perhaps make the get_useful_states return a witness on demand somehow.
    if (!cex) {
        return is_lang_empty_scc();
    }

    std::list<State> worklist(initial.begin(), initial.end());
    std::unordered_set<State> processed(initial.begin(), initial.end());

    // 'paths[s] == t' denotes that state 's' was accessed from state 't',
    // 'paths[s] == s' means that 's' is an initial state
    std::map<State, State> paths;
    // Initialize paths.
    for (const State s: worklist) { paths[s] = s; }

    State state;
    while (!worklist.empty()) {
        state = worklist.front();
        worklist.pop_front();

        if (final[state]) {
            if (nullptr != cex) {
                cex->path.clear();
                cex->path.push_back(state);
                while (paths[state] != state) {
                    state = paths[state];
                    cex->path.push_back(state);
                }
                std::reverse(cex->path.begin(), cex->path.end());
                cex->word = this->get_word_for_path(*cex).first.word;
            }
            return false;
        }

        if (delta.empty()) { continue; }

        for (const SymbolPost& symbol_post: delta[state]) {
            for (const State& target: symbol_post.targets) {
                bool inserted;
                tie(std::ignore, inserted) = processed.insert(target);
                if (inserted) {
                    worklist.push_back(target);
                    // Also set that tgt_state was accessed from state.
                    paths[target] = state;
                } else { assert(haskey(paths, target)); /* Invariant. */ }
            }
        }
    } // while (!worklist.empty()).
    return true;
} // is_lang_empty().


Nfa mata::nfa::algorithms::minimize_brzozowski(const Nfa& aut) {
    //compute the minimal deterministic automaton, Brzozovski algorithm
    return determinize(revert(determinize(revert(aut))));
}

Nfa mata::nfa::minimize(
                const Nfa& aut,
                const ParameterMap& params)
{
    Nfa result;
    // setting the default algorithm
    decltype(algorithms::minimize_brzozowski)* algo = algorithms::minimize_brzozowski;
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
            " requires setting the \"algo\" key in the \"params\" argument; "
            "received: " + std::to_string(params));
    }

    const std::string& str_algo = params.at("algorithm");
    if ("brzozowski" == str_algo) {  /* default */ }
    else {
        throw std::runtime_error(std::to_string(__func__) +
            " received an unknown value of the \"algo\" key: " + str_algo);
    }

    return algo(aut);
}

Nfa mata::nfa::intersection(const Nfa& lhs, const Nfa& rhs, const Symbol first_epsilon, std::unordered_map<std::pair<State, State>, State>  *prod_map) {

    auto both_final = [&](const State lhs_state,const State rhs_state) {
        return lhs.final.contains(lhs_state) && rhs.final.contains(rhs_state);
    };

    if (lhs.final.empty() || lhs.initial.empty() || rhs.initial.empty() || rhs.final.empty())
        return Nfa{};

    return algorithms::product(lhs, rhs, both_final, first_epsilon, prod_map);
}

Nfa mata::nfa::union_product(const Nfa &lhs, const Nfa &rhs, const Symbol first_epsilon, std::unordered_map<std::pair<State,State>,State> *prod_map) {
    auto one_final = [&](const State lhs_state,const State rhs_state) {
        return lhs.final.contains(lhs_state) || rhs.final.contains(rhs_state);
    };

    if (lhs.final.empty() || lhs.initial.empty()) { return rhs; }
    if (rhs.final.empty() || rhs.initial.empty()) { return lhs; }
    return algorithms::product(lhs, rhs, one_final, first_epsilon, prod_map);
}

Nfa mata::nfa::union_nondet(const Nfa &lhs, const Nfa &rhs) { return Nfa{ lhs }.unite_nondet_with(rhs); }

Simlib::Util::BinaryRelation mata::nfa::algorithms::compute_relation(const Nfa& aut, const ParameterMap& params) {
    if (!haskey(params, "relation")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"relation\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }
    if (!haskey(params, "direction")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"direction\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    const std::string& relation = params.at("relation");
    const std::string& direction = params.at("direction");
    if ("simulation" == relation && direction == "forward") {
        return compute_fw_direct_simulation(aut);
    }
    else if ("simulation" == relation && direction == "iny") {
        return compute_iny_direct_simulation(aut);
    }
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"relation\" key: " + relation);
    }
}

Nfa mata::nfa::reduce(const Nfa &aut, StateRenaming *state_renaming, const ParameterMap& params) {
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algorithm\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    Nfa result;
    std::unordered_map<State,State> reduced_state_map;
    const std::string& algorithm = params.at("algorithm");
    if ("simulation" == algorithm) {
        result = reduce_size_by_simulation(aut, reduced_state_map);
    }
    else if ("residual" == algorithm) {
        // reduce type either 'after' or 'with' creation of residual automaton
        if (!haskey(params, "type")) {
            throw std::runtime_error(std::to_string(__func__) +
                                    " requires setting the \"type\" key in the \"params\" argument; "
                                    "received: " + std::to_string(params));
        }
        // forward or backward canonical residual automaton
        if (!haskey(params, "direction")) {
            throw std::runtime_error(std::to_string(__func__) +
                                    " requires setting the \"direction\" key in the \"params\" argument; "
                                    "received: " + std::to_string(params));
        }

        const std::string& residual_type = params.at("type");
        const std::string& residual_direction = params.at("direction");

        result = reduce_size_by_residual(aut, reduced_state_map, residual_type, residual_direction);
    } else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algorithm\" key: " + algorithm);
    }

    if (state_renaming) {
        state_renaming->clear();
        *state_renaming = reduced_state_map;
    }
    return result;
}

Nfa mata::nfa::determinize(
    const Nfa&  aut, std::unordered_map<StateSet, State>* subset_map,
    std::optional<std::function<bool(const Nfa&, const State, const StateSet&)>> macrostate_discover
) {
    Nfa result{};
    //assuming all sets targets are non-empty
    std::vector<std::pair<State, StateSet>> worklist{};
    std::unordered_map<StateSet, State> subset_map_local{};
    if (subset_map == nullptr) { subset_map = &subset_map_local; }

    const StateSet S0{ aut.initial };
    const State S0id{ result.add_state() };
    result.initial.insert(S0id);

    if (aut.final.intersects_with(S0)) {
        result.final.insert(S0id);
    }
    worklist.emplace_back(S0id, S0);
    (*subset_map)[mata::utils::OrdVector<State>(S0)] = S0id;
    if (aut.delta.empty()) { return result; }
    if (macrostate_discover.has_value() && !(*macrostate_discover)(result, S0id, S0)) { return result; }

    using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
    SynchronizedExistentialSymbolPostIterator synchronized_iterator;

    while (!worklist.empty()) {
        const auto Spair = worklist.back();
        worklist.pop_back();
        const StateSet S = Spair.second;
        const State Sid = Spair.first;
        if (S.empty()) {
            // This should not happen assuming all sets targets are non-empty.
            break;
        }

        // add moves of S to the sync ex iterator
        synchronized_iterator.reset();
        for (State q: S) {
            mata::utils::push_back(synchronized_iterator, aut.delta[q]);
        }

        while (synchronized_iterator.advance()) {
            // extract post from the synchronized_iterator iterator
            const std::vector<Iterator>& symbol_posts = synchronized_iterator.get_current();
            Symbol currentSymbol = (*symbol_posts.begin())->symbol;
            StateSet T = synchronized_iterator.unify_targets();

            const auto existingTitr = subset_map->find(T);
            State Tid;
            if (existingTitr != subset_map->end()) {
                Tid = existingTitr->second;
            } else {
                Tid = result.add_state();
                (*subset_map)[mata::utils::OrdVector<State>(T)] = Tid;
                if (aut.final.intersects_with(T)) {
                    result.final.insert(Tid);
                }
                worklist.emplace_back(Tid, T);
            }
            result.delta.mutable_state_post(Sid).insert(SymbolPost(currentSymbol, Tid));
            if (macrostate_discover.has_value() && existingTitr == subset_map->end()
                && !(*macrostate_discover)(result, Tid, T)) { return result; }
        }
    }
    return result;
}

std::ostream& std::operator<<(std::ostream& os, const Nfa& nfa) {
    nfa.print_to_mata(os);
    return os;
}

void mata::nfa::Nfa::fill_alphabet(OnTheFlyAlphabet& alphabet_to_fill) const {
    for (const StatePost& state_post: this->delta) {
        for (const SymbolPost& symbol_post: state_post) {
            alphabet_to_fill.update_next_symbol_value(symbol_post.symbol);
            alphabet_to_fill.try_add_new_symbol(std::to_string(symbol_post.symbol), symbol_post.symbol);
        }
    }
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<const Nfa>>& nfas) {
    mata::OnTheFlyAlphabet alphabet{};
    for (const auto& nfa: nfas) {
        nfa.get().fill_alphabet(alphabet);
    }
    return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<Nfa>>& nfas) {
    mata::OnTheFlyAlphabet alphabet{};
    for (const auto& nfa: nfas) {
        nfa.get().fill_alphabet(alphabet);
    }
    return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<const Nfa *>& nfas) {
    mata::OnTheFlyAlphabet alphabet{};
    for (const Nfa* const nfa: nfas) {
        nfa->fill_alphabet(alphabet);
    }
    return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<Nfa*>& nfas) {
    mata::OnTheFlyAlphabet alphabet{};
    for (const Nfa* const nfa: nfas) {
        nfa->fill_alphabet(alphabet);
    }
    return alphabet;
}

Run mata::nfa::encode_word(const Alphabet* alphabet, const std::vector<std::string>& input) {
    return { .word = alphabet->translate_word(input) };
}

std::set<mata::Word> mata::nfa::Nfa::get_words(unsigned max_length) const {
    std::set<mata::Word> result;

    // contains a pair: a state s and the word with which we got to the state s
    std::vector<std::pair<State, mata::Word>> worklist;
    // initializing worklist
    for (State init_state : initial) {
        worklist.push_back({init_state, {}});
        if (final.contains(init_state)) {
            result.insert(mata::Word());
        }
    }
    
    // will be used during the loop
    std::vector<std::pair<State, mata::Word>> new_worklist;

    unsigned cur_length = 0;
    while (!worklist.empty() && cur_length < max_length) {
        new_worklist.clear();
        for (const auto& state_and_word : worklist) {
            State s_from = state_and_word.first;
            const mata::Word& word = state_and_word.second;
            for (const SymbolPost& sp : delta[s_from]) {
                mata::Word new_word = word;
                new_word.push_back(sp.symbol);
                for (State s_to : sp.targets) {
                    new_worklist.emplace_back(s_to, new_word);
                    if (final.contains(s_to)) {
                        result.insert(new_word);
                    }
                }
            }
        }
        worklist.swap(new_worklist);
        ++cur_length;
    }

    return result;
}

OrdVector<Symbol> mata::nfa::get_symbols_to_work_with(const Nfa& nfa, const mata::Alphabet *const shared_alphabet) {
    if (shared_alphabet != nullptr) { return shared_alphabet->get_alphabet_symbols(); }
    else if (nfa.alphabet != nullptr) { return nfa.alphabet->get_alphabet_symbols(); }
    else { return nfa.delta.get_used_symbols(); }
}

std::optional<mata::Word> Nfa::get_word(const Symbol first_epsilon) const {
    if (initial.empty() || final.empty()) { return std::nullopt; }

    std::vector<std::pair<State, Word>> worklist{};
    for (const State initial_state: initial) {
        if (final.contains(initial_state)) { return Word{}; }
        worklist.emplace_back(initial_state, Word{});
    }
    std::vector<bool> searched(num_of_states());

    while (!worklist.empty()) {
        auto [state, word]{ std::move(worklist.back()) };
        worklist.pop_back();
        for (const Move move: delta[state].moves()) {
            if (searched[move.target]) { continue; }
            Word target_word{ word };
            if (move.symbol < first_epsilon) { target_word.push_back(move.symbol); }
            if (final.contains(move.target)) { return target_word; }
            worklist.emplace_back(move.target, target_word);
            searched[move.target] = true;
        }
    }
    return std::nullopt;
}

std::optional<mata::Word> Nfa::get_word_from_complement(const Alphabet* alphabet) const {
    if (are_disjoint(initial, final)) { return Word{}; }

    std::vector<std::unordered_map<StateSet, State>::const_pointer> worklist{};
    std::unordered_map<StateSet, State> subset_map{};

    Nfa nfa_complete{};
    const State sink_state{ nfa_complete.add_state() };
    nfa_complete.final.insert(sink_state);
    const State new_initial{ nfa_complete.add_state() };
    nfa_complete.initial.insert(new_initial);
    auto subset_map_it{ subset_map.emplace(initial, new_initial).first };
    worklist.emplace_back(subset_map_it.operator->());

    using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
    SynchronizedExistentialSymbolPostIterator synchronized_iterator{};

    const utils::OrdVector<Symbol> symbols{ get_symbols_to_work_with(*this, alphabet) };
    const auto symbols_end{ symbols.end() };
    bool continue_complementation{ true };
    while (continue_complementation && !worklist.empty()) {
        const auto curr_state_set_it{ worklist.back() };
        const State macrostate{ curr_state_set_it->second };
        const StateSet& curr_state_set{ curr_state_set_it->first };
        worklist.pop_back();

        synchronized_iterator.reset();
        for (const State orig_state: curr_state_set) { mata::utils::push_back(synchronized_iterator, delta[orig_state]); }
        bool sync_it_advanced{ synchronized_iterator.advance() };
        auto symbols_it{ symbols.begin() };
        while (sync_it_advanced || symbols_it != symbols_end) {
            if (!sync_it_advanced) {
                assert(symbols_it != symbols_end);
                // There are no more transitions from the 'orig_states' but there is a symbol from the 'symbols'. Make
                //  the complemented NFA complete by adding a transition to a sink state. We can now return the access
                //  word for the sink state.
                nfa_complete.delta.add(macrostate, *symbols_it, sink_state);
                continue_complementation = false;
                break;
            }
            assert(sync_it_advanced);
            const std::vector<Iterator>& orig_symbol_posts{ synchronized_iterator.get_current() };
            const Symbol symbol_advanced_to{ (*orig_symbol_posts.begin())->symbol };
            StateSet orig_targets{ synchronized_iterator.unify_targets() };
            State target_macrostate;

            if (symbols_it == symbols_end || symbol_advanced_to <= *symbols_it) {
                // Continue with the determinization of the NFA.
                const auto target_macrostate_it = subset_map.find(orig_targets);
                if (target_macrostate_it != subset_map.end()) {
                    target_macrostate = target_macrostate_it->second;
                } else {
                    target_macrostate = nfa_complete.add_state();
                    if (!final.intersects_with(orig_targets)) {
                        nfa_complete.final.insert(target_macrostate);
                        continue_complementation = false;
                    }
                    subset_map_it = subset_map.emplace(std::move(orig_targets), target_macrostate).first;
                    worklist.emplace_back(subset_map_it.operator->());
                }
                nfa_complete.delta.add(macrostate, symbol_advanced_to, target_macrostate);
            } else {
                assert(symbol_advanced_to > *symbols_it);
                // There are more transitions from the 'orig_states', but there is a missing transition over
                //  '*symbols_it'. Make the complemented NFA complete by adding a transition to a sink state. We can now
                //  return the access word for the sink state.
                nfa_complete.delta.add(macrostate, *symbols_it, sink_state);
                continue_complementation = false;
                break;
            }

            if (!continue_complementation) { break; }
            if(symbol_advanced_to >= *symbols_it) { ++symbols_it; }
            sync_it_advanced = synchronized_iterator.advance();
        }
    }
    return nfa_complete.get_word();
}

Nfa mata::nfa::lang_difference(
    const Nfa& nfa_included, const Nfa& nfa_excluded,
    std::optional<
        std::function<bool(const Nfa&, const Nfa&, const StateSet&, const StateSet&, const State, const Nfa&)>
    > macrostate_discover
) {
    std::unordered_set<StateSet> subset_set_included{};
    std::unordered_set<StateSet> subset_set_excluded{};
    using SubsetMacrostateMap = std::unordered_map<std::pair<
        std::unordered_set<StateSet>::const_pointer,
        std::unordered_set<StateSet>::const_pointer
    >, State>;
    SubsetMacrostateMap subset_macrostate_map{};
    std::vector<SubsetMacrostateMap::const_pointer> worklist{};

    // '{}' represents a sink state that is always final in the complement.
    const auto subset_set_excluded_sink_ptr{ subset_set_excluded.insert(StateSet{}).first.operator->() };

    Nfa nfa_lang_difference{};
    const State new_initial{ nfa_lang_difference.add_state() };
    nfa_lang_difference.initial.insert(new_initial);
    if (nfa_included.final.intersects_with(nfa_included.initial) &&
        !nfa_excluded.final.intersects_with(nfa_excluded.initial)) {
        nfa_lang_difference.final.insert(new_initial);
    }
    auto subset_set_included_ptr{
        subset_set_included.emplace(nfa_included.initial).first.operator->() };
    auto subset_set_excluded_ptr{
        subset_set_excluded.emplace(nfa_excluded.initial).first.operator->() };
    auto subset_macrostate_map_ptr{
        subset_macrostate_map.emplace(
            std::make_pair(subset_set_included_ptr, subset_set_excluded_ptr), new_initial).first.operator->() };
    worklist.emplace_back(subset_macrostate_map_ptr);
    if (macrostate_discover.has_value()
        && !(*macrostate_discover)(
            nfa_included, nfa_excluded,
            *subset_set_included_ptr, *subset_set_excluded_ptr,
            new_initial, nfa_lang_difference)
    ) { return nfa_lang_difference; }

    using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
    SynchronizedExistentialSymbolPostIterator synchronized_iterator_included{};
    SynchronizedExistentialSymbolPostIterator synchronized_iterator_excluded{};

    while (!worklist.empty()) {
        const auto curr_macrostate_ptr{ worklist.back() };
        worklist.pop_back();
        const auto curr_state_set_included_ptr{ curr_macrostate_ptr->first.first };
        const auto curr_state_set_excluded_ptr{ curr_macrostate_ptr->first.second };
        const StateSet& curr_state_set_included{ *curr_state_set_included_ptr };
        const StateSet& curr_state_set_excluded{ *curr_state_set_excluded_ptr };
        const State macrostate{ curr_macrostate_ptr->second };

        synchronized_iterator_included.reset();
        synchronized_iterator_excluded.reset();
        for (const State orig_state: curr_state_set_included) {
            mata::utils::push_back(synchronized_iterator_included, nfa_included.delta[orig_state]);
        }
        for (const State orig_state: curr_state_set_excluded) {
            mata::utils::push_back(synchronized_iterator_excluded, nfa_excluded.delta[orig_state]);
        }
        bool sync_it_included_advanced{ synchronized_iterator_included.advance() };
        bool sync_it_excluded_advanced{ false };
        while (sync_it_included_advanced) {
            const std::vector<Iterator>& orig_symbol_posts{ synchronized_iterator_included.get_current() };
            const Symbol symbol_advanced_to{ (*orig_symbol_posts.begin())->symbol };
            StateSet orig_targets_included{ synchronized_iterator_included.unify_targets() };
            sync_it_excluded_advanced = synchronized_iterator_excluded.synchronize_with(symbol_advanced_to);
            StateSet orig_targets_excluded{
                sync_it_excluded_advanced ? synchronized_iterator_excluded.unify_targets() : StateSet{}
            };
            const bool final_included_intersects_targets{ nfa_included.final.intersects_with(orig_targets_included) };
            const bool final_excluded_intersects_targets{ nfa_excluded.final.intersects_with(orig_targets_excluded) };
            subset_set_included_ptr = subset_set_included.insert(std::move(orig_targets_included)).first.operator->();
            subset_set_excluded_ptr = subset_set_excluded.insert(std::move(orig_targets_excluded)).first.operator->();
            const auto [target_macrostate_it, macrostate_inserted]{ subset_macrostate_map.emplace(
                std::make_pair(subset_set_included_ptr, subset_set_excluded_ptr), nfa_lang_difference.num_of_states()
            ) };
            subset_macrostate_map_ptr = target_macrostate_it.operator->();
            const State target_macrostate{ target_macrostate_it->second };
            nfa_lang_difference.delta.add(macrostate, symbol_advanced_to, target_macrostate);
            if (macrostate_inserted) {
                // 'sync_it_excluded_advanced' is true iff there is a transition in the excluded NFA over the symbol
                //  'symbol_advanced_to'. If sync_it_excluded_advanced == false, the complement of the excluded NFA will
                //  have a transition over 'symbol_advanced_to' to a "sink state" which is a final state in the
                //  complement, and therefore must always be final in the language difference.
                if (final_included_intersects_targets) {
                    if (subset_set_excluded_ptr == subset_set_excluded_sink_ptr
                        || (sync_it_excluded_advanced && !final_excluded_intersects_targets)) {
                        nfa_lang_difference.final.insert(target_macrostate);
                    }
                }
                if (macrostate_discover.has_value()
                    && !(*macrostate_discover)(
                        nfa_included, nfa_excluded,
                        *subset_set_included_ptr, *subset_set_excluded_ptr,
                        target_macrostate, nfa_lang_difference)) { return nfa_lang_difference; }

                worklist.emplace_back(subset_macrostate_map_ptr);
            }
            sync_it_included_advanced = synchronized_iterator_included.advance();
        }
    }
    return nfa_lang_difference;
}

std::optional<mata::Word> mata::nfa::get_word_from_lang_difference(const Nfa & nfa_included, const Nfa & nfa_excluded) {
    return lang_difference(nfa_included, nfa_excluded,
        [&](const Nfa& nfa_included, const Nfa& nfa_excluded,
            const StateSet& macrostate_included_state_set, const StateSet& macrostate_excluded_state_set,
            const State macrostate, const Nfa& nfa_lang_difference) {
            (void)nfa_included, (void)nfa_excluded;
            (void)macrostate_included_state_set, (void)macrostate_excluded_state_set;
            (void)macrostate;
            return nfa_lang_difference.final.empty();
        }).get_word();
}
