/* composition.cc -- Composition of two NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include <cassert>


using namespace mata::utils;


namespace mata::nft
{

Nft compose_fast(const Nft& lhs, const Nft& rhs, const utils::OrdVector<Level>& lhs_sync_levels, const utils::OrdVector<Level>& rhs_sync_levels, const bool are_sync_levels_unwinded, const bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    // Number of Levels
    const size_t num_of_sync_levels = lhs_sync_levels.size();
    const size_t lhs_num_of_levels = lhs.num_of_levels;
    const size_t rhs_num_of_levels = rhs.num_of_levels;
    const size_t result_num_of_levels = lhs_num_of_levels + rhs_num_of_levels - (project_out_sync_levels ? (2 * num_of_sync_levels) : num_of_sync_levels);

    // Calculate number of lhs/rhs transitions before/between/after sync levels
    auto lhs_sync_levels_it = lhs_sync_levels.cbegin();
    auto rhs_sync_levels_it = rhs_sync_levels.cbegin();
    // Sync levels are on on borders between vector elements.
    std::vector<size_t> lhs_between(num_of_sync_levels + 1, 0);
    std::vector<size_t> rhs_between(num_of_sync_levels + 1, 0);
    // Before the first sync level
    lhs_between[0] = *lhs_sync_levels_it;
    rhs_between[0] = *rhs_sync_levels_it;
    // Between sync levels
    for (size_t i = 1; i < num_of_sync_levels; ++i) {
        const size_t lhs_prev = *lhs_sync_levels_it;
        const size_t rhs_prev = *rhs_sync_levels_it;
        ++lhs_sync_levels_it;
        ++rhs_sync_levels_it;
        lhs_between[i] = *lhs_sync_levels_it - lhs_prev - 1;
        rhs_between[i] = *rhs_sync_levels_it - rhs_prev - 1;
    }
    // After the last sync level
    lhs_between[num_of_sync_levels] = lhs_num_of_levels - *lhs_sync_levels_it - 1;
    rhs_between[num_of_sync_levels] = rhs_num_of_levels - *rhs_sync_levels_it - 1;

    // Is level a sync level?
    BoolVector lhs_is_sync_level(lhs_num_of_levels, false);
    BoolVector rhs_is_sync_level(rhs_num_of_levels, false);
    for (const Level level : lhs_sync_levels) {
        lhs_is_sync_level[level] = true;
    }
    for (const Level level : rhs_sync_levels) {
        rhs_is_sync_level[level] = true;
    }

    // Sync levels inverted
    // Element on index i (level i) tells the index of the level in sync_levels vector.
    std::vector<size_t> lhs_sync_levels_inv(lhs_num_of_levels, std::numeric_limits<size_t>::max());
    std::vector<size_t> rhs_sync_levels_inv(rhs_num_of_levels, std::numeric_limits<size_t>::max());
    size_t idx = 0;
    const auto lhs_end_it = lhs_sync_levels.cend();
    for (auto lhs_it = lhs_sync_levels.cbegin(), rhs_it = rhs_sync_levels.cbegin();
         lhs_it != lhs_end_it;
         ++lhs_it, ++rhs_it, ++idx)
    {
        lhs_sync_levels_inv[*lhs_it] = idx;
        rhs_sync_levels_inv[*rhs_it] = idx;
    }

    // Initialize the result NFT, state map, and worklist
    Nft result;
    result.num_of_levels = result_num_of_levels;
    std::unordered_map<std::pair<State, State>, State> main_state_map;
    std::unordered_map<State, StateSet> pred_map;
    std::stack<std::pair<State, State>> worklist;
    std::unordered_set<State> commited_states;

    // Function: adds a new state to the result and puts it to the worklist
    auto try_to_map_state_add_to_wordlist = [&](const State state, const State orig_a, const State orig_b, const bool is_a_lhs) {
        const auto key = is_a_lhs ? std::make_pair(orig_a, orig_b) : std::make_pair(orig_b, orig_a);
        auto it = main_state_map.find(key);
        if (it != main_state_map.end()) {
            assert(it->second == state);
            return;
        }
        main_state_map[key] = state;
        result.levels[state] = 0;
        worklist.push(key);
        if ((is_a_lhs && lhs.final.contains(orig_a) && rhs.final.contains(orig_b)) ||
            (!is_a_lhs && rhs.final.contains(orig_a) && lhs.final.contains(orig_b))) {
            result.final.insert(state);
        }
    };

    // Function: returns existing zero-level state or adds a new one and puts it to the worklist
    auto get_state_or_add_and_put_to_worklist = [&](const State orig_a, const State orig_b, const bool is_a_lhs, std::stack<std::pair<State, State>>& worklist) {
        const auto key = is_a_lhs ? std::make_pair(orig_a, orig_b) : std::make_pair(orig_b, orig_a);
        auto it = main_state_map.find(key);
        if (it != main_state_map.end()) {
            return it->second;
        }
        const State new_state = result.add_state_with_level(0);
        main_state_map[key] = new_state;
        worklist.push(key);
        if ((is_a_lhs && lhs.final.contains(orig_a) && rhs.final.contains(orig_b)) ||
            (!is_a_lhs && rhs.final.contains(orig_a) && lhs.final.contains(orig_b))) {
            result.final.insert(new_state);
        }
        return new_state;
    };

    auto get_state = [&](const State lhs_state, const State rhs_state, const Level level) {
        // Get the state from the map or add a new one.
        const auto key = std::make_pair(lhs_state, rhs_state);
        auto it = main_state_map.find(key);
        if (it != main_state_map.end()) {
            return it->second;
        }
        const State new_state = result.add_state_with_level(level);
        main_state_map[key] = new_state;
        if (level == 0) {
            worklist.push(key);
            if (lhs.final.contains(lhs_state) && rhs.final.contains(rhs_state)) {
                result.final.insert(new_state);
            }
        }
        return new_state;
    };

    // Function: adds a transition from src to tgt with the given symbol
    // TODO: Replace by a method in Nft
    auto repeat_transition = [&](const State src, const size_t trans_len, const Symbol symbol) {
        if (trans_len == 0) {
            return src; // No transition, return the same state
        }
        if (jump_mode == JumpMode::RepeatSymbol || trans_len == 1) {
            assert((result.levels[src] + trans_len) <= result_num_of_levels);
            const State tgt = result.add_state_with_level((result.levels[src] + trans_len) % result_num_of_levels);
            pred_map[tgt].insert(src);
            result.delta.add(src, symbol, tgt);
            return tgt;
        }
        State inner_src = src;
        for (size_t i = 0; i < trans_len - 1; ++i) {
            assert(result.levels[inner_src] + 1 < result_num_of_levels);
            const State tgt = result.add_state_with_level(result.levels[inner_src] + 1);
            result.delta.add(inner_src, symbol, tgt);
            inner_src = tgt;
        }
        assert((result.levels[inner_src] + 1) <= result_num_of_levels);
        const State tgt = result.add_state_with_level((result.levels[inner_src] + 1) % result_num_of_levels);
        pred_map[tgt].insert(inner_src);
        result.delta.add(inner_src, symbol, tgt);
        return tgt;
    };

    // Function: adds a transition from src to tgt with the given symbol
    // TODO: Replace by a method in Nft
    auto repeat_transition_with_tgt = [&](const State src, const State tgt, const size_t trans_len, const Symbol symbol) {
        if (trans_len == 0) {
            return; // No transition, do nothing
        }
        if (jump_mode == JumpMode::RepeatSymbol || trans_len == 1) {
            pred_map[tgt].insert(src);
            result.delta.add(src, symbol, tgt);
            return;
        }
        State inner_src = repeat_transition(src, trans_len - 1, symbol);
        pred_map[tgt].insert(inner_src);
        result.delta.add(inner_src, symbol, tgt);
    };

    // Function: redirects transitions from old_tgt to new_tgt
    auto redirect = [&](const State old_tgt, const State new_tgt) {
        if (old_tgt == new_tgt) {
            return;
        }
        auto it = pred_map.find(old_tgt);
        assert(it != pred_map.end());
        for (const State pred: it->second) {
            for (SymbolPost& symbol_post: result.delta.mutable_state_post(pred)) {
                if (symbol_post.targets.contains(old_tgt)) {
                    symbol_post.targets.insert(new_tgt);
                }
            }
        }
    };

    // Function: maps epsilon transitions on the sync path
    auto map_epsilon_on_sync_path = [&](const State res_root, const State orig_root, const State wait_root, const bool is_orig_lhs) {
        const Nft& orig_nft = is_orig_lhs ? lhs : rhs;
        const OrdVector<Level>& sync_levels = is_orig_lhs ? lhs_sync_levels : rhs_sync_levels;
        const std::vector<size_t>& sync_levels_inv = is_orig_lhs ? lhs_sync_levels_inv : rhs_sync_levels_inv;
        const std::vector<size_t>& between = is_orig_lhs ? rhs_between : lhs_between;   // Use rhs_between if we are in lhs NFT, and vice versa.
        const BoolVector& is_sync_level = is_orig_lhs ? lhs_is_sync_level : rhs_is_sync_level;
        const bool add_before = !is_orig_lhs;   // On each section within sync levels, lhs transitions go begore rhs transitions.
        std::stack<std::pair<State, State>>& main_worklist = worklist;

        // Worklist contains pairs of (state in the result NFT, and state in the original lhs/rhs NFT).
        std::stack<std::pair<State, State>> worklist;
        worklist.emplace(res_root, orig_root);
        StateSet visited;   // It is not necessary is the NFT has a valid structure (each cycle contains a zero-level state).
        visited.insert(orig_root);
        pred_map.clear();

        std::unordered_map<State, State> local_state_map;
        auto get_state = [&](const State orig_state, const Level level) {
            // Get the state from the map or add a new one.
            auto it = local_state_map.find(orig_state);
            if (it != local_state_map.end()) {
                assert(result.levels[it->second] == level);
                return it->second;
            }
            const State new_state = result.add_state_with_level(level);
            local_state_map[orig_state] = new_state;
            return new_state;
        };

        while (!worklist.empty()) {
            auto [res_src, orig_src] = worklist.top();
            worklist.pop();

            // Add EPSILON transition for those in lhs BEFORE the first transition (we are in rhs).
            if (res_src == res_root && add_before) {
                res_src = repeat_transition(res_src, between[0], EPSILON);
            }

            if (is_sync_level[orig_nft.levels[orig_src]]) {
                // We are at a sync level. We need to match only the epsilon transitions.
                const auto epsilon_post_it = orig_nft.delta[orig_src].find(EPSILON);
                if (epsilon_post_it == orig_nft.delta[orig_src].end()) {
                    continue;
                }
                const size_t sync_level_idx = sync_levels_inv[orig_nft.levels[orig_src]];
                // Add EPSILON transitions for those in rhs AFTER this last transition before the sync level (we are in lhs).
                if (!add_before) {
                    res_src = repeat_transition(res_src, between[sync_level_idx], EPSILON);
                }
                // Add EPSILON transitions for those in lhs BEFORE first transition after the sync level (we are in rhs).
                if (add_before) {
                    res_src = repeat_transition(res_src, between[sync_level_idx + 1], EPSILON);
                }

                if (!project_out_sync_levels) {
                    res_src = repeat_transition(res_src, 1, EPSILON);
                }

                // Process targets of the epsilon transitions (this transition will be projected out).
                for (const State orig_tgt: epsilon_post_it->targets) {
                    if (orig_nft.levels[orig_tgt] == 0) {
                        // We are connecting to a zero-level state.
                        // Add EPSILON transitions for those in rhs AFTER this last transition (we are in lhs).
                        if (!add_before && between[num_of_sync_levels] != 0) {
                            const State res_tgt = get_state_or_add_and_put_to_worklist(orig_tgt, wait_root, is_orig_lhs, main_worklist);
                            repeat_transition_with_tgt(res_src, res_tgt, between[num_of_sync_levels], EPSILON);
                            commited_states.insert(res_tgt);
                        } else {
                            // There is nothing to add.
                            // Try to find if there is already a mapping for this state.
                            const auto key = is_orig_lhs ? std::make_pair(orig_tgt, wait_root) : std::make_pair(wait_root, orig_tgt);
                            auto it = main_state_map.find(key);
                            if (it != main_state_map.end()) {
                                // The mapping exists, redirect transition goig to orig_src to the existing state.
                                redirect(res_src, it->second);
                            } else {
                                // The mapping does not exist. Update it.
                                try_to_map_state_add_to_wordlist(res_src, orig_tgt, wait_root, is_orig_lhs);
                                commited_states.insert(res_src);
                            }
                        }
                    } else {
                        // We are connecting to a non-zero-level state.
                        if (!visited.contains(orig_tgt)) {
                            // Let's continue the traversal.
                            // Project out the transition. Merge orig_scr and orig_tgt, while staying in res_src.
                            worklist.push({res_src, orig_tgt});
                            visited.insert(orig_tgt);
                        }
                    }
                }
            } else {
                // We are not at a sync level. We need just to copy the transitions.
                const Level orig_src_level = orig_nft.levels[orig_src];
                for (const SymbolPost& symbol_post: orig_nft.delta[orig_src]) {
                    for (const State orig_tgt: symbol_post.targets) {
                        const Level orig_tgt_level = orig_nft.levels[orig_tgt];

                        if (orig_tgt_level == 0) {
                            // We are connecting to a zero-level state.
                            const Level level_diff = orig_nft.num_of_levels - orig_src_level;
                            if (!add_before && between[num_of_sync_levels] != 0) {
                                // We will add EPSILON transitions from rhs AFTER this last transition (we are in lhs).
                                assert(result.levels[res_src] + level_diff < result_num_of_levels);
                                const bool exist_res_tgt_inner = local_state_map.contains(orig_tgt);
                                const State res_tgt_inner = get_state(orig_tgt, result.levels[res_src] + level_diff);
                                // First process the transition with the symbol_post.symbol.
                                result.delta.add(res_src, symbol_post.symbol, res_tgt_inner);
                                if (!exist_res_tgt_inner) {
                                    // Then add the EPSILON transitions.
                                    const State res_tgt = get_state_or_add_and_put_to_worklist(orig_tgt, wait_root, is_orig_lhs, main_worklist);
                                    repeat_transition_with_tgt(res_tgt_inner, res_tgt, between[num_of_sync_levels], EPSILON);
                                    commited_states.insert(res_tgt);
                                }
                            } else {
                                assert(result.levels[res_src] + level_diff == result_num_of_levels);
                                const State res_tgt = get_state_or_add_and_put_to_worklist(orig_tgt, wait_root, is_orig_lhs, main_worklist);
                                result.delta.add(res_src, symbol_post.symbol, res_tgt);
                                commited_states.insert(res_tgt);
                            }
                        } else {
                            // We are connecting to a non-zero-level state.
                            // Just copy the transition.
                            const Level level_diff = orig_tgt_level - orig_src_level;
                            assert(result.levels[res_src] + level_diff <= result_num_of_levels);
                            const State tgt_res = get_state(orig_tgt, (result.levels[res_src] + level_diff) % result_num_of_levels);
                            result.delta.add(res_src, symbol_post.symbol, tgt_res);
                            pred_map[tgt_res].insert(res_src);
                            if (!visited.contains(orig_tgt)) {
                                // Not visited, add to the worklist
                                worklist.push({tgt_res, orig_tgt});
                                visited.insert(orig_tgt);
                            }
                        }
                    }
                }
            }
        }
    };

    auto map_combination_path = [&](const State res_root, const State lhs_root, const State rhs_root) {
        std::stack<std::pair<State, State>>& main_worklist = worklist;
        pred_map.clear();
        std::stack<std::tuple<State, State, State>> worklist;
        worklist.push({res_root, lhs_root, rhs_root});
        std::unordered_set<std::pair<State, State>> visited;
        visited.insert({lhs_root, rhs_root});

        bool perform_lhs_wait = false;
        bool perform_rhs_wait = false;

        while (!worklist.empty()) {
            auto [res_src, lhs_src, rhs_src] = worklist.top();
            worklist.pop();

            const Level res_src_level = result.levels[res_src];
            const Level lhs_src_level = lhs.levels[lhs_src];
            const Level rhs_src_level = rhs.levels[rhs_src];

            const bool lhs_in_target_and_rhs_remains = lhs_src_level == 0 && rhs_src_level != 0;

            // Go in lhs all the way down to the sync level.
            if (!lhs_is_sync_level[lhs_src_level] && !lhs_in_target_and_rhs_remains && !lhs.delta[lhs_src].empty()) {
                for (const SymbolPost& symbol_post: lhs.delta[lhs_src]) {
                    for (const State lhs_tgt: symbol_post.targets) {
                        const Level lhs_tgt_level = lhs.levels[lhs_tgt];
                        if (lhs_tgt_level == 0) {
                            const size_t trans_len = lhs_num_of_levels - lhs_src_level;
                            // We are connecting to a zero-level state.
                            if (rhs_between[num_of_sync_levels] == 0) {
                                // There is no transition in rhs.
                                const State res_tgt = get_state_or_add_and_put_to_worklist(lhs_tgt, rhs_src, true, main_worklist);
                                result.delta.add(res_src, symbol_post.symbol, res_tgt);
                                commited_states.insert(res_tgt);
                            } else {
                                // There is a transition in rhs.
                                const State res_tgt = get_state(lhs_tgt, rhs_src, trans_len + res_src_level);
                                result.delta.add(res_src, symbol_post.symbol, res_tgt);
                                pred_map[res_tgt].insert(res_src);
                                if (!visited.contains({lhs_tgt, rhs_src})) {
                                    // Not visited, add to the worklist
                                    worklist.push({res_tgt, lhs_tgt, rhs_src});
                                    visited.insert({lhs_tgt, rhs_src});
                                }
                            }
                        } else {
                            // We are connecting to a non-zero-level state.
                            const Level level_diff = lhs_tgt_level - lhs_src_level;
                            assert(res_src_level + level_diff <= result_num_of_levels);
                            const State res_tgt = get_state(lhs_tgt, rhs_src, (res_src_level + level_diff) % result_num_of_levels);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            pred_map[res_tgt].insert(res_src);
                            if (!visited.contains({lhs_tgt, rhs_src})) {
                                // Not visited, add to the worklist
                                worklist.push({res_tgt, lhs_tgt, rhs_src});
                                visited.insert({lhs_tgt, rhs_src});
                            }
                        }
                    }
                }
                continue;
            }

            // Go in rhs all the way down to the sync level.
            if (!rhs_is_sync_level[rhs_src_level]) {
                for (const SymbolPost& symbol_post: rhs.delta[rhs_src]) {
                    for (const State rhs_tgt: symbol_post.targets) {
                        const Level rhs_tgt_level = rhs.levels[rhs_tgt];
                        if (rhs_tgt_level == 0) {
                            const State res_tgt = get_state_or_add_and_put_to_worklist(lhs_src, rhs_tgt, true, main_worklist);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            commited_states.insert(res_tgt);
                        } else {
                            // We are connecting to a non-zero-level state.
                            const Level level_diff = rhs_tgt_level - rhs_src_level;
                            assert(res_src_level + level_diff <= result_num_of_levels);
                            const State res_tgt = get_state(lhs_src, rhs_tgt, (res_src_level + level_diff) % result_num_of_levels);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            pred_map[res_tgt].insert(res_src);
                            if (!visited.contains({lhs_src, rhs_tgt})) {
                                // Not visited, add to the worklist
                                worklist.push({res_tgt, lhs_src, rhs_tgt});
                                visited.insert({lhs_src, rhs_tgt});
                            }
                        }
                    }
                }
                continue;
            }

            // Everythink is on sync level.
            const bool epsilon_in_lhs = lhs.delta[lhs_src].find(EPSILON) != lhs.delta[lhs_src].end();
            const bool epsilon_in_rhs = rhs.delta[rhs_src].find(EPSILON) != rhs.delta[rhs_src].end();
            perform_lhs_wait = perform_lhs_wait || (epsilon_in_rhs && !epsilon_in_lhs);
            perform_rhs_wait = perform_rhs_wait || (epsilon_in_lhs && !epsilon_in_rhs);

            auto process_intersection = [&](const StateSet& lhs_targets, const StateSet& rhs_targets, const Symbol symbol) {
                State local_res_src = res_src;
                if (!project_out_sync_levels && !lhs_targets.empty() && !rhs_targets.empty()) {
                    local_res_src = repeat_transition(local_res_src, 1, symbol);
                }
                for (const State lhs_tgt: lhs_targets) {
                    const Level lhs_tgt_level = lhs.levels[lhs_tgt];
                    for (const State rhs_tgt: rhs_targets) {
                        const Level rhs_tgt_level = rhs.levels[rhs_tgt];
                        if (lhs_tgt_level == 0 && rhs_tgt_level == 0) {
                            const auto key = std::make_pair(lhs_tgt, rhs_tgt);
                            auto it = main_state_map.find(key);
                            // if (!project_out_sync_levels) {
                            //     if (it != main_state_map.end()) {
                            //         const State res_tgt = it->second;
                            //         // The mapping exists, redirect transition going to orig_src to the existing state.
                            //         result.delta.add(res_src, symbol, res_tgt);
                            //     } else {
                            //         // The mapping does not exist. Update it.
                            //         const State res_tgt = get_state_or_add_and_put_to_worklist(lhs_tgt, rhs_tgt, true, main_worklist);
                            //         result.delta.add(res_src, symbol, res_tgt);
                            //         commited_states.insert(res_tgt);
                            //     }
                            // } else {
                                if (it != main_state_map.end()) {
                                    // The mapping exists, redirect transition goig to orig_src to the existing state.
                                    redirect(local_res_src, it->second);
                                } else {
                                    // The mapping does not exist. Update it.
                                    try_to_map_state_add_to_wordlist(local_res_src, lhs_tgt, rhs_tgt, true);
                                    commited_states.insert(local_res_src);
                                }
                            // }
                        } else {
                            // if (!project_out_sync_levels) {
                            //     const State res_tgt = get_state(lhs_tgt, rhs_tgt, (res_src_level + 1) % result_num_of_levels);
                            //     result.delta.add(res_src, symbol, res_tgt);
                            //     pred_map[res_tgt].insert(res_src);
                            //     if (!visited.contains({lhs_tgt, rhs_tgt})) {
                            //         // We have not visited this pair yet.
                            //         visited.insert({lhs_tgt, rhs_tgt});
                            //         worklist.push({res_tgt, lhs_tgt, rhs_tgt});
                            //     }
                            // } else {
                                if (!visited.contains({lhs_tgt, rhs_tgt})) {
                                    // We have not visited this pair yet.
                                    visited.insert({lhs_tgt, rhs_tgt});
                                    main_state_map[std::make_pair(lhs_tgt, rhs_tgt)] = local_res_src;
                                    worklist.push({local_res_src, lhs_tgt, rhs_tgt});
                                }
                            // }
                        }
                    }
                }
            };

            // Intersection
            mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
            mata::utils::push_back(sync_iterator, lhs.delta[lhs_src]);
            mata::utils::push_back(sync_iterator, rhs.delta[rhs_src]);
            while (sync_iterator.advance()) {
                const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
                assert(same_symbol_posts.size() == 2); // One move per state in the pair.

                process_intersection(
                    same_symbol_posts[0]->targets,
                    same_symbol_posts[1]->targets,
                    same_symbol_posts[0]->symbol
                );
            }

            if (lhs.delta[lhs_src].find(DONT_CARE) != lhs.delta[lhs_src].end()) {
                for (const SymbolPost& symbol_post: rhs.delta[rhs_src]) {
                    process_intersection(
                        lhs.delta[lhs_src].find(DONT_CARE)->targets,
                        symbol_post.targets,
                        symbol_post.symbol
                    );
                }
            }

            if (rhs.delta[rhs_src].find(DONT_CARE) != rhs.delta[rhs_src].end()) {
                for (const SymbolPost& symbol_post: lhs.delta[lhs_src]) {
                    process_intersection(
                        symbol_post.targets,
                        rhs.delta[rhs_src].find(DONT_CARE)->targets,
                        symbol_post.symbol
                    );
                }
            }
        }

        if (perform_lhs_wait) {
            map_epsilon_on_sync_path(res_root, rhs_root, lhs_root, false);
        }
        if (perform_rhs_wait) {
            map_epsilon_on_sync_path(res_root, lhs_root, rhs_root, true);
        }
    };

    // INITIALIZATION
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            const State res_root = get_state_or_add_and_put_to_worklist(lhs_root, rhs_root, true, worklist);
            commited_states.insert(res_root);
            result.initial.insert(res_root);
        }
    }

    // MAIN LOOP
    while (!worklist.empty()) {
        auto [orig_a, orig_b] = worklist.top();
        worklist.pop();

        // Get the state in the result NFT
        const State res_root = main_state_map.at({orig_a, orig_b});
        if (!commited_states.contains(res_root)) {
            result.final.erase(res_root);
            continue;
        }

        // Map epsilon transitions on the sync path
        map_combination_path(res_root, orig_a, orig_b);
        // map_epsilon_on_sync_path(res_root, orig_b, orig_a, false);
        // break;
    }

    return result.trim();
}



Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());


    return compose_fast(lhs.unwind_jumps({ DONT_CARE }, jump_mode), rhs.unwind_jumps({ DONT_CARE }, jump_mode), lhs_sync_levels, rhs_sync_levels, true, project_out_sync_levels, JumpMode::NoJump);

    // Inserts loop into the given Nft for each state with level 0.
    // The loop word is constructed using the EPSILON symbol for all levels, except for the levels
    // where is_dcare_on_transition is true, in which case the DONT_CARE symbol is used.
    auto insert_self_loops = [&](Nft &nft, const BoolVector &is_dcare_on_transition) {
        Word loop_word(nft.num_of_levels, EPSILON);
        for (size_t i{ 0 }; i < nft.num_of_levels; i++) {
            if (is_dcare_on_transition[i]) {
                loop_word[i] = DONT_CARE;
            }
        }

        size_t original_num_of_states = nft.num_of_states();
        for (State s{ 0 }; s < original_num_of_states; s++) {
            if (nft.levels[s] == 0) {
                nft.insert_word(s, loop_word, s);
            }
        }
    };

    // Calculate the number of non-synchronization levels in lhs and rhs before/after each/last synchronization level.
    // Example:
    //    Lets suppose we have 2 synchnonization levels.
    //    The vector lhs_nonsync_levels_cnt = { 2, 0, 1 } indicates that
    //    - before the first synchronization level (i == 0) there are 2 non-synchronization levels
    //    - before the second synchronization level (i == 1) there are 0 non-synchronization levels
    //    - after the last synchronization level (i == |sync|) there is 1 non-synchronization level
    std::vector<unsigned> lhs_nonsync_levels_cnt;
    std::vector<unsigned> rhs_nonsync_levels_cnt;
    const size_t num_of_sync_levels = lhs_sync_levels.size();
    lhs_nonsync_levels_cnt.reserve(num_of_sync_levels + 1);
    rhs_nonsync_levels_cnt.reserve(num_of_sync_levels + 1);
    auto lhs_sync_levels_it = lhs_sync_levels.cbegin();
    auto rhs_sync_levels_it = rhs_sync_levels.cbegin();
    // Before the first synchronization level (i == 0)
    lhs_nonsync_levels_cnt.push_back(*lhs_sync_levels_it++);
    rhs_nonsync_levels_cnt.push_back(*rhs_sync_levels_it++);
    // Before each synchronization level (i < |sync|)
    for (size_t i = 1; i < num_of_sync_levels; ++i) {
        lhs_nonsync_levels_cnt.push_back(*lhs_sync_levels_it - *(lhs_sync_levels_it - 1) - 1);
        rhs_nonsync_levels_cnt.push_back(*rhs_sync_levels_it - *(rhs_sync_levels_it - 1) - 1);
        ++lhs_sync_levels_it;
        ++rhs_sync_levels_it;
    }
    // After the last synchronization level (i == |sync|)
    lhs_nonsync_levels_cnt.push_back(static_cast<unsigned>(lhs.num_of_levels) - *(lhs_sync_levels_it - 1) - 1);
    rhs_nonsync_levels_cnt.push_back(static_cast<unsigned>(rhs.num_of_levels) - *(rhs_sync_levels_it - 1) - 1);

    // Construct a mask for new levels in lhs and rhs.
    // For each synchronization block (non-synchronization levels up to a synchronization transition),
    // first insert the non-synchronization levels from lhs, then from rhs, and finally the synchronization level itself.
    // For the last block of non-synchronization levels after the last synchronization level,
    // insert the non-synchronization levels from lhs first, followed by the levels from rhs.
    // Example:
    //                 LHS                         |                    RHS
    // --------------------------------------------+---------------------------------------------
    //            num_of_levels = 5                |             num_of_levels = 4
    //          sync_levels = { 2, 4 }             |            sync_levels = { 1, 2 }
    // --------------------------------------------|---------------------------------------------
    //       nonsync_levels_cnt = { 2, 1, 0 }      |       nonsync_levels_cnt = { 1, 0, 1 }
    //  new_levels_mask = { 0, 0, 1, 0, 0, 0, 1 }  |  new_levels_mask = { 1, 1, 0, 0, 1, 0, 0 }
    auto lhs_nonsync_levels_cnt_it = lhs_nonsync_levels_cnt.cbegin();
    auto rhs_nonsync_levels_cnt_it = rhs_nonsync_levels_cnt.cbegin();
    BoolVector lhs_new_levels_mask;
    BoolVector rhs_new_levels_mask;
    OrdVector<Level> sync_levels_to_project_out;
    Level level = 0;
    const size_t nonsync_levels_cnt_size = lhs_nonsync_levels_cnt.size();
    for (size_t i = 0; i < nonsync_levels_cnt_size; ++i) {
        // LHS goes first -> make space (insert new levels) in RHS
        for (unsigned j = 0; j < *lhs_nonsync_levels_cnt_it; ++j, ++level) {
            lhs_new_levels_mask.push_back(false);
            rhs_new_levels_mask.push_back(true);
        }
        // RHS goes first -> make space (insert new levels) in LHS
        for (unsigned j = 0; j < *rhs_nonsync_levels_cnt_it; ++j, ++level) {
            lhs_new_levels_mask.push_back(true);
            rhs_new_levels_mask.push_back(false);
        }
        // The synchronization level goes last
        if (i < num_of_sync_levels) {
            sync_levels_to_project_out.push_back(level);
            lhs_new_levels_mask.push_back(false);
            rhs_new_levels_mask.push_back(false);
            ++level;
        }
        ++lhs_nonsync_levels_cnt_it;
        ++rhs_nonsync_levels_cnt_it;
    }

    // Insert new levels into lhs and rhs.
    Nft lhs_synced = insert_levels(lhs, lhs_new_levels_mask, jump_mode);
    Nft rhs_synced = insert_levels(rhs, rhs_new_levels_mask, jump_mode);

    // Two auxiliary states (states from inserted loops) can not create a product state.
    const State lhs_first_aux_state = lhs_synced.num_of_states();
    const State rhs_first_aux_state = rhs_synced.num_of_states();

    // Insert self-loops into lhs and rhs to ensure synchronization on epsilon transitions.
    insert_self_loops(lhs_synced, lhs_new_levels_mask);
    insert_self_loops(rhs_synced, rhs_new_levels_mask);

    Nft result{ intersection(lhs_synced, rhs_synced, nullptr, jump_mode, lhs_first_aux_state, rhs_first_aux_state) };
    if (project_out_sync_levels) {
        result = project_out(result, sync_levels_to_project_out, jump_mode);
    }
    return result;
}

Nft compose(const Nft& lhs, const Nft& rhs, const Level lhs_sync_level, const Level rhs_sync_level, bool project_out_sync_levels, const JumpMode jump_mode) {
    return compose(lhs, rhs, OrdVector{ lhs_sync_level }, OrdVector{ rhs_sync_level }, project_out_sync_levels, jump_mode);
}

} // mata::nft
