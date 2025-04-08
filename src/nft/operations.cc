/* nft.cc -- operations for NFT
 */

#include <algorithm>
#include <list>
#include <unordered_set>
#include <iterator>
#include <algorithm>

// MATA headers
#include "mata/nft/delta.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/strings.hh"
#include <mata/simlib/explicit_lts.hh>

using std::tie;

using namespace mata::utils;
using namespace mata::nft;
using namespace mata::nfa;
using namespace mata;
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

        // similarly, states on different levels cannot be simulated by each other, we add self loops over the same fresh symbol for each state of the same level
        for (State state = 0; state < state_num; ++state) {
            LTSforSimulation.add_transition(state, maxSymbol + 2 + aut.levels[state], state);
        }

        LTSforSimulation.init();
        return LTSforSimulation.compute_simulation();
    }

    Nft reduce_size_by_simulation(const Nft& aut, StateRenaming &state_renaming) {
        Nft result;
        result.num_of_levels = aut.num_of_levels;
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
                assert(aut.levels[q] == aut.levels[qReprState]);
                const State qClass = result.add_state_with_level(aut.levels[qReprState]);
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

Nft mata::nft::remove_epsilon(const Nft& aut, Symbol epsilon) {
    const size_t num_of_states{aut.num_of_states() };
    mata::nfa::Nfa reversed_nfa{ mata::nfa::revert(aut) };

    // this vector will collect epsilon run from level 0 state to level 0 state
    // that contains only epsilon transitions, and all states inbetween (i.e.
    // not-level-0 states) will have only one epsilon transition going to it
    // and one epsilon transition going from it
    std::vector<std::vector<State>> safe_epsilon_runs;

    // for each level 0 state q, eps_delta[q] represents all states to which
    // we can get to by some safe epsilon run
    std::vector<StateSet> eps_delta(num_of_states);
    // its inverse
    std::vector<StateSet> eps_delta_inverse(num_of_states);

    for (State state = 0; state != num_of_states; ++state) {
        if (aut.levels[state] == DEFAULT_LEVEL) {
            std::vector<std::vector<State>> state_safe_epsilon_runs{ {state} };
            for (Level cur_level = 0; cur_level < aut.num_of_levels; ++cur_level) {
                std::vector<std::vector<State>> new_state_safe_epsilon_runs;
                for (const std::vector<State>& safe_epsilon_run : state_safe_epsilon_runs) {
                    State state_s = safe_epsilon_run.back();
                    const StatePost& post_s{ aut.delta[state_s] };
                    const auto eps_move_it_s { post_s.find(epsilon) };
                    if (eps_move_it_s != post_s.end()) {
                        if (cur_level != DEFAULT_LEVEL && aut.delta[state_s].size() != 1) {
                            // for levels > DEFAULT_LEVEL we do not want to have transitions that are not with empty symbol
                            continue;
                        }
                        for (State target : eps_move_it_s->targets) {
                            if (cur_level == aut.num_of_levels-1) {
                                // we are at the last level, next level will be 0
                                assert(aut.levels[target] == DEFAULT_LEVEL);
                                std::vector<State> new_safe_epsilon_run = safe_epsilon_run;
                                new_safe_epsilon_run.push_back(target);
                                // we finish with generating this safe epsilon run and push it to safe_epsilon_runs directly
                                safe_epsilon_runs.push_back(new_safe_epsilon_run);
                                eps_delta[state].insert(target);
                                eps_delta_inverse[target].insert(state);
                            } else if (reversed_nfa.delta[target].size() == 1) {
                                // we are not at the last level, next state must be level incremented by one (assuming no jumps)
                                assert(aut.levels[target] == cur_level + 1);
                                std::vector<State> new_safe_epsilon_run = safe_epsilon_run;
                                new_safe_epsilon_run.push_back(target);
                                // this safe epsilon run is not finished yet, we save new_state_safe_epsilon_runs to return to it
                                new_state_safe_epsilon_runs.push_back(new_safe_epsilon_run);
                            }
                        }
                    }
                }
                if (new_state_safe_epsilon_runs.empty()) {
                    break;
                } else {
                    state_safe_epsilon_runs = new_state_safe_epsilon_runs;
                }
            }
        }
    }

    // compute the transition closure of the epsilon delta
    // by using simplified Floyd-Warshall algorithm, see
    // https://stackoverflow.com/a/76173280
    for (State state{ 0 }; state < num_of_states; ++state) {
        // for every pair of transitions
        //       before_state -epsilon-> state -epsilon-> after_state
        // we add transition
        //       before_state -epsilon-> after_state
        const StateSet& after_states = eps_delta[state];
        const StateSet& before_states = eps_delta_inverse[state];

        if (after_states.empty() || before_states.empty()) {
            // there is no such pair of transitions
            continue;
        }

        for (State before_state : before_states) {
            if (before_state != state) {
                eps_delta[before_state].insert(after_states);
            }
        }
        for (State after_state : after_states) {
            if (after_state != state) {
                eps_delta_inverse[after_state].insert(before_states);
            }
        }
    }

    // At this point eps_delta represents epsilon closure of level 0 states, but it might not be reflexive

    // Construct the automaton without epsilon transitions.
    Nft result{ aut };
    result.delta.allocate(num_of_states); // just to be safe

    // we first remove all epsilon transitions
    std::set<Transition> safe_epsilon_runs_transitions;
    for (const auto& safe_epsilon_run : safe_epsilon_runs) {
        for (size_t i = 0; i < safe_epsilon_run.size()-1; ++i) {
            Transition safe_epsilon_run_transition{ safe_epsilon_run[i], epsilon, safe_epsilon_run[i+1] };
            if (!safe_epsilon_runs_transitions.contains(safe_epsilon_run_transition)) {
                result.delta.remove(safe_epsilon_run_transition);
                safe_epsilon_runs_transitions.insert(safe_epsilon_run_transition);
            }
        }
    }

    // we add new transitions using epsilon closure
    for (State state{ 0 }; state < num_of_states; ++state) {
        for (State eps_cl_state : eps_delta[state]) { // For every state in its epsilon closure.
            if (eps_cl_state != state) { // transitions from state are already in result
                if (aut.final[eps_cl_state]) { result.final.insert(state); }
                // we only need to add the first transition to level 1 state
                for (const SymbolPost& move : result.delta[eps_cl_state]) {
                    result.delta.add(state, move.symbol, move.targets);
                }
            }
        }
    }
    return result;
}

Nft mata::nft::project_out(const Nft& nft, const utils::OrdVector<Level>& levels_to_project, const JumpMode jump_mode) {
    assert(!levels_to_project.empty());
    assert(*std::max_element(levels_to_project.begin(), levels_to_project.end()) < nft.num_of_levels);

    // Checks if a given state is being projected out based on the levels_to_project vector.
    auto is_projected_out = [&](State s) {
        return levels_to_project.find(nft.levels[s]) != levels_to_project.end();
    };

    // Checks if each level between given states is being projected out.
    auto is_projected_along_path = [&](State src, State tgt) {
        Level stop_lvl = (nft.levels[tgt] == 0) ? static_cast<Level>(nft.num_of_levels) : nft.levels[tgt];
        for (Level lvl{ nft.levels[src] }; lvl < stop_lvl; lvl++) {
            if (levels_to_project.find(lvl) == levels_to_project.end()) {
                return false;
            }
        }
        return true;
    };

    // Determines the transition length between two states based on their levels.
    auto get_trans_len = [&](State src, State tgt) {
        return (nft.levels[src] == 0) ? (nft.num_of_levels - nft.levels[src]) : (nft.levels[tgt] - nft.levels[src]);
    };

    // Returns one-state automaton it the case of projecting all levels.
    if (nft.num_of_levels == levels_to_project.size()) {
        if (nft.is_lang_empty()) {
            return Nft(1, {0}, {}, {}, 0);
        }
        return Nft(1, {0}, {0}, {}, 0);
    }

    // Calculates the smallest level 0 < k < num_of_levels that starts a consecutive ascending sequence
    // of levels k, k+1, k+2, ..., num_of_levels-1 in the ordered-vector levels_to_project.
    // If there is no such sequence, then k == num_of_levels.
    size_t seq_start_idx = nft.num_of_levels;
    const std::vector<Level> levels_to_proj_v = levels_to_project.to_vector();
    for (auto levels_to_proj_v_revit = levels_to_proj_v.rbegin();
         levels_to_proj_v_revit != levels_to_proj_v.rend() && *levels_to_proj_v_revit == seq_start_idx - 1;
         ++levels_to_proj_v_revit, --seq_start_idx);

    // Only states whose level is part of the sequence (will have level 0) can additionally be marked as final.
    auto can_be_final = [&](State s) {
        return seq_start_idx <= nft.levels[s];
    };

    // Builds a vector of size num_of_levels. Each index k contains a new level for level k.
    // Sets levels to 0 starting from seq_start_idx.
    // Example:
    // old levels    0 1 2 3 4 5 6
    // project out     x   x   x x
    // new levels    0 0 1 2 2 0 0
    std::vector<Level> new_levels(nft.num_of_levels , 0);
    Level lvl_sub{ 0 };
    for (Level lvl_old{ 0 }; lvl_old < seq_start_idx; lvl_old++) {
        new_levels[lvl_old] = static_cast<Level>(lvl_old - lvl_sub);
        if (levels_to_project.find(lvl_old) != levels_to_project.end()) {
            lvl_sub++;
        }
    }

    // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
    const size_t num_of_states_in_delta{ nft.delta.num_of_states() };
    std::vector<StateSet> closure = std::vector<StateSet>(num_of_states_in_delta, OrdVector<State>());;

    // TODO: Evaluate efficiency. This might not be as inefficient as the remove_epsilon closure.
    // Begin by initializing the closure.
    for (State source{ 0 }; source < num_of_states_in_delta; ++source)
    {
        closure[source].insert(source);
        if (!is_projected_out(source)) {
            continue;
        }
        for (const auto& trans: nft.delta[source])
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
                if (!closure[cls_state].is_subset_of(closure[s])) {
                    closure[s].insert(closure[cls_state]);
                    changed = true;
                }
            }
        }
    }

    // Construct the automaton with projected levels.
    Nft result{ Delta{}, nft.initial, nft.final, nft.levels, nft.num_of_levels, nft.alphabet };
    for (State src_state{ 0 }; src_state < num_of_states_in_delta; src_state++) { // For every state.
        for (State cls_state : closure[src_state]) { // For every state in its epsilon closure.
            if (nft.final[cls_state] && can_be_final(src_state)) result.final.insert(src_state);
            for (const SymbolPost& move : nft.delta[cls_state]) {
                // TODO: this could be done more efficiently if we had a better add method
                for (State tgt_state : move.targets) {
                    bool is_loop_on_target = cls_state == tgt_state;
                    if (is_projected_along_path(cls_state, tgt_state)) continue;
                    if (is_projected_out(cls_state) && get_trans_len(cls_state, tgt_state) == 1 && !is_loop_on_target) continue;

                    if (is_projected_out(cls_state)) {
                        // If there are remaining levels between cls_state and tgt_state
                        // on a transition with a length greater than 1, then these levels must be preserved.
                        if (jump_mode == JumpMode::RepeatSymbol) {
                            result.delta.add(src_state, move.symbol, tgt_state);
                        } else {
                            result.delta.add(src_state, DONT_CARE, tgt_state);
                        }
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
    result.num_of_levels = static_cast<Level>(result.num_of_levels - levels_to_project.size());

    return result;
}

Nft mata::nft::project_out(const Nft& nft, const Level level_to_project, const JumpMode jump_mode) {
    return project_out(nft, utils::OrdVector{ level_to_project }, jump_mode);
}

Nft mata::nft::project_to(const Nft& nft, const OrdVector<Level>& levels_to_project, const JumpMode jump_mode) {
    OrdVector<Level> all_levels{ OrdVector<Level>::with_reserved(nft.num_of_levels) };
    for (Level level{ 0 }; level < nft.num_of_levels; ++level) { all_levels.push_back(level); }
    OrdVector<Level> levels_to_project_out{ OrdVector<Level>::with_reserved(nft.num_of_levels) };
    std::set_difference(all_levels.begin(), all_levels.end(), levels_to_project.begin(),
                        levels_to_project.end(), std::back_inserter(levels_to_project_out) );
    return project_out(nft, levels_to_project_out, jump_mode);
}

Nft mata::nft::project_to(const Nft& nft, Level level_to_project, const JumpMode jump_mode) {
    return project_to(nft, OrdVector<Level>{ level_to_project }, jump_mode);
}

Nft Nft::apply(const nfa::Nfa& nfa, Level level_to_apply_on, bool project_out_applied_level, JumpMode jump_mode) const {
    Nft nft_from_nfa{ nfa };
    return compose(nft_from_nfa, *this, 0, level_to_apply_on, project_out_applied_level, jump_mode);
}

Nft Nft::apply(const Word& word, Level level_to_apply_on, bool project_out_applied_level, JumpMode jump_mode) const {
    return apply(nfa::builder::create_single_word_nfa(word), level_to_apply_on, project_out_applied_level, jump_mode);
}

Nft mata::nft::insert_levels(const Nft& nft, const BoolVector& new_levels_mask, const JumpMode jump_mode) {
    assert(0 < nft.num_of_levels);
    assert(nft.num_of_levels <= new_levels_mask.size());
    assert(static_cast<size_t>(std::count(new_levels_mask.begin(), new_levels_mask.end(), false)) == nft.num_of_levels);

    if (nft.num_of_levels == new_levels_mask.size()) {
         return { nft };
    }

    // Construct a vector of size equal to num_of_levels. Each index k in this vector represents a new level for the original level k.
    // Note: The 0th index always remains zero.
    // Here are a couple of examples to illustrate this:
    // Example 1:
    //   Input (levels)      : 0 1 2
    //   Mask                : 0 0 1 1 0 1
    //   Output (new levels) : 0 1 4
    // Example 2:
    //   Input (levels)      : 0 1 2
    //   Mask                : 1 1 0 1 1 0 0
    //   Output (new levels) : 0 3 6
    std::vector<Level> updated_levels(nft.num_of_levels, 0);
    Level old_lvl = 0;
    Level new_lvl = 0;
    auto mask_it = std::find(new_levels_mask.begin(), new_levels_mask.end(), false);
    if (new_levels_mask[0]) {
        old_lvl = 1;
        new_lvl = static_cast<Level>(std::distance(new_levels_mask.begin(), mask_it) + 1);
        ++mask_it;
    }
    for (; mask_it != new_levels_mask.end(); ++mask_it, ++new_lvl) {
        if (!*mask_it) {
            updated_levels[old_lvl++] = new_lvl;
        }
    }
    // Repare state levels
    std::vector<Level> new_state_levels(nft.levels.size());
    for (State s{ 0 }; s < nft.levels.size(); s++) {
        new_state_levels[s] = updated_levels[nft.levels[s]];
    }

    // Construct vector of next inner levels for usable inner states.
    // This allows us to create only the important inner states, rather than all of them.
    // Example:
    //  Input (new_levels_mask):               0 1 1 0 1 0 1 1  0  1  1  1
    //  Output (JumpMode::RepeatSymbol):       1 3 3 4 5 6 8 8  9 12 12 12
    //  Output (JumpMode::AppendDontCares):    3 3 3 5 5 8 8 8 12 12 12 12
    const size_t mask_size = new_levels_mask.size();
    std::vector<Level> next_inner_levels(mask_size);
    Level next_level = static_cast<Level>(mask_size);
    size_t i = mask_size - 1;
    for (auto it = new_levels_mask.rbegin(); it != new_levels_mask.rend(); ++it, --i) {
        next_inner_levels[i] = next_level;
        if (!*it) {
            if (jump_mode == JumpMode::RepeatSymbol) {
                next_inner_levels[i] = static_cast<Level>(i+1);
            }
            next_level = static_cast<Level>(i);
        }
    }

    // Construct an empty automaton with updated levels.
    Nft result(Delta(nft.num_of_states()), nft.initial, nft.final, new_state_levels, static_cast<unsigned int>(new_levels_mask.size()), nft.alphabet);

    // Function to create a transition between source and target states.
    // The transition symbol is determined based on the parameters:
    // it could be a specific symb, DONT_CARE, or a default symbol.
    auto create_transition = [&](const State src, const Symbol symb, const State tgt, const bool is_inserted_level, const bool is_old_level_processed) {
        if (!is_inserted_level && (jump_mode == JumpMode::RepeatSymbol || !is_old_level_processed)) {
            result.delta.add(src, symb, tgt);
        } else {
            result.delta.add(src, DONT_CARE, tgt);
        }
    };

    std::vector<std::vector<State>> state_level_matrix(nft.num_of_states(), std::vector<State>());
    // Creates an inner state for a given source state and inner level.
    // If the inner state already exists, then it is reused.
    auto get_inner_state = [&](const State src, const Level inner_level, const bool is_inserted_level, const bool is_old_level_processed) {
        if (!is_old_level_processed && is_inserted_level) {
            const size_t inner_state_idx = inner_level - result.levels[src] - 1;

            if (state_level_matrix[src].size() <= inner_state_idx) {
                state_level_matrix[src].resize((inner_state_idx + 1) * 2, Limits::max_state);
            }

            if (state_level_matrix[src][inner_state_idx] != Limits::max_state) {
                return state_level_matrix[src][inner_state_idx];
            }

            State inner_state = result.add_state_with_level(inner_level);
            state_level_matrix[src][inner_state_idx] = inner_state;
            return inner_state;

        }
        return result.add_state_with_level(inner_level);
    };

    //Construct delta with inserted levels and auxiliary states.
    for (const auto &trans : nft.delta.transitions()) {
        State src = trans.source;
        Level src_lvl = result.levels[trans.source];
        State inner;
        const Level stop_level = static_cast<Level>((result.levels[trans.target] == 0) ? (new_levels_mask.size() - 1) : (result.levels[trans.target] - 1));

        // Construct the first n-1 parts of the original transition.
        bool is_old_level_processed = false;
        while (next_inner_levels[src_lvl] < next_inner_levels[stop_level]) {
            inner = get_inner_state(trans.source, next_inner_levels[src_lvl], new_levels_mask[src_lvl], is_old_level_processed);
            create_transition(src, trans.symbol, inner, new_levels_mask[src_lvl], is_old_level_processed);
            if (!new_levels_mask[src_lvl]) {
                is_old_level_processed = true;
            }
            src = inner;
            src_lvl = result.levels[src];
            assert(src_lvl == result.levels[inner]);
        }
        // Construct the n-th part of the transition.
        create_transition(src, trans.symbol, trans.target, new_levels_mask[src_lvl], is_old_level_processed);
    }

    return result;
}

Nft mata::nft::insert_level(const Nft& nft, const Level new_level, const JumpMode jump_mode) {
    // TODO(nft): Optimize the insertion of just one level by using move.
    BoolVector new_levels_mask(nft.num_of_levels + 1, false);
    if (new_level < new_levels_mask.size()) {
        new_levels_mask[new_level] = true;
    } else {
        new_levels_mask[nft.num_of_levels] = true;
        new_levels_mask.resize(new_level + 1, true);
    }
    return insert_levels(nft, new_levels_mask, jump_mode);
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

Nft mata::nft::invert_levels(const Nft& aut, const JumpMode jump_mode) {
    const size_t num_of_states = aut.num_of_states();

    // Find states with level zero and rename them.
    std::vector<State> renaming(num_of_states, Limits::max_state);
    size_t num_of_zerostates = 0;
    for (State s = 0; s < num_of_states; ++s) {
        if (aut.levels[s] == 0) {
            renaming[s] = num_of_zerostates++;
        }
    }

    // Rename initial states
    SparseSet<State> new_initial;
    for (const State s: aut.initial) {
        new_initial.insert(renaming[s]);
    }
    // Rename final states
    SparseSet<State> new_final;
    for (const State s: aut.final) {
        new_final.insert(renaming[s]);
    }
    // Create new automaton
    Nft aut_inv = Nft(num_of_zerostates, std::move(new_initial), std::move(new_final), Levels(num_of_zerostates, 0), aut.num_of_levels, aut.alphabet);

    // Creates new states with inverted levels for each inner state in the path.
    auto create_states_with_inverted_levels = [&](const std::vector<State>& path_states) {
        for (const State s: path_states) {
            if (aut.levels[s] == 0) { continue; }
            renaming[s] = aut_inv.add_state_with_level(static_cast<Level>(aut.num_of_levels - aut.levels[s]));
        }
    };

    // Returns transitions of the path.
    auto get_path_transitions = [&](const std::vector<State>& path_states) {
        std::vector<Transition> path_transitions;
        for (size_t i = 0; i < path_states.size() - 1; ++i) {
            const State src = path_states[i];
            const State tgt = path_states[i + 1];
            std::vector<Transition> transitions_between = aut.delta.get_transitions_between(src, tgt);
            path_transitions.insert(path_transitions.end(), transitions_between.begin(), transitions_between.end());
        }

        return path_transitions;
    };

    // Creates inverted transitions for each transition in the path.
    // Can work with different jump modes.
    auto map_inverted_transitions = [&](const std::vector<Transition>& path, const State head, const State tail) {
        // Auxiliary state between two states can be reused for transitions over different symbols.
        // The key is a pair of source and target states.
        // auxiliary_states map will be used only if the jump_mode is JumpMode::AppendDontCares.
        std::unordered_map<std::pair<State, State>, State> auxiliary_states;
        auto get_aux_state = [&](const State src, const State tgt, const Level level) {
            const std::pair<State, State> key{ src, tgt };
            if (auxiliary_states.find(key) == auxiliary_states.end()) {
                auxiliary_states[key] = aut_inv.add_state_with_level(level);
            }
            return auxiliary_states[key];
        };
        for (const auto &[src, symbol, tgt]: path) {
            const bool is_src_head = src == head;
            const bool is_tgt_tail = tgt == tail;
            if (is_src_head && is_tgt_tail) {
                // It is a direct transition between two zero-states (the head and the tail).
                // Just copy it.
                if (jump_mode == JumpMode::AppendDontCares && aut.num_of_levels > 1) {
                    const State aux_state = get_aux_state(src, tgt, static_cast<Level>(aut.num_of_levels - 1));
                    aut_inv.delta.add(renaming[src], DONT_CARE, aux_state);
                    aut_inv.delta.add(aux_state, symbol, renaming[tgt]);
                } else {
                    aut_inv.delta.add(renaming[src], symbol, renaming[tgt]);
                }
            } else if (is_src_head) {
                // It is a transition from a zero-state (head) to a nonzero-state (inner state).
                // Map it as transition from that nonzero-state (inner state) to the tail (zero-states).
                if (jump_mode == JumpMode::AppendDontCares && aut.levels[tgt] > 1) {
                    const State aux_state = get_aux_state(src, tgt, static_cast<Level>(aut.num_of_levels - 1));
                    aut_inv.delta.add(renaming[tgt], DONT_CARE, aux_state);
                    aut_inv.delta.add(aux_state, symbol, renaming[tail]);
                } else {
                    aut_inv.delta.add(renaming[tgt], symbol, renaming[tail]);
                }

            } else if (is_tgt_tail) {
                // It is a transition from a nonzero-state (inner state) to a zero-state (tail).
                // Map it as transition from the zero-state (head) to that nonzero-state (inner state).
                if (jump_mode == JumpMode::AppendDontCares && (aut.num_of_levels - aut.levels[src]) > 1) {
                    const State aux_state = get_aux_state(src, tgt, static_cast<Level>(aut_inv.levels[renaming[src]] - 1));
                    aut_inv.delta.add(renaming[head], DONT_CARE, aux_state);
                    aut_inv.delta.add(aux_state, symbol, renaming[src]);
                } else {
                    aut_inv.delta.add(renaming[head], symbol, renaming[src]);
                }
            } else {
                // It is a transition between two nonzero-states (inner states).
                // Just swap the source and target.
                if (jump_mode == JumpMode::AppendDontCares && (aut.levels[tgt] - aut.levels[src]) > 1) {
                    const State aux_state = get_aux_state(src, tgt, static_cast<Level>(aut_inv.levels[renaming[src]] - 1));
                    aut_inv.delta.add(renaming[tgt], DONT_CARE, aux_state);
                    aut_inv.delta.add(aux_state, symbol, renaming[src]);
                } else {
                    aut_inv.delta.add(renaming[tgt], symbol, renaming[src]);
                }
            }
        }
    };

    // Main loop of the algorithm.
    for (State path_head = 0; path_head < num_of_states; ++path_head) {
        // Test if the state is a head of some path.
        if (aut.levels[path_head] != 0) {
            continue;   // Not a head.
        }
        // Process all paths using dfs.
        std::stack<std::pair<State, std::vector<State>>> stack;
        stack.push({ path_head, { path_head } });
        while (!stack.empty()) {
            auto [src, path] = stack.top();
            stack.pop();

            if (aut.levels[src] == 0 && path.size() > 1) {
                // A tail of the path (src) has been reached.
                create_states_with_inverted_levels(path);
                const std::vector<Transition> path_transitions = get_path_transitions(path);
                map_inverted_transitions(path_transitions, path.front(), path.back());
                continue;
            }

            for (const State tgt: aut.delta[src].get_successors()) {
                // Extend the path.
                std::vector<State> new_path = path;
                new_path.push_back(tgt);
                stack.push({ tgt, std::move(new_path) });
            }
        }
    }

    return aut_inv;
}

//TODO: Implement for NFT
bool mata::nft::Nft::is_in_lang(const Run& run, const bool use_epsilon, const bool match_prefix) const {
    std::cerr << "Warning: Nft::is_in_lang uses Nfa::is_in_lang, which is not designed for NFT's jump transitions" << std::endl;
    return nfa::Nfa::is_in_lang(run, use_epsilon, match_prefix);
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
            " requires setting the \"algorithm\" key in the \"params\" argument; "
            "received: " + std::to_string(params));
    }

    const std::string& str_algo = params.at("algorithm");
    if ("brzozowski" == str_algo) {  /* default */ }
    else {
        throw std::runtime_error(std::to_string(__func__) +
            " received an unknown value of the \"algorithm\" key: " + str_algo);
    }

    return algo(aut);
}

Nft mata::nft::union_nondet(const Nft &lhs, const Nft &rhs) {
    Nft union_nft{ lhs };
    return union_nft.unite_nondet_with(rhs);
}

Nft& Nft::unite_nondet_with(const Nft& aut) {
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

std::set<mata::Word> mata::nft::Nft::get_words(size_t max_length) const {
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

bool Nft::is_tuple_in_lang(const std::vector<Word>& track_words) {
    if (track_words.size() != num_of_levels) {
        throw std::runtime_error("Invalid number of tracks. Expected " + std::to_string(num_of_levels) + ".");
    }
    std::vector<Word::const_iterator> track_words_begins(num_of_levels);
    for (size_t track{ 0 }; track < num_of_levels; ++track) {
        track_words_begins[track] = track_words[track].begin();
    }

    const std::vector<Word::const_iterator> track_words_ends{
        [&](){
            std::vector<Word::const_iterator> track_words_ends(num_of_levels);
            for (size_t track{ 0 }; track < num_of_levels; ++track) {
                track_words_ends[track] = track_words[track].end();
            }
            return track_words_ends;
        }()
    };

    auto are_all_track_words_read = [&](const std::vector<Word::const_iterator>& word_begins){
       for (size_t i{ 0 }; Word::const_iterator word_it: word_begins) {
           if (word_it != track_words_ends[i]) { return false; }
           ++i;
       }
       return true;
    };

    if (are_all_track_words_read(track_words_begins) && final.intersects_with(initial)) { return true; }

    using StateWordBeginsPair = std::pair<State, std::vector<Word::const_iterator>>;
    std::deque<StateWordBeginsPair> worklist{};
    for (const State state: initial) {
        worklist.emplace_back(state, track_words_begins);
    }
    Level level;
    while (!worklist.empty()) {
        const auto [state, words_its]{ std::move(worklist.front()) };
        worklist.pop_front();
        level = levels[state];
        const StatePost& state_post{ delta[state] };
        const auto state_post_end{ state_post.end() };
        const Word::const_iterator word_symbol_it{ words_its[level] };

        auto symbol_post_it{ state_post.find(EPSILON) };
        if (symbol_post_it != state_post_end) {
            for (State target: symbol_post_it->targets) {
                if (are_all_track_words_read(words_its) && final.contains(target)) { return true; }
                worklist.emplace_back(target, words_its);
            }
        }

        if (word_symbol_it != track_words_ends[level]) {
//            auto symbol_post_it{ state_post.find(EPSILON) };
//            if (symbol_post_it != state_post_end) {
//                for (State target: symbol_post_it->targets) {
//                    if (are_all_track_words_read(words_its) && final.contains(target)) { return true; }
//                    worklist.emplace_back(target, words_its);
//                }
//            }

            symbol_post_it = state_post.find(DONT_CARE);
            if (*word_symbol_it != EPSILON && symbol_post_it != state_post_end) {
                for (const State target: symbol_post_it->targets) {
                    bool continue_to_next_target{ false };
                    std::vector<Word::const_iterator> next_words_its{ words_its };
                    Level level_in_transition{ level };
                    do {
                        if (next_words_its[level_in_transition] == track_words_ends[level_in_transition]) {
                            continue_to_next_target = true;
                        }
                        ++next_words_its[level_in_transition];
                        level_in_transition = (level_in_transition + 1) % static_cast<Level>(num_of_levels);
                    } while(level_in_transition % num_of_levels != levels[target] && !continue_to_next_target);
                    if (continue_to_next_target) { continue; }
                    if (are_all_track_words_read(next_words_its) && final.contains(target)) { return true; }
                    worklist.emplace_back(target, next_words_its);
                }
            }

            symbol_post_it = state_post.find(*word_symbol_it);
            if (*word_symbol_it != DONT_CARE && *word_symbol_it != EPSILON && symbol_post_it != state_post_end) {
                for (State target: symbol_post_it->targets) {
                    std::vector<Word::const_iterator> next_words_its{ words_its };
                    ++next_words_its[level];
                    if (are_all_track_words_read(next_words_its) && final.contains(target)) { return true; }
                    worklist.emplace_back(target, next_words_its);
                }
            }
            // TODO(nft): Input words may contain epsilons and dont cares, theoretically. Handle that.
        }
    }
    return false;
}
