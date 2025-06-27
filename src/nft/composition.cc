/* composition.cc -- Composition of two NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include <cassert>


using namespace mata::utils;


namespace {
    using mata::Symbol;
    using namespace mata::nft;
    using PredMap = std::unordered_map<State, StateSet>;

    using ProductMap = std::unordered_map<std::pair<State,State>,State>;
    using MatrixProductStorage = std::vector<std::vector<State>>;
    using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
    using InvertedProductStorage = std::vector<State>;

    template<typename T>
    mata::BoolVector create_mask(const OrdVector<T>& entries, const size_t universum_size) {
        mata::BoolVector mask(universum_size, false);
        for (const auto& entry : entries) {
            mask[entry] = true;
        }
        return mask;
    }

    template<typename T>
    std::vector<size_t> invert(const OrdVector<T>& entries, const size_t universum_size) {
        std::vector<size_t> inverted(universum_size, std::numeric_limits<T>::max());
        size_t idx = 0;
        for (const auto& entry : entries) {
            inverted[entry] = idx++;
        }
        return inverted;
    }

    template<typename T>
    std::vector<size_t> get_partition_sizes(const OrdVector<T>& border_indices, const size_t universum_size) {
        assert(!border_indices.empty());
        const size_t num_partitions = border_indices.size();
        std::vector<size_t> sizes(num_partitions + 1, 0);
        auto partition_borders_it = border_indices.cbegin();
        sizes[0] = *partition_borders_it;
        for (size_t i = 1; i < num_partitions; ++i) {
            const size_t prev_border = *partition_borders_it;
            ++partition_borders_it;
            sizes[i] = *partition_borders_it - prev_border - 1;
        }
        sizes[num_partitions] = universum_size - *partition_borders_it - 1;
        return sizes;
    }

    State add_transition(Nft& nft, const State source, const Symbol symbol, const size_t len, const JumpMode jump_mode, PredMap& pred_map) {
        assert(nft.levels[source] + len <= nft.num_of_levels);
        if (len == 0) { return source; }

        const State target = nft.add_state_with_level((nft.levels[source] + len) % static_cast<Level>(nft.num_of_levels));
        if (len == 1 || jump_mode == JumpMode::RepeatSymbol) {
            nft.delta.add(source, symbol, target);
            pred_map[target].insert(source);
            return target;
        }
        State inner_src = source;
        for (size_t i = 0; i < len - 1; ++i) {
            const State inner_target = nft.add_state_with_level(nft.levels[inner_src] + 1);
            nft.delta.add(inner_src, symbol, inner_target);
            // pred_map[inner_target].insert(inner_src);    // Optimization: we do not need to track inner states in pred_map.
            inner_src = inner_target;
        }
        nft.delta.add(inner_src, symbol, target);
        pred_map[target].insert(inner_src);
        return target;
    }

    void add_transition_with_target(Nft& nft, const State source, const Symbol symbol, const State target, const JumpMode jump_mode, PredMap& pred_map) {
        if (source == target) { return;}
        assert(nft.levels[source] < nft.levels[target] || nft.levels[target] == 0);

        const size_t trans_len = nft.levels[target] == 0 ? nft.num_of_levels - nft.levels[source] : nft.levels[target] - nft.levels[source];
        assert(trans_len > 0);
        if (trans_len == 1 || jump_mode == JumpMode::RepeatSymbol) {
            nft.delta.add(source, symbol, target);
            pred_map[target].insert(source);
            return;
        }
        State inner_src = source;
        for (size_t i = 0; i < trans_len - 1; ++i) {
            const State inner_target = nft.add_state_with_level(nft.levels[inner_src] + 1);
            nft.delta.add(inner_src, symbol, inner_target);
            // pred_map[inner_target].insert(inner_src);    // Optimization: we do not need to track inner states in pred_map.
            inner_src = inner_target;
        }
        nft.delta.add(inner_src, symbol, target);
        pred_map[target].insert(inner_src);
    }

    void redirect_transitions(Nft& nft, const State old_target, const State new_target, const PredMap& pred_map) {
        if (old_target == new_target) { return; }

        auto it = pred_map.find(old_target);
        assert(it != pred_map.end());
        for (const State pred: it->second) {
            for (SymbolPost& symbol_post: nft.delta.mutable_state_post(pred)) {
                if (symbol_post.targets.contains(old_target)) {
                    symbol_post.targets.insert(new_target);
                }
            }
        }
    }
}


namespace mata::nft
{

// TODO: Refactor this function to be more readable and maintainable.
//       There is a shitton of code duplication in this function.
Nft compose_fast(const Nft& lhs, const Nft& rhs, const utils::OrdVector<Level>& lhs_sync_levels, const utils::OrdVector<Level>& rhs_sync_levels, const bool project_out_sync_levels, const bool are_sync_levels_unwinded, const JumpMode jump_mode) {
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    // Number of Levels and States
    const size_t num_of_sync_levels = lhs_sync_levels.size();
    const size_t lhs_num_of_levels = lhs.num_of_levels;
    const size_t rhs_num_of_levels = rhs.num_of_levels;
    const size_t lhs_num_of_states = lhs.num_of_states();
    const size_t rhs_num_of_states = rhs.num_of_states();
    const size_t result_num_of_levels = lhs_num_of_levels + rhs_num_of_levels - (project_out_sync_levels ? (2 * num_of_sync_levels) : num_of_sync_levels);

    // FAST STORAGE OF COMPOSITION STATES
    // The largest matrix (product_matrix) of pairs of states we are brave enough to allocate.
    // Let's say we are fine with allocating larget_composition * (about 8 Bytes) space.
    // So ten million cells is close to 100 MB.
    // If the number is larger, then we do not allocate a matrix, but use a vector of unordered maps (composition_vec_map).
    // The unordered_map seems to be about twice slower.
    constexpr size_t MAX_COMPOSITION_MATRIX_SIZE = 50'000'000;
    // constexpr size_t MAX_COMPOSITION_MATRIX_SIZE = 0;
    const bool larget_composition = lhs_num_of_states * rhs_num_of_states > MAX_COMPOSITION_MATRIX_SIZE;
    assert(lhs_num_of_states < Limits::max_state);
    assert(rhs_num_of_states < Limits::max_state);

    //Two variants of storage for the mapping from pairs of lhs and rhs states to composition state, for large and non-large products.
    MatrixProductStorage matrix_composition_storage;
    VecMapProductStorage vec_map_composition_storage;
    InvertedProductStorage composition_to_lhs(lhs_num_of_states+rhs_num_of_states);
    InvertedProductStorage composition_to_rhs(lhs_num_of_states+rhs_num_of_states);

    //Initialize the storage, according to the number of possible state pairs.
    if (!larget_composition)
        matrix_composition_storage = MatrixProductStorage(lhs_num_of_states, std::vector<State>(rhs_num_of_states, Limits::max_state));
    else
        vec_map_composition_storage = VecMapProductStorage(lhs_num_of_states);

    /// Give me the composition state for the pair of lhs and rhs states.
    /// Returns Limits::max_state if not found.
    auto get_state_from_product_storage = [&](State lhs_state, State rhs_state) {
        if (!larget_composition)
            return matrix_composition_storage[lhs_state][rhs_state];
        else {
            auto it = vec_map_composition_storage[lhs_state].find(rhs_state);
            if (it == vec_map_composition_storage[lhs_state].end())
                return Limits::max_state;
            else
                return it->second;
        }
    };

    /// Insert new mapping lhs rhs state pair to composition state.
    auto insert_to_product_storage = [&](State lhs_state, State rhs_state, State composition_state) {
        if (!larget_composition)
            matrix_composition_storage[lhs_state][rhs_state] = composition_state;
        else
            vec_map_composition_storage[lhs_state][rhs_state] = composition_state;

        composition_to_lhs.resize(composition_state+1);
        composition_to_rhs.resize(composition_state+1);
        composition_to_lhs[composition_state] = lhs_state;
        composition_to_rhs[composition_state] = rhs_state;
    };

    // Initialize the result NFT, state map, and worklist
    Nft result;
    result.num_of_levels = result_num_of_levels;
    std::unordered_map<State, StateSet> pred_map;
    std::stack<std::pair<State, State>> worklist;
    std::unordered_set<State> commited_states;

    // Calculate number of lhs/rhs transitions before/between/after sync levels
    const std::vector<size_t> lhs_between = get_partition_sizes(lhs_sync_levels, lhs_num_of_levels);
    const std::vector<size_t> rhs_between = get_partition_sizes(rhs_sync_levels, rhs_num_of_levels);

    BoolVector lhs_is_sync_level = create_mask(lhs_sync_levels, lhs_num_of_levels);
    BoolVector rhs_is_sync_level = create_mask(rhs_sync_levels, rhs_num_of_levels);

    const std::vector<size_t> lhs_sync_levels_inv = invert(lhs_sync_levels, lhs_num_of_levels);
    const std::vector<size_t> rhs_sync_levels_inv = invert(rhs_sync_levels, rhs_num_of_levels);

    auto create_composition_state = [&](const State state_a, const State state_b, const Level level, const bool is_a_lhs = true, const State composition_state_to_add = Limits::max_state) {
        assert(composition_state_to_add == Limits::max_state || result.levels[composition_state_to_add] == level);

        // Try to find the entry in the state map.
        const auto key = is_a_lhs ? std::make_pair(state_a, state_b) : std::make_pair(state_b, state_a);
        const State lhs_state = key.first;
        const State rhs_state = key.second;
        const State found_state = get_state_from_product_storage(lhs_state, rhs_state);
        if (found_state != Limits::max_state) {
            assert(found_state == composition_state_to_add || composition_state_to_add == Limits::max_state);
            return found_state;
        }

        // If not found, add a new state to the result NFT.
        State new_state = (composition_state_to_add != Limits::max_state) ? composition_state_to_add : result.add_state_with_level(level);
        insert_to_product_storage(lhs_state, rhs_state, new_state);

        // If the level is zero, we need to check for final states and add to the worklist.
        if (level == 0) {
            // Because the key pair was not found in the map, we are sure that the state was not in the worklist yet either.
            worklist.push(key);
            if ((is_a_lhs && lhs.final.contains(state_a) && rhs.final.contains(state_b)) ||
                (!is_a_lhs && rhs.final.contains(state_a) && lhs.final.contains(state_b))) {
                result.final.insert(new_state);
            }
        }

        return new_state;
    };

    // Function: maps epsilon transitions on the sync path
    // TODO: Refactor - too many similarities in the code and with the code of map_combination_path.
    auto map_epsilon_on_sync_path = [&](const State res_root, const State orig_root, const State wait_root, const bool is_orig_lhs) {
        const Nft& orig_nft = is_orig_lhs ? lhs : rhs;
        const OrdVector<Level>& sync_levels = is_orig_lhs ? lhs_sync_levels : rhs_sync_levels;
        const std::vector<size_t>& sync_levels_inv = is_orig_lhs ? lhs_sync_levels_inv : rhs_sync_levels_inv;
        const std::vector<size_t>& between = is_orig_lhs ? rhs_between : lhs_between;   // Use rhs_between if we are in lhs NFT, and vice versa.
        const BoolVector& is_sync_level = is_orig_lhs ? lhs_is_sync_level : rhs_is_sync_level;
        const bool add_before = !is_orig_lhs;   // On each section within sync levels, lhs transitions go begore rhs transitions.

        // Worklist contains pairs of (state in the result NFT, and state in the original lhs/rhs NFT).
        std::stack<std::pair<State, State>> worklist;
        worklist.emplace(res_root, orig_root);
        StateSet visited;   // It is not necessary is the NFT has a valid structure (each cycle contains a zero-level state).
        visited.insert(orig_root);
        pred_map.clear();

        // TODO: Use more optimal date structure.
        // TODO: I can maybe use the main memory storage (matrix),
        //       I'll just look up the pair (root, orig_state). Maybe.
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
                res_src = add_transition(result, res_src, EPSILON, between[0], jump_mode, pred_map);
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
                    res_src = add_transition(result, res_src, EPSILON, between[sync_level_idx], jump_mode, pred_map);
                }
                // Add EPSILON transitions for those in lhs BEFORE first transition after the sync level (we are in rhs).
                if (add_before) {
                    res_src = add_transition(result, res_src, EPSILON, between[sync_level_idx + 1], jump_mode, pred_map);
                }

                if (!project_out_sync_levels) {
                    res_src = add_transition(result, res_src, EPSILON, 1, jump_mode, pred_map);
                }

                // Process targets of the epsilon transitions (this transition will be projected out).
                for (const State orig_tgt: epsilon_post_it->targets) {
                    if (orig_nft.levels[orig_tgt] == 0) {
                        if (orig_src == orig_root && orig_nft.num_of_levels > 1) {
                            // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                            // It has been already handled in the main loop.
                            continue;
                        }
                        // We are connecting to a zero-level state.
                        // Add EPSILON transitions for those in rhs AFTER this last transition (we are in lhs).
                        if (!add_before && between[num_of_sync_levels] != 0) {
                            const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
                            assert((result.levels[res_src] + between[num_of_sync_levels]) % result_num_of_levels == result.levels[res_tgt]);
                            add_transition_with_target(result, res_src, EPSILON, res_tgt, jump_mode, pred_map);
                            commited_states.insert(res_tgt);
                        } else {
                            // There is nothing to add.
                            // Try to find if there is already a mapping for this state.
                            const auto key = is_orig_lhs ? std::make_pair(orig_tgt, wait_root) : std::make_pair(wait_root, orig_tgt);
                            const State found_state = get_state_from_product_storage(key.first, key.second);
                            if (found_state != Limits::max_state) {
                                // The mapping exists, redirect transition goig to orig_src to the existing state.
                                redirect_transitions(result, res_src, found_state, pred_map);
                            } else {
                                assert(found_state == Limits::max_state);
                                // The mapping does not exist. Update it.
                                create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs, res_src);
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
                            if (orig_src == orig_root && symbol_post.symbol == EPSILON && orig_nft.num_of_levels > 1) {
                                // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                                // It has been already handled in the main loop.
                                continue;
                            }

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
                                    const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
                                    assert((result.levels[res_tgt_inner] + between[num_of_sync_levels]) % result_num_of_levels == result.levels[res_tgt]);
                                    add_transition_with_target(result, res_tgt_inner, EPSILON, res_tgt, jump_mode, pred_map);
                                    commited_states.insert(res_tgt);
                                }
                            } else {
                                assert(result.levels[res_src] + level_diff == result_num_of_levels);
                                const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
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

    // Maps result of a composition starting from zero-level states lhs_root and rhs_root.
    // This mapping ends in next zero-level states.
    auto map_combination_path = [&](const State res_root, const State lhs_root, const State rhs_root) {
        pred_map.clear();
        std::stack<std::tuple<State, State, State>> worklist;
        worklist.push({res_root, lhs_root, rhs_root});

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
                        if (symbol_post.symbol == EPSILON && lhs_src_level == 0 && lhs_tgt_level == 0 && lhs_num_of_levels > 1) {
                            // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                            // It has been already handled in the main loop.
                            continue;
                        }

                        if (lhs_tgt_level == 0) {
                            const size_t trans_len = lhs_num_of_levels - lhs_src_level;
                            // We are connecting to a zero-level state.
                            if (rhs_between[num_of_sync_levels] == 0) {
                                // There is no transition in rhs.
                                const State res_tgt = create_composition_state(lhs_tgt, rhs_src, 0);
                                result.delta.add(res_src, symbol_post.symbol, res_tgt);
                                // There are no unprocessed synchronization transitions, so we can commit the state.
                                // TODO: is it really necessary?
                                commited_states.insert(res_tgt);
                            } else {
                                // There is a transition in rhs.
                                const bool visited = get_state_from_product_storage(lhs_tgt, rhs_src) != Limits::max_state;
                                const State res_tgt = create_composition_state(lhs_tgt, rhs_src, trans_len + res_src_level);
                                result.delta.add(res_src, symbol_post.symbol, res_tgt);
                                pred_map[res_tgt].insert(res_src);
                                if (!visited) {
                                    // Not visited, add to the worklist
                                    worklist.push({res_tgt, lhs_tgt, rhs_src});
                                }
                            }
                        } else {
                            // We are connecting to a non-zero-level state.
                            const Level level_diff = lhs_tgt_level - lhs_src_level;
                            assert(res_src_level + level_diff <= result_num_of_levels);
                            bool visited = get_state_from_product_storage(lhs_tgt, rhs_src) != Limits::max_state;
                            const State res_tgt = create_composition_state(lhs_tgt, rhs_src, (res_src_level + level_diff) % result_num_of_levels);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            pred_map[res_tgt].insert(res_src);
                            if (!visited) {
                                // Not visited, add to the worklist
                                worklist.push({res_tgt, lhs_tgt, rhs_src});
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
                        if (symbol_post.symbol == EPSILON && rhs_src_level == 0 && rhs_tgt_level == 0 && rhs_num_of_levels > 1) {
                            // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                            // It has been already handled in the main loop.
                            continue;
                        }

                        if (rhs_tgt_level == 0) {
                            // We are connecting to a zero-level state.
                            const State res_tgt = create_composition_state(lhs_src, rhs_tgt, 0);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            commited_states.insert(res_tgt);
                        } else {
                            // We are connecting to a non-zero-level state.
                            const Level level_diff = rhs_tgt_level - rhs_src_level;
                            assert(res_src_level + level_diff <= result_num_of_levels);
                            const bool visited = get_state_from_product_storage(lhs_src, rhs_tgt) != Limits::max_state;
                            const State res_tgt = create_composition_state(lhs_src, rhs_tgt, (res_src_level + level_diff) % result_num_of_levels);
                            result.delta.add(res_src, symbol_post.symbol, res_tgt);
                            pred_map[res_tgt].insert(res_src);
                            if (!visited) {
                                // Not visited, add to the worklist
                                worklist.push({res_tgt, lhs_src, rhs_tgt});
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

            // Processes the intersection of transitions in lhs and rhs.
            auto process_intersection = [&](const StateSet& lhs_targets, const StateSet& rhs_targets, const Symbol symbol) {
                State local_res_src = res_src;
                if (!project_out_sync_levels && !lhs_targets.empty() && !rhs_targets.empty()) {
                    local_res_src = add_transition(result, local_res_src, symbol, 1, jump_mode, pred_map);
                }
                for (const State lhs_tgt: lhs_targets) {
                    const Level lhs_tgt_level = lhs.levels[lhs_tgt];
                    if (symbol == EPSILON && lhs_src_level == 0 && lhs_tgt_level == 0 && lhs_num_of_levels > 1) {
                        // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                        // It has been already handled in the main loop.
                        continue;
                    }
                    for (const State rhs_tgt: rhs_targets) {
                        const Level rhs_tgt_level = rhs.levels[rhs_tgt];
                        if (symbol == EPSILON && rhs_src_level == 0 && rhs_tgt_level == 0 && rhs_num_of_levels > 1) {
                            // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
                            // It has been already handled in the main loop.
                            continue;
                        }

                        if (lhs_tgt_level == 0 && rhs_tgt_level == 0) {
                            // We are connecting to a zero-level state.
                            const State found_state = get_state_from_product_storage(lhs_tgt, rhs_tgt);
                            if (found_state != Limits::max_state) {
                                // The mapping exists, redirect transition goig to orig_src to the existing state.
                                redirect_transitions(result, local_res_src, found_state, pred_map);
                            } else {
                                assert(found_state == Limits::max_state);
                                // The mapping does not exist. Update it.
                                create_composition_state(lhs_tgt, rhs_tgt, 0, true, local_res_src);
                                commited_states.insert(local_res_src);
                            }
                        } else {
                            if (get_state_from_product_storage(lhs_tgt, rhs_tgt) == Limits::max_state) {
                                // We have not visited this pair yet.
                                insert_to_product_storage(lhs_tgt, rhs_tgt, local_res_src);
                                worklist.push({local_res_src, lhs_tgt, rhs_tgt});
                            }
                        }
                    }
                }
            };

            // Process symbol intersection
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

            // Process DONT_CARE symbol in lhs.
            if (lhs.delta[lhs_src].find(DONT_CARE) != lhs.delta[lhs_src].end()) {
                for (const SymbolPost& symbol_post: rhs.delta[rhs_src]) {
                    process_intersection(
                        lhs.delta[lhs_src].find(DONT_CARE)->targets,
                        symbol_post.targets,
                        symbol_post.symbol
                    );
                }
            }

            // Process DONT_CARE symbol in rhs.
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

        // Perform waiting if necessary.
        if (perform_lhs_wait) {
            map_epsilon_on_sync_path(res_root, rhs_root, lhs_root, false);
        }
        if (perform_rhs_wait) {
            map_epsilon_on_sync_path(res_root, lhs_root, rhs_root, true);
        }
    };

    // INITIALIZATION on a worklist (side effect of a create_composition_state)
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            const State res_root = create_composition_state(lhs_root, rhs_root, 0);
            commited_states.insert(res_root);
            result.initial.insert(res_root);
        }
    }

    // MAIN LOOP
    while (!worklist.empty()) {
        auto [orig_lhs, orig_rhs] = worklist.top();
        worklist.pop();

        // Get the state in the result NFT
        const State res_root = get_state_from_product_storage(orig_lhs, orig_rhs);
        // TODO - is commited_states really necessary?
        if (!commited_states.contains(res_root)) {
            result.final.erase(res_root);
            continue;
        }

        // Process "long" EPSILON transitions leading from zero-level state to zero-level state.
        auto epsilon_post_a = lhs.delta[orig_lhs].find(EPSILON);
        if (epsilon_post_a != lhs.delta[orig_lhs].end()) {
            for (const State target: epsilon_post_a->targets) {
                if (lhs.levels[target] == 0) {
                    const State res_tgt = create_composition_state(target, orig_rhs, 0);
                    add_transition_with_target(result, res_root, EPSILON, res_tgt, jump_mode, pred_map);
                    // result.delta.add(res_root, EPSILON, res_tgt);
                    commited_states.insert(res_tgt);
                }
            }
        }
        auto epsilon_post_b = rhs.delta[orig_rhs].find(EPSILON);
        if (epsilon_post_b != rhs.delta[orig_rhs].end()) {
            for (const State target: epsilon_post_b->targets) {
                if (rhs.levels[target] == 0) {
                    const State res_tgt = create_composition_state(orig_lhs, target, 0);
                    add_transition_with_target(result, res_root, EPSILON, res_tgt, jump_mode, pred_map);
                    // result.delta.add(res_root, EPSILON, res_tgt);
                    commited_states.insert(res_tgt);
                }
            }
        }

        // Map path created by the composition of lhs and rhs NFTs.
        // Each iteration starts in zero-level states orig_lhs and orig_rhs
        // and ends in next zero-level states.
        map_combination_path(res_root, orig_lhs, orig_rhs);
    }

    return result.trim();
}

Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    if (jump_mode == JumpMode::NoJump) {
        // If we are not jumping, we can use the fast composition.
        return compose_fast(lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, true, jump_mode);
    }

    // TODO - add better unwinding
    // throw std::runtime_error("Noodler should not go here. Noodler should always use JumpMode::NoJump.");
    return compose_fast(lhs.unwind_jumps({ DONT_CARE }, jump_mode), rhs.unwind_jumps({ DONT_CARE }, jump_mode), lhs_sync_levels, rhs_sync_levels,  project_out_sync_levels, true, JumpMode::NoJump);
}

} // mata::nft
