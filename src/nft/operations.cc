/* nft.cc -- operations for NFT
 */

#include <algorithm>
#include <list>
#include <unordered_set>
#include <iterator>

// MATA headers
#include "mata/nft/delta.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include <mata/simlib/explicit_lts.hh>

using std::tie;

using namespace mata::utils;
using namespace mata::nft;
using mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

namespace {
    Simlib::Util::BinaryRelation compute_fw_direct_simulation(const Nft& aut) {
        Symbol maxSymbol{ aut.delta.get_max_symbol() };
        const size_t state_num{ aut.num_of_states() };
        Simlib::ExplicitLTS LTSforSimulation(state_num);

        for (const Transition& transition : aut.delta.transitions()) {
            LTSforSimulation.add_transition(transition.source, transition.symbol, transition.target);
        }

        // final states cannot be simulated by nonfinal -> we add new selfloops over final states with new symbol in LTS
        for (State finalState : aut.final) {
            LTSforSimulation.add_transition(finalState, maxSymbol + 1, finalState);
        }

        LTSforSimulation.init();
        return LTSforSimulation.compute_simulation();
    }

    Nft reduce_size_by_simulation(const Nft& aut, StateRenaming &state_renaming) {
        Nft result;
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

//TODO: based on the comments inside, this function needs to be rewritten in a more optimal way.
Nft mata::nft::remove_epsilon(const Nft& aut, Symbol epsilon) {
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
    Nft result{ Delta{}, aut.initial, aut.final, aut.levels, aut.levels_cnt, aut.alphabet };
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

Nft mata::nft::project_out(const Nft& aut, const utils::OrdVector<Level>& levels_to_proj) {
    assert(!levels_to_proj.empty());
    assert(*std::max_element(levels_to_proj.begin(), levels_to_proj.end()) < aut.levels_cnt);

    // Checks if a given state is being projected out based on the levels_to_proj vector.
    auto is_projected_out = [&](State s) {
        return levels_to_proj.find(aut.levels[s]) != levels_to_proj.end();
    };

    // Checks if each level between given states is being projected out.
    auto is_projected_along_path = [&](State a, State b) {
        Level stop_lvl = (aut.levels[b] == 0) ? aut.levels_cnt : aut.levels[b];
        for (Level lvl{ aut.levels[a] }; lvl < stop_lvl; lvl++) {
            if (levels_to_proj.find(lvl) == levels_to_proj.end()) {
                return false;
            }
        }
        return true;
    };

    // Determines the transition length between two states based on their levels.
    auto get_trans_len = [&](State src, State tgt) {
        return (aut.levels[src] == 0) ? (aut.levels_cnt - aut.levels[src]) : (aut.levels[tgt] - aut.levels[src]);
    };

    // Returns one-state automaton it the case of projecting all levels.
    if (aut.levels_cnt == levels_to_proj.size()) {
        if (aut.is_lang_empty()) {
            return Nft(1, {0}, {}, {}, 0);
        }
        return Nft(1, {0}, {0}, {}, 0);
    }

    // Calculates the smallest level 0 < k < levels_cnt that starts a consecutive ascending sequence
    // of levels k, k+1, k+2, ..., levels_cnt-1 in the ordered-vector levels_to_proj.
    // If there is no such sequence, then k == levels_cnt.
    size_t seq_start_idx = aut.levels_cnt;
    const std::vector<Level> levels_to_proj_v = levels_to_proj.ToVector();
    for (auto levels_to_proj_v_revit = levels_to_proj_v.rbegin();
         levels_to_proj_v_revit != levels_to_proj_v.rend() && *levels_to_proj_v_revit == seq_start_idx - 1;
         ++levels_to_proj_v_revit, --seq_start_idx);

    // Only states whose level is part of the sequence (will have level 0) can additionally be marked as final.
    auto can_be_final = [&](State s) {
        return seq_start_idx <= aut.levels[s];
    };

    // Builds a vector of size levels_cnt. Each index k contains a new level for level k.
    // Sets levels to 0 starting from seq_start_idx.
    // Example:
    // old levels    0 1 2 3 4 5 6
    // project out     x   x   x x
    // new levels    0 0 1 2 2 0 0
    std::vector<Level> new_levels(aut.levels_cnt , 0);
    Level lvl_sub{ 0 };
    for (Level lvl_old{ 0 }; lvl_old < seq_start_idx; lvl_old++) {
        new_levels[lvl_old] = static_cast<Level>(lvl_old - lvl_sub);
        if (is_projected_out(lvl_old))
            lvl_sub++;
    }

    // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
    const size_t num_of_states_in_delta{ aut.delta.num_of_states() };
    std::vector<StateSet> closure = std::vector<StateSet>(num_of_states_in_delta, OrdVector<State>());;

    // TODO: Evaluate efficiency. This might not be as inefficient as the remove_epsilon closure.
    // Begin by initializing the closure.
    for (State source{ 0 }; source < num_of_states_in_delta; ++source)
    {
        closure[source].insert(source);
        if (!is_projected_out(source)) {
            continue;
        }
        for (const auto& trans: aut.delta[source])
        {
            for (const auto& target : trans.targets) {
                if (is_projected_along_path(source, target)) {
                    closure[source].insert(target);
                }
            }
        }
    }

    // We will focus only on those states that will be affected by projection.
    std::vector<State> states_to_project;
    for (State s{ 0 }; s < num_of_states_in_delta; s++) {
        if (closure[s].size() > 1) {
            states_to_project.push_back(s);
        }
    }

    // Compute transitive closure.
    bool changed = true;
    while (changed) { // Compute the fixpoint.
        changed = false;
        for (const State s : states_to_project) {
            for (const State cls_state : closure[s]) {
                if (!closure[cls_state].IsSubsetOf(closure[s])) {
                    closure[s].insert(closure[cls_state]);
                    changed = true;
                }
            }
        }
    }

    // Construct the automaton with projected levels.
    Nft result{ Delta{}, aut.initial, aut.final, aut.levels, aut.levels_cnt, aut.alphabet };
    for (State src_state{ 0 }; src_state < num_of_states_in_delta; src_state++) { // For every state.
        for (State cls_state : closure[src_state]) { // For every state in its epsilon closure.
            if (aut.final[cls_state] && can_be_final(src_state)) result.final.insert(src_state);
            for (const SymbolPost& move : aut.delta[cls_state]) {
                // TODO: this could be done more efficiently if we had a better add method
                for (State tgt_state : move.targets) {
                    bool is_loop_on_target = cls_state == tgt_state;
                    if (is_projected_along_path(cls_state, tgt_state)) continue;
                    if (is_projected_out(cls_state) && get_trans_len(cls_state, tgt_state) == 1 && !is_loop_on_target) continue;

                    if (is_projected_out(cls_state)) {
                        // If there are remaining levels (testing only DONT_CARE) between cls_state and tgt_state
                        // on a transition with a length greater than 1, then these levels must be preserved.
                        result.delta.add(src_state, DONT_CARE, tgt_state);
                    } else if (is_loop_on_target) {
                        // Instead of creating a transition to tgt_state and
                        // then a self-loop, establish the self-loop directly on src_state.
                        result.delta.add(src_state, move.symbol, src_state);
                    } else {
                        result.delta.add(src_state, move.symbol, tgt_state);
                    }
                }
            }
        }
    }
    // TODO(nft): Sometimes, unreachable states are left in the automaton.
    // These states typically contain projected levels with a self-loop.
    result = result.trim();

    // Repare levels
    for (State s{ 0 }; s < result.levels.size(); s++) {
        result.levels[s] = new_levels[result.levels[s]];
    }
    result.levels_cnt = static_cast<Level>(result.levels_cnt - levels_to_proj.size());

    return result;
}

Nft mata::nft::project_out(const Nft& aut, const Level level_to_project) {
    return project_out(aut, utils::OrdVector{ level_to_project });
}

Nft mata::nft::fragile_revert(const Nft& aut) {
    const size_t num_of_states{ aut.num_of_states() };

    Nft result(num_of_states);

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

Nft mata::nft::simple_revert(const Nft& aut) {
    Nft result;
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
Nft mata::nft::somewhat_simple_revert(const Nft& aut) {
    const size_t num_of_states{ aut.num_of_states() };

    Nft result(num_of_states);

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

Nft mata::nft::revert(const Nft& aut) {
    return simple_revert(aut);
    //return fragile_revert(aut);
    //return somewhat_simple_revert(aut);
}

std::pair<Run, bool> mata::nft::Nft::get_word_for_path(const Run& run) const {
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
bool mata::nft::Nft::is_in_lang(const Run& run) const {
    StateSet current_post(this->initial);
    for (const Symbol sym : run.word) {
        current_post = this->post(current_post, sym);
        if (current_post.empty()) { return false; }
    }
    return this->final.intersects_with(current_post);
}

/// Checks whether the prefix of a string is in the language of an automaton
// TODO: slow and it should share code with is_in_lang
bool mata::nft::Nft::is_prfx_in_lang(const Run& run) const {
    StateSet current_post{ this->initial };
    for (const Symbol sym : run.word) {
        if (this->final.intersects_with(current_post)) { return true; }
        current_post = this->post(current_post, sym);
        if (current_post.empty()) { return false; }
    }
    return this->final.intersects_with(current_post);
}

Nft mata::nft::algorithms::minimize_brzozowski(const Nft& aut) {
    //compute the minimal deterministic automaton, Brzozovski algorithm
    return determinize(revert(determinize(revert(aut))));
}

Nft mata::nft::minimize(
                const Nft& aut,
                const ParameterMap& params)
{
    Nft result;
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

Nft mata::nft::uni(const Nft &lhs, const Nft &rhs) {
    Nft union_nft{ lhs };
    return union_nft.uni(rhs);
}

Nft& Nft::uni(const Nft& aut) {
    size_t n = this->num_of_states();
    auto upd_fnc = [&](State st) {
        return st + n;
    };

    // copy the information about aut to save the case when this is the same object as aut.
    size_t aut_states = aut.num_of_states();
    SparseSet<mata::nft::State> aut_final_copy = aut.final;
    SparseSet<mata::nft::State> aut_initial_copy = aut.initial;

    this->delta.allocate(n);
    this->delta.append(aut.delta.renumber_targets(upd_fnc));

    // set accepting states
    this->final.reserve(n+aut_states);
    for(const State& aut_fin : aut_final_copy) {
        this->final.insert(upd_fnc(aut_fin));
    }
    // set unitial states
    this->initial.reserve(n+aut_states);
    for(const State& aut_ini : aut_initial_copy) {
        this->initial.insert(upd_fnc(aut_ini));
    }

    return *this;
}

Simlib::Util::BinaryRelation mata::nft::algorithms::compute_relation(const Nft& aut, const ParameterMap& params) {
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
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"relation\" key: " + relation);
    }
}

Nft mata::nft::reduce(const Nft &aut, StateRenaming *state_renaming, const ParameterMap& params) {
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algorithm\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    Nft result;
    std::unordered_map<State,State> reduced_state_map;
    const std::string& algorithm = params.at("algorithm");
    if ("simulation" == algorithm) {
        result = reduce_size_by_simulation(aut, reduced_state_map);
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

Nft mata::nft::determinize(
        const Nft&  aut,
        std::unordered_map<StateSet, State> *subset_map) {

    Nft result;
    //assuming all sets targets are non-empty
    std::vector<std::pair<State, StateSet>> worklist;
    bool deallocate_subset_map = false;
    if (subset_map == nullptr) {
        subset_map = new std::unordered_map<StateSet,State>();
        deallocate_subset_map = true;
    }

    result.clear();

    const StateSet S0 =  StateSet(aut.initial);
    const State S0id = result.add_state();
    result.initial.insert(S0id);

    if (aut.final.intersects_with(S0)) {
        result.final.insert(S0id);
    }
    worklist.emplace_back(S0id, S0);

    (*subset_map)[mata::utils::OrdVector<State>(S0)] = S0id;

    if (aut.delta.empty())
        return result;

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
        // TODO: shouldn't we also reset first?
        for (State q: S) {
            mata::utils::push_back(synchronized_iterator, aut.delta[q]);
        }

        while (synchronized_iterator.advance()) {

            // extract post from the sychronized_iterator iterator
            const std::vector<Iterator>& moves = synchronized_iterator.get_current();
            Symbol currentSymbol = (*moves.begin())->symbol;
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
        }
    }

    if (deallocate_subset_map) { delete subset_map; }

    return result;
}

std::ostream& std::operator<<(std::ostream& os, const Nft& nft) {
    nft.print_to_mata(os);
    return os;
}

Run mata::nft::encode_word(const Alphabet* alphabet, const std::vector<std::string>& input) {
    return mata::nfa::encode_word(alphabet, input);
}

std::set<mata::Word> mata::nft::Nft::get_words(unsigned max_length) {
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
                    new_worklist.push_back({s_to, new_word});
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
