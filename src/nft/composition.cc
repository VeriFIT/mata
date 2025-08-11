/* composition.cc -- Composition of two NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include <cassert>
#include <numeric>


using namespace mata::utils;


namespace {
    using mata::Symbol;
    using namespace mata::nft;
    using PredMap = std::unordered_map<State, StateSet>;

    using ProductMap = std::unordered_map<std::pair<State,State>,State>;
    using MatrixProductStorage = std::vector<std::vector<State>>;
    using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
    using InvertedProductStorage = std::vector<State>;

    /**
     * Create a mask from the given entries and universum size.
     * The mask will have `true` for each entry in @p entries and `false` for all other indices.
     *
     * @param entries The entries to be set in the mask.
     * @param universum_size The size of the universum.
     *
     * @return A BoolVector representing the mask.
     */
    template<typename T>
    mata::BoolVector create_mask(const OrdVector<T>& entries, const size_t universum_size) {
        assert(entries.empty() || entries.back() < universum_size);
        mata::BoolVector mask(universum_size, false);
        for (const auto& entry : entries) {
            mask[entry] = true;
        }
        return mask;
    }

    /**
     * Get a vector of indices for the entries in the given OrdVector.
     * This is useful for quickly finding the position of an entry in the OrdVector.
     *
     * @param entries The OrdVector containing the entries.
     * @param universum_size The size of the universum.
     *
     * @return A vector of indices where each index corresponds to the position of the entry in @p entries.
     *         If an entry is not present, its index will be set to `std::numeric_limits<T>::max()`.
     */
    template<typename T>
    std::vector<size_t> get_entry_indices_vec(const OrdVector<T>& entries, const size_t universum_size) {
        assert(entries.empty() || entries.back() < universum_size);
        std::vector<size_t> entry_indices_vec(universum_size, std::numeric_limits<T>::max());
        size_t idx = 0;
        for (const auto& entry : entries) {
            entry_indices_vec[entry] = idx++;
        }
        return entry_indices_vec;
    }

    Level get_last_useful_level(const Nft& nft, const mata::BoolVector& is_sync_level) {
        assert(std::any_of(is_sync_level.begin(), is_sync_level.end(), [](bool val) { return val; }));
        Level last_useful_level = 0;
        const size_t num_of_levels = is_sync_level.size();
        for (Level level = 0; level < num_of_levels; ++level) {
            if (!is_sync_level[level]) {
                last_useful_level = level;
            }
        }
        return last_useful_level;
    }

    /**
     * Get the sizes of intervals between borders in the given OrdVector. The border on index 0 is before
     * the first element, the border on index 1 is after the first element, and so on.
     *
     * @param border_indices The OrdVector containing the indices of the borders.
     * @param universum_size The size of the universum.
     *
     * @return A vector containing the sizes of intervals between the entries in @p border_indices.
     */
    template<typename T>
    std::vector<size_t> get_interval_sizes(const OrdVector<T>& border_indices, const size_t universum_size) {
        assert(!border_indices.empty());
        const size_t num_of_intervals = border_indices.size() + 1;
        std::vector<size_t> sizes(num_of_intervals, 0);
        auto border_indices_it = border_indices.cbegin();

        sizes[0] = *border_indices_it;
        const size_t last_interval_idx = num_of_intervals - 1;
        for (size_t i = 1; i < last_interval_idx; ++i) {
            const size_t prev_border = *border_indices_it;
            ++border_indices_it;
            sizes[i] = *border_indices_it - prev_border - 1;
        }
        sizes[num_of_intervals - 1] = universum_size - *border_indices_it - 1;

        return sizes;
    }

    /** Add a transition to the NFT from source state over symbol and length len to a new target state.
     *
     * @param nft The NFT to which the transition will be added.
     * @param source The source state of the transition.
     * @param symbol The symbol of the transition.
     * @param len The length of the transition.
     * @param jump_mode The mode of the jump.
     * @param pred_map A map to keep track of predecessors for each target state.
     *
     * @return The target state of the transition.
     */
    State add_transition(Nft& nft, const State source, const Symbol symbol, const size_t len, const JumpMode jump_mode){//, PredMap& pred_map) {
        if (len == 0) { return source; }

        assert(nft.levels[source] + len <= nft.num_of_levels);
        const Level target_level = static_cast<Level>((nft.levels[source] + len) % nft.num_of_levels);
        const State target = nft.add_state_with_level(target_level);

        if (len == 1 || jump_mode == JumpMode::RepeatSymbol) {
            nft.delta.add(source, symbol, target);
            // pred_map[target].insert(source);
            return target;
        }

        State inner_src = source;
        Level inner_level = nft.levels[inner_src] + 1;
        for (size_t i = 0; i < len - 1; ++i, ++inner_level) {
            assert(inner_level < nft.num_of_levels);
            const State inner_target = nft.add_state_with_level(inner_level);
            nft.delta.add(inner_src, symbol, inner_target);
            // pred_map[inner_target].insert(inner_src);    // Optimization: we do not need to track inner states in pred_map.
            inner_src = inner_target;
        }
        assert(inner_level == nft.levels[source] + len);
        nft.delta.add(inner_src, symbol, target);
        // pred_map[target].insert(inner_src);

        return target;
    }

    /**
     * Add a transition from source state to target state with the given symbol.
     *
     * @param nft The NFT to which the transition will be added.
     * @param source The source state of the transition.
     * @param symbol The symbol of the transition.
     * @param target The target state of the transition.
     * @param jump_mode The mode of the jump transitions.
     * @param pred_map A map to keep track of predecessors for each target state.
     */
    void add_transition_with_target(Nft& nft, const State source, const Symbol symbol, const State target, const JumpMode jump_mode){//, PredMap& pred_map) {
        if (source == target) { return; }

        assert(nft.levels[source] < nft.levels[target] || nft.levels[target] == 0);
        const size_t trans_len = (nft.levels[target] == 0 ? nft.num_of_levels : nft.levels[target]) - nft.levels[source];
        assert(trans_len > 0);

        if (trans_len == 1 || jump_mode == JumpMode::RepeatSymbol) {
            nft.delta.add(source, symbol, target);
            // pred_map[target].insert(source);
            return;
        }

        State inner_src = source;
        Level inner_level = nft.levels[inner_src] + 1;
        for (size_t i = 0; i < trans_len - 1; ++i, ++inner_level) {
            assert(inner_level < nft.num_of_levels);
            const State inner_target = nft.add_state_with_level(inner_level);
            nft.delta.add(inner_src, symbol, inner_target);
            // pred_map[inner_target].insert(inner_src);    // Optimization: we do not need to track inner states in pred_map.
            inner_src = inner_target;
        }
        assert(inner_level == nft.levels[source] + trans_len);
        nft.delta.add(inner_src, symbol, target);
        // pred_map[target].insert(inner_src);
    }

    /** Duplicate transitions that lead to the old target state and map them to the new target state.
     *
     * @param nft The NFT in which the transitions will be duplicated.
     * @param old_target The original target state of the transitions.
     * @param new_target The new target state to which the transitions will be duplicated.
     * @param pred_map A map that keeps track of predecessors for each target state.
     */
    void duplicate_transitions(Nft& nft, const State old_target, const State new_target, const PredMap& pred_map) {
        assert(nft.levels[old_target] == nft.levels[new_target]);
        if (old_target == new_target) { return; }

        // NODE: The state new_target is a "product-like" state, so if old_target is a final state,
        //       than new_target should already be a final state.
        // if (nft.final.contains(old_target)) {
        //     nft.final.insert(new_target);
        // }

        // NODE: Do we need this? Isn't it the same as the previous check?
        //       Also, how does the state become initial? Does both states have to be initial?
        // if (nft.initial.contains(old_target)) {
        //     nft.initial.insert(new_target);
        // }

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

    StateSet get_epsilon_on_sync_levels_succ(const Nft& nft, const State source, const mata::BoolVector& is_sync_level) {
        StateSet succ;
        std::queue<State> worklist;
        worklist.push(source);
        while (!worklist.empty()) {
            const State current = worklist.front();
            const Level current_level = nft.levels[current];
            worklist.pop();
            for (const SymbolPost& symbol_post: nft.delta[current]) {
                if (is_sync_level[current_level] && symbol_post.symbol != EPSILON) {
                    continue; // Skip non-EPSILON transitions on sync levels.
                }
                for (const State target: symbol_post.targets) {
                    const Level target_level = nft.levels[target];
                    // We suppose that there are no cycles between zero levels.
                    assert(target_level == 0 || current_level < target_level);
                    if (target_level == 0) {
                        succ.insert(target);
                    } else {
                        worklist.push(target);
                    }
                }
            }
        }

        return succ;
    }

    StateSet get_epsilon_succ(const Nft& nft, const State source) {
        if (nft.levels[source] == 0) {
            return { source }; // If the source is a zero-level state, return it as the only successor.
        }

        StateSet succ;
        std::queue<State> worklist;
        worklist.push(source);
        while (!worklist.empty()) {
            const State current = worklist.front();
            const Level current_level = nft.levels[current];
            worklist.pop();

            for (const SymbolPost& symbol_post: nft.delta[current]) {
                if (symbol_post.symbol != EPSILON) {
                    continue; // Skip non-EPSILON transitions.
                }
                for (const State target: symbol_post.targets) {
                    const Level target_level = nft.levels[target];
                    // We suppose that there are no cycles between zero levels.
                    assert(target_level == 0 || current_level < target_level);
                    if (target_level == 0) {
                        succ.insert(target);
                    } else {
                        worklist.push(target);
                    }
                }
            }
        }

        return succ;
    }

    StateSet get_sync_succ(const Nft& lhs, const Nft& rhs, const State lhs_state, const State rhs_state, bool& is_lhs_waiting, bool& is_rhs_waiting) {
        if (lhs.levels[lhs_state] == 0 && rhs.levels[rhs_state] == 0) {
            return { lhs_state, rhs_state }; // If both states are zero-level, return them as successors.
        }
        assert(lhs.levels[lhs_state] != 0 && rhs.levels[rhs_state] != 0);
        StateSet succ;
        
    }
                           
}

// TODO
// Use dequeue with nonzero state pairs on the left and pairs on zero levels on the right.
// At the end ack if need to add a waiting cycle in lhs or rhs.
// Using get_epsilon_on_sync_levels_succ() to get successors that are going to be paired with the other.
// Potentialy there is no need to manualy do the nonwaiting path, maybe we dont even need the get_epsilonb_on_sync_level_succ function.
// Just create waiting loop and then try to do it again. Because pairs that does not lead anywere weare already visided, we will be foreced to go
// through the waiting loop. Although, there can be a problem with the memory storage. We can not reshape it during the computation.
// No, we can not use those two functions. We have to do the wait in place. Meaning that the function get_epsilon_on_sync_levels_succ() is goig to need
// an access to the memory storage and create the path.
// We need to calculate the last level to avoid usage of pred_map.





namespace mata::nft
{

// TODO: Refactor this function to be more readable and maintainable.
//       There is a shitton of code duplication in this function.
Nft compose_fast(const Nft& lhs, const Nft& rhs, const utils::OrdVector<Level>& lhs_sync_levels, const utils::OrdVector<Level>& rhs_sync_levels, const bool project_out_sync_levels, const bool are_sync_levels_unwinded, const JumpMode jump_mode) {
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());
    assert(lhs_sync_levels.size() < lhs.num_of_levels && rhs_sync_levels.size() < rhs.num_of_levels);

    // Number of Levels and States
    const size_t num_of_sync_levels = lhs_sync_levels.size();
    const size_t lhs_num_of_levels = lhs.num_of_levels;
    const size_t rhs_num_of_levels = rhs.num_of_levels;
    const size_t lhs_num_of_states = lhs.num_of_states();
    const size_t rhs_num_of_states = rhs.num_of_states();
    const size_t result_num_of_levels = lhs_num_of_levels + rhs_num_of_levels - (project_out_sync_levels ? (2 * num_of_sync_levels) : num_of_sync_levels);

    // FAST STORAGE OF COMPOSITION STATES
    // The largest matrix of pairs of states we are brave enough to allocate.
    // Let's say we are fine with allocating larget_composition * (about 8 Bytes) space.
    // So ten million cells is close to 100 MB.
    // If the number is larger, then we do not allocate a matrix, but use a vector of unordered maps.
    // The unordered_map seems to be about twice slower.
    constexpr size_t MAX_COMPOSITION_MATRIX_SIZE = 50'000'000;
    // constexpr size_t MAX_COMPOSITION_MATRIX_SIZE = 0;
    const bool larget_composition = lhs_num_of_states * rhs_num_of_states > MAX_COMPOSITION_MATRIX_SIZE;
    assert(lhs_num_of_states < Limits::max_state);
    assert(rhs_num_of_states < Limits::max_state);

    // Two variants of storage for the mapping from pairs of lhs and rhs states to composition state, for large and non-large products.
    MatrixProductStorage matrix_composition_storage;
    VecMapProductStorage vec_map_composition_storage;
    InvertedProductStorage composition_to_lhs(lhs_num_of_states + rhs_num_of_states);
    InvertedProductStorage composition_to_rhs(lhs_num_of_states + rhs_num_of_states);

    // Initialize the storage, according to the number of possible state pairs.
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


    Nft result;
    result.num_of_levels = result_num_of_levels;
    std::deque<std::pair<State, State>> worklist;

    // Calculate number of lhs/rhs transitions before/between/after sync levels
    const std::vector<size_t> lhs_interleave = get_interval_sizes(lhs_sync_levels, lhs_num_of_levels);
    const std::vector<size_t> rhs_interleave = get_interval_sizes(rhs_sync_levels, rhs_num_of_levels);

    BoolVector lhs_is_sync_level_v = create_mask(lhs_sync_levels, lhs_num_of_levels);
    BoolVector rhs_is_sync_level_v = create_mask(rhs_sync_levels, rhs_num_of_levels);

    const std::vector<size_t> lhs_sync_levels_inv = get_entry_indices_vec(lhs_sync_levels, lhs_num_of_levels);
    const std::vector<size_t> rhs_sync_levels_inv = get_entry_indices_vec(rhs_sync_levels, rhs_num_of_levels);

    const Level lhs_last_nonsync_level = get_last_useful_level(lhs, lhs_is_sync_level_v);
    const Level rhs_last_nonsync_level = get_last_useful_level(rhs, rhs_is_sync_level_v);

    /**
     * @brief Create a new composition state for the given pair of states, if it does not already exist.
     *
     * @param state_a The first state (lhs or rhs).
     * @param state_b The second state (rhs or lhs).
     * @param level The level of the new composition state.
     * @param is_a_lhs If true, state_a is from the lhs NFT, otherwise it is from the rhs NFT.
     * @param composition_state_to_add If provided, this is the state to be added to the result NFT.
     *                                 If not provided, a new state will be created.
     *
     * @return The composition state for the given pair of states.
     */
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
            worklist.push_back(key);
            if ((is_a_lhs && lhs.final.contains(state_a) && rhs.final.contains(state_b)) ||
                (!is_a_lhs && rhs.final.contains(state_a) && lhs.final.contains(state_b))) {
                result.final.insert(new_state);
            }
        } else {
            worklist.push_front(key); // Push to the front for non-zero levels.
        }

        return new_state;
    };

    /**
     * @brief Model the waiting in the waiting loop in one of transducers.
     *
     * @param run_nft The NFT that is being run (the one that is not waiting).
     * @param wait_zero_state The zero state of the waiting NFT.
     * @param run_zero_state The zero state of the running NFT.
     * @param composition_zero_state The zero state of the composition NFT.
     * @param run_is_sync_level The mask of sync levels in the running NFT.
     * @param run_sync_levels_inv The inverted indices of sync levels in the running NFT.
     * @param run_last_nonsync_level The last non-sync level in the running NFT.
     * @param interleave The interleave vector that defines the order of levels in the composition NFT.
     * @param is_wait_lhs If true, the waiting NFT is the left one (lhs), otherwise it is the right one (rhs).
     */
    auto model_waiting = [&](const Nft&                     run_nft,
                             const State                    wait_zero_state,
                             const State                    run_zero_state,
                             const State                    composition_zero_state,
                             const BoolVector&              run_is_sync_level,
                             const std::vector<State>&      run_sync_levels_inv,
                             const Level                    run_last_nonsync_level,
                             const std::vector<size_t>&     interleave,
                             const bool                     is_wait_lhs) {

        const bool interleave_before = is_wait_lhs;

        std::stack<std::pair<State, State>> worklist;
        worklist.push({ run_zero_state, composition_zero_state });
        while (!worklist.empty()) {
            auto [run_state, composition_state] = worklist.top();
            const Level run_state_level = run_nft.levels[run_state];
            const Level composition_state_level = result.levels[composition_state];
            const bool is_sync_level = run_is_sync_level[run_state_level];
            worklist.pop();

            // Add epsilon transition for the transitions in the waiting NFT.
            if (interleave_before && run_state_level == 0 && interleave.at(0) != 0) {
                // Insert before the first section in the run NFT.
                composition_state = add_transition(result, composition_state, EPSILON, interleave.at(0), jump_mode);
            } else if (is_sync_level) {
                // Insert before or right after the sync level in the run NFT.
                const size_t idx_interleave = interleave_before ? run_sync_levels_inv[run_state_level] + 1 : run_sync_levels_inv[run_state_level];
                composition_state = add_transition(result, composition_state, EPSILON, interleave.at(idx_interleave), jump_mode);
            }

            // Process transitions from the run NFT.
            for (const SymbolPost& run_symbol_post: run_nft.delta[run_state]) {
                if (is_sync_level && run_symbol_post.symbol != EPSILON) {
                    continue; // Skip non-EPSILON transitions on sync levels.
                }
                for (const State run_target: run_symbol_post.targets) {
                    const Level run_target_level = run_nft.levels[run_target];
                    const size_t transition_len = (run_target_level == 0 ? run_nft.num_of_levels : run_target_level) - run_state_level;
                    assert(run_target_level == 0 || run_state_level < run_target_level);
                    assert(transition_len > 0);

                    const bool is_last_useful_transition = run_state_level <= run_last_nonsync_level && (run_target_level == 0 || run_target_level > run_last_nonsync_level);
                    if (is_last_useful_transition) {
                        // After this transiton, there can only be epsilon transitions from the wait NFT.
                        const size_t next_interleave_level_idx = (run_target_level == 0 ? (interleave.size() - 1) : run_sync_levels_inv[run_target_level]) + (interleave_before ? 1 : 0);
                        const size_t levels_to_add = std::accumulate(interleave.begin() + next_interleave_level_idx, interleave.end(), 0);

                        if (levels_to_add == 0) {
                            // There are not epsilon transitions to add connect directly.
                            for (const State run_zero_target: get_epsilon_succ(run_nft, run_target)) {
                                const State composition_target_state = create_composition_state(wait_zero_state, run_zero_target, 0, is_wait_lhs);
                                add_transition_with_target(result, composition_state, run_symbol_post.symbol, composition_target_state, jump_mode);
                            }
                        } else {
                            // There are epsilon transitions to add, so we need to first create an inner state.
                            const State composition_inner_state = add_transition(result, composition_state, run_symbol_post.symbol, transition_len, jump_mode);
                            for (const State run_zero_target: get_epsilon_succ(run_nft, run_target)) {
                                const State composition_target_state = create_composition_state(wait_zero_state, run_zero_target, 0, is_wait_lhs);
                                add_transition_with_target(result, composition_inner_state, EPSILON, composition_target_state, jump_mode);
                            }
                        }
                    } else if (is_sync_level) {
                        // This is a sync level transition, after which there will be at least one useful transition in the run NFT.
                        worklist.push({ run_target, composition_state });

                    } else {
                        // This is a non-sync level transition after which there is more transitions in the run NFT.
                        assert(composition_state_level + transition_len < result.num_of_levels);
                        const State composition_target_state = result.add_state_with_level(composition_state_level + transition_len);
                        add_transition_with_target(result, composition_state, run_symbol_post.symbol, composition_target_state, jump_mode);
                        worklist.push({ run_target, composition_target_state });
                    }
                }
            }
        }
    };

    // INITIALIZATION on a worklist (side effect of a create_composition_state)
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            const State res_root = create_composition_state(lhs_root, rhs_root, 0);
            result.initial.insert(res_root);
        }
    }

    // Main Loop
    bool lhs_waiting_cycle = false;
    bool rhs_waiting_cycle = false;

    auto get_sync_succ = [&](const State lhs_state, const State rhs_state) {
        if (lhs.levels[lhs_state] == 0 && rhs.levels[rhs_state] == 0) {
            return StateSet{ lhs_state, rhs_state }; // If both states are zero-level, return them as successors.
        }

        StateSet succ;
        std::queue<std::pair<State, State>> worklist;
        worklist.push({ lhs_state, rhs_state });

        while (!worklist.empty()) {
            const auto [lhs_source, rhs_source] = worklist.front();
            worklist.pop();
            assert(lhs.levels[lhs_source] != 0 && rhs.levels[rhs_source] != 0);

            // Test if we need to wait later.
            const bool lhs_state_post_contains_epsilon = lhs.delta[lhs_source].find(EPSILON) != lhs.delta[lhs_source].end();
            const bool rhs_state_post_contains_epsilon = rhs.delta[lhs_source].find(EPSILON) != rhs.delta[lhs_source].end();
            if (rhs_state_post_contains_epsilon && !lhs_state_post_contains_epsilon) {
                lhs_waiting_cycle = true;
            }
            if (lhs_state_post_contains_epsilon && !rhs_state_post_contains_epsilon) {
                rhs_waiting_cycle = true;
            }

            mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
            mata::utils::push_back(sync_iterator, lhs.delta[lhs_source]);
            mata::utils::push_back(sync_iterator, rhs.delta[rhs_source]);
            while (sync_iterator.advance()) {
                const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
                assert(same_symbol_posts.size() == 2); // One move per state in the pair.
                for (const State lhs_target: same_symbol_posts[0]->targets) {
                    const Level lhs_target_level = lhs.levels[lhs_target];
                    assert(lhs_target_level == 0 || lhs.levels[lhs_source] < lhs_target_level);
                    for (const State rhs_target: same_symbol_posts[1]->targets) {
                        const Level rhs_target_level = rhs.levels[rhs_target];
                        assert(rhs_target_level == 0 || rhs.levels[rhs_source] < rhs_target_level);
                        assert((lhs_target_level != 0 && rhs_target_level != 0) || (lhs_target_level == 0 && rhs_target_level == 0));
                        if (lhs_target_level == 0 && rhs_target_level == 0) {
                            succ.insert(create_composition_state(lhs_target, rhs_target, 0, true));
                        } else {
                            worklist.push({ lhs_target, rhs_target });
                        }
                    }
                }
            }
        }
    };


    State last_zero_level_composition_state = Limits::max_state;
    while (!worklist.empty()) {
        const auto [lhs_state, rhs_state] = worklist.front();
        const State composition_state = get_state_from_product_storage(lhs_state, rhs_state);
        const Level composition_state_level = result.levels[composition_state];
        assert(composition_state != Limits::max_state); // The state should always be found in the product storage.
        worklist.pop_front();
        
        const StatePost& lhs_state_post = lhs.delta[lhs_state];
        const StatePost& rhs_state_post = rhs.delta[rhs_state];
        const Level lhs_state_level = lhs.levels[lhs_state];
        const Level rhs_state_level = rhs.levels[rhs_state];
        const bool lhs_is_sync_level = lhs_is_sync_level_v[lhs_state_level];
        const bool rhs_is_sync_level = rhs_is_sync_level_v[rhs_state_level];
        const bool lhs_after_last_nonsync_level = lhs_state_level > lhs_last_nonsync_level;
        const bool rhs_after_last_nonsync_level = rhs_state_level > rhs_last_nonsync_level;

        // TODO: Move somewhere else.
        // Because non-zero levels are processed first, we can reset wariables when we reach a zero level.
        if (composition_state_level == 0) {
            if (lhs_waiting_cycle) {
                assert(last_zero_level_composition_state != Limits::max_state);
                const State lhs_wait_state = composition_to_lhs[last_zero_level_composition_state];
                const State rhs_run_state = composition_to_rhs[last_zero_level_composition_state];
                model_waiting(rhs, lhs_wait_state, rhs_run_state, composition_state, rhs_is_sync_level_v, rhs_sync_levels_inv, rhs_last_nonsync_level, lhs_interleave, true);
            }
            if (rhs_waiting_cycle) {
                assert(last_zero_level_composition_state != Limits::max_state);
                const State lhs_run_state = composition_to_lhs[last_zero_level_composition_state];
                const State rhs_wait_state = composition_to_rhs[last_zero_level_composition_state];
                model_waiting(lhs, rhs_wait_state, lhs_run_state, composition_state, lhs_is_sync_level_v, lhs_sync_levels_inv, lhs_last_nonsync_level, rhs_interleave, false);
            }
            lhs_waiting_cycle = false;
            rhs_waiting_cycle = false;
            last_zero_level_composition_state = composition_state;
        }

        
        if (!lhs_is_sync_level) {
            assert(!lhs_after_last_nonsync_level);
            // We need to go deeper in the lhs NFT.                
            for (const SymbolPost& lhs_symbol_post: lhs_state_post) {
                for (const State lhs_target: lhs_symbol_post.targets) {
                    const Level lhs_target_level = lhs.levels[lhs_target];
                    const size_t transition_len = (lhs_target_level == 0 ? lhs.num_of_levels : lhs_target_level) - lhs_state_level;
                    assert(lhs_target_level == 0 || lhs_state_level < lhs_target_level);
                    assert(transition_len > 0);
                    
                    const bool is_last_useful_transition = lhs_state_level <= lhs_last_nonsync_level && (lhs_target_level == 0 || lhs_target_level > lhs_last_nonsync_level);
                    if (rhs_after_last_nonsync_level && is_last_useful_transition) {
                        // We have to be careful because there are only sync level in the rhs NFT.
                        // This is last meaningful transition after which we only check for synchronization and connect to those targets.
                        for (const State composition_target: get_sync_succ(lhs_target, rhs_state)) {
                            // We can not use the create_composition_state here, because it will add the state to the worklist.
                            // Instead, we will just add a transition to the composition NFT.
                            assert(composition_state_level + transition_len == result.num_of_levels);
                            add_transition_with_target(result, composition_state, lhs_symbol_post.symbol, composition_target, jump_mode);
                        }
                    } else {
                        // This is a non-sync level and also not the last useful transition.
                        // We will just copy it to the composition NFT.
                        assert(composition_state_level + transition_len < result.num_of_levels);
                        const State composition_target_state = create_composition_state(lhs_target, rhs_state, composition_state_level + transition_len, true);
                        add_transition_with_target(result, composition_state, lhs_symbol_post.symbol, composition_target_state, jump_mode);
                    }
                }
            }
        } else if (!rhs_is_sync_level) {
            // We need to go deeper in the rhs NFT.
            assert(!rhs_after_last_nonsync_level);
            for (const SymbolPost& rhs_symbol_post: rhs_state_post) {
                for (const State rhs_target: rhs_symbol_post.targets) {
                    const Level rhs_target_level = rhs.levels[rhs_target];
                    const size_t transition_len = (rhs_target_level == 0 ? rhs.num_of_levels : rhs_target_level) - rhs_state_level;
                    assert(rhs_target_level == 0 || rhs_state_level < rhs_target_level);
                    assert(transition_len > 0);

                    const bool is_last_useful_transition = rhs_state_level <= rhs_last_nonsync_level && (rhs_target_level == 0 || rhs_target_level > rhs_last_nonsync_level);
                    if (lhs_after_last_nonsync_level && is_last_useful_transition) {
                        // We have to be careful because there are only sync level in the lhs NFT.
                        // This is last meaningful transition after which we only check for synchronization and connect to those targets
                        for (const State composition_target: get_sync_succ(lhs_state, rhs_target)) {
                            // We can not use the create_composition_state here, because it will add the state to the worklist.
                            // Instead, we will just add a transition to the composition NFT.
                            assert(composition_state_level + transition_len == result.num_of_levels);
                            add_transition_with_target(result, composition_state, rhs_symbol_post.symbol, composition_target, jump_mode);
                        }
                    } else {
                        // This is a non-sync level and also not the last useful transition.
                        // We will just copy it to the composition NFT.
                        assert(composition_state_level + transition_len < result.num_of_levels);
                        const State composition_target_state = create_composition_state(lhs_state, rhs_target, composition_state_level + transition_len, false);
                        add_transition_with_target(result, composition_state, rhs_symbol_post.symbol, composition_target_state, jump_mode);
                    }
                }
            }
        } else {
            // Both NFTs are on sync levels, we need to check for synchronization.
            const auto lhs_state_post_end = lhs_state_post.end();
            const auto rhs_state_post_end = rhs_state_post.end();
            const bool lhs_state_post_contains_epsilon = lhs_state_post.find(EPSILON) != lhs_state_post_end;
            const bool rhs_state_post_contains_epsilon = rhs_state_post.find(EPSILON) != rhs_state_post_end;
            if (rhs_state_post_contains_epsilon && !lhs_state_post_contains_epsilon) {
                lhs_waiting_cycle = true;
            }
            if (lhs_state_post_contains_epsilon && !rhs_state_post_contains_epsilon) {
                rhs_waiting_cycle = true;
            }
            mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
            mata::utils::push_back(sync_iterator, lhs.delta[lhs_state]);
            mata::utils::push_back(sync_iterator, rhs.delta[rhs_state]);
            while (sync_iterator.advance()) {
                const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
                assert(same_symbol_posts.size() == 2); // One move per state in the pair.
                for (const State lhs_target: same_symbol_posts[0]->targets) {
                    for (const State rhs_target: same_symbol_posts[1]->targets) {
                        create_composition_state(lhs_target, rhs_target, composition_state_level, true, composition_state);
                    }
                }
            }
        }

    }


    // // Initialize the result NFT, state map, and worklist
    // Nft result;
    // result.num_of_levels = result_num_of_levels;
    // std::unordered_map<State, StateSet> pred_map;
    // std::stack<std::pair<State, State>> worklist;
    // std::unordered_set<State> commited_states;

    // // Calculate number of lhs/rhs transitions before/between/after sync levels
    // const std::vector<size_t> lhs_between = get_interval_sizes(lhs_sync_levels, lhs_num_of_levels);
    // const std::vector<size_t> rhs_between = get_interval_sizes(rhs_sync_levels, rhs_num_of_levels);

    // BoolVector lhs_is_sync_level = create_mask(lhs_sync_levels, lhs_num_of_levels);
    // BoolVector rhs_is_sync_level = create_mask(rhs_sync_levels, rhs_num_of_levels);

    // const std::vector<size_t> lhs_sync_levels_inv = get_entry_indices_vec(lhs_sync_levels, lhs_num_of_levels);
    // const std::vector<size_t> rhs_sync_levels_inv = get_entry_indices_vec(rhs_sync_levels, rhs_num_of_levels);

    // auto create_composition_state = [&](const State state_a, const State state_b, const Level level, const bool is_a_lhs = true, const State composition_state_to_add = Limits::max_state) {
    //     assert(composition_state_to_add == Limits::max_state || result.levels[composition_state_to_add] == level);

    //     // Try to find the entry in the state map.
    //     const auto key = is_a_lhs ? std::make_pair(state_a, state_b) : std::make_pair(state_b, state_a);
    //     const State lhs_state = key.first;
    //     const State rhs_state = key.second;
    //     const State found_state = get_state_from_product_storage(lhs_state, rhs_state);
    //     if (found_state != Limits::max_state) {
    //         assert(found_state == composition_state_to_add || composition_state_to_add == Limits::max_state);
    //         return found_state;
    //     }

    //     // If not found, add a new state to the result NFT.
    //     State new_state = (composition_state_to_add != Limits::max_state) ? composition_state_to_add : result.add_state_with_level(level);
    //     insert_to_product_storage(lhs_state, rhs_state, new_state);

    //     // If the level is zero, we need to check for final states and add to the worklist.
    //     if (level == 0) {
    //         // Because the key pair was not found in the map, we are sure that the state was not in the worklist yet either.
    //         worklist.push(key);
    //         if ((is_a_lhs && lhs.final.contains(state_a) && rhs.final.contains(state_b)) ||
    //             (!is_a_lhs && rhs.final.contains(state_a) && lhs.final.contains(state_b))) {
    //             result.final.insert(new_state);
    //         }
    //     }

    //     return new_state;
    // };

    // // Function: maps epsilon transitions on the sync path
    // // TODO: Refactor - too many similarities in the code and with the code of map_combination_path.
    // auto map_epsilon_on_sync_path = [&](const State res_root, const State orig_root, const State wait_root, const bool is_orig_lhs) {
    //     const Nft& orig_nft = is_orig_lhs ? lhs : rhs;
    //     const OrdVector<Level>& sync_levels = is_orig_lhs ? lhs_sync_levels : rhs_sync_levels;
    //     const std::vector<size_t>& sync_levels_inv = is_orig_lhs ? lhs_sync_levels_inv : rhs_sync_levels_inv;
    //     const std::vector<size_t>& between = is_orig_lhs ? rhs_between : lhs_between;   // Use rhs_between if we are in lhs NFT, and vice versa.
    //     const BoolVector& is_sync_level = is_orig_lhs ? lhs_is_sync_level : rhs_is_sync_level;
    //     const bool add_before = !is_orig_lhs;   // On each section within sync levels, lhs transitions go begore rhs transitions.

    //     // Worklist contains pairs of (state in the result NFT, and state in the original lhs/rhs NFT).
    //     std::stack<std::pair<State, State>> worklist;
    //     worklist.emplace(res_root, orig_root);
    //     StateSet visited;   // It is not necessary is the NFT has a valid structure (each cycle contains a zero-level state).
    //     visited.insert(orig_root);
    //     pred_map.clear();

    //     // TODO: Use more optimal date structure.
    //     // TODO: I can maybe use the main memory storage (matrix),
    //     //       I'll just look up the pair (root, orig_state). Maybe.
    //     std::unordered_map<State, State> local_state_map;
    //     auto get_state = [&](const State orig_state, const Level level) {
    //         // Get the state from the map or add a new one.
    //         auto it = local_state_map.find(orig_state);
    //         if (it != local_state_map.end()) {
    //             assert(result.levels[it->second] == level);
    //             return it->second;
    //         }
    //         const State new_state = result.add_state_with_level(level);
    //         local_state_map[orig_state] = new_state;
    //         return new_state;
    //     };

    //     while (!worklist.empty()) {
    //         auto [res_src, orig_src] = worklist.top();
    //         worklist.pop();

    //         // Add EPSILON transition for those in lhs BEFORE the first transition (we are in rhs).
    //         if (res_src == res_root && add_before) {
    //             res_src = add_transition(result, res_src, EPSILON, between[0], jump_mode, pred_map);
    //         }

    //         if (is_sync_level[orig_nft.levels[orig_src]]) {
    //             // We are at a sync level. We need to match only the epsilon transitions.
    //             const auto epsilon_post_it = orig_nft.delta[orig_src].find(EPSILON);
    //             if (epsilon_post_it == orig_nft.delta[orig_src].end()) {
    //                 continue;
    //             }
    //             const size_t sync_level_idx = sync_levels_inv[orig_nft.levels[orig_src]];
    //             // Add EPSILON transitions for those in rhs AFTER this last transition before the sync level (we are in lhs).
    //             if (!add_before) {
    //                 res_src = add_transition(result, res_src, EPSILON, between[sync_level_idx], jump_mode, pred_map);
    //             }
    //             // Add EPSILON transitions for those in lhs BEFORE first transition after the sync level (we are in rhs).
    //             if (add_before) {
    //                 res_src = add_transition(result, res_src, EPSILON, between[sync_level_idx + 1], jump_mode, pred_map);
    //             }

    //             if (!project_out_sync_levels) {
    //                 res_src = add_transition(result, res_src, EPSILON, 1, jump_mode, pred_map);
    //             }

    //             // Process targets of the epsilon transitions (this transition will be projected out).
    //             for (const State orig_tgt: epsilon_post_it->targets) {
    //                 if (orig_nft.levels[orig_tgt] == 0) {
    //                     if (orig_src == orig_root && orig_nft.num_of_levels > 1) {
    //                         // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                         // It has been already handled in the main loop.
    //                         continue;
    //                     }
    //                     // We are connecting to a zero-level state.
    //                     // Add EPSILON transitions for those in rhs AFTER this last transition (we are in lhs).
    //                     if (!add_before && between[num_of_sync_levels] != 0) {
    //                         const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
    //                         assert((result.levels[res_src] + between[num_of_sync_levels]) % result_num_of_levels == result.levels[res_tgt]);
    //                         add_transition_with_target(result, res_src, EPSILON, res_tgt, jump_mode, pred_map);
    //                         commited_states.insert(res_tgt);
    //                     } else {
    //                         // There is nothing to add.
    //                         // Try to find if there is already a mapping for this state.
    //                         const auto key = is_orig_lhs ? std::make_pair(orig_tgt, wait_root) : std::make_pair(wait_root, orig_tgt);
    //                         const State found_state = get_state_from_product_storage(key.first, key.second);
    //                         if (found_state != Limits::max_state) {
    //                             // The mapping exists, redirect transition goig to orig_src to the existing state.
    //                             duplicate_transitions(result, res_src, found_state, pred_map);
    //                         } else {
    //                             assert(found_state == Limits::max_state);
    //                             // The mapping does not exist. Update it.
    //                             create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs, res_src);
    //                             commited_states.insert(res_src);
    //                         }
    //                     }
    //                 } else {
    //                     // We are connecting to a non-zero-level state.
    //                     if (!visited.contains(orig_tgt)) {
    //                         // Let's continue the traversal.
    //                         // Project out the transition. Merge orig_scr and orig_tgt, while staying in res_src.
    //                         worklist.push({res_src, orig_tgt});
    //                         visited.insert(orig_tgt);
    //                     }
    //                 }
    //             }
    //         } else {
    //             // We are not at a sync level. We need just to copy the transitions.
    //             const Level orig_src_level = orig_nft.levels[orig_src];
    //             for (const SymbolPost& symbol_post: orig_nft.delta[orig_src]) {
    //                 for (const State orig_tgt: symbol_post.targets) {
    //                     const Level orig_tgt_level = orig_nft.levels[orig_tgt];
    //                     if (orig_tgt_level == 0) {
    //                         if (orig_src == orig_root && symbol_post.symbol == EPSILON && orig_nft.num_of_levels > 1) {
    //                             // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                             // It has been already handled in the main loop.
    //                             continue;
    //                         }

    //                         // We are connecting to a zero-level state.
    //                         const Level level_diff = orig_nft.num_of_levels - orig_src_level;
    //                         if (!add_before && between[num_of_sync_levels] != 0) {
    //                             // We will add EPSILON transitions from rhs AFTER this last transition (we are in lhs).
    //                             assert(result.levels[res_src] + level_diff < result_num_of_levels);
    //                             const bool exist_res_tgt_inner = local_state_map.contains(orig_tgt);
    //                             const State res_tgt_inner = get_state(orig_tgt, result.levels[res_src] + level_diff);
    //                             // First process the transition with the symbol_post.symbol.
    //                             result.delta.add(res_src, symbol_post.symbol, res_tgt_inner);
    //                             if (!exist_res_tgt_inner) {
    //                                 // Then add the EPSILON transitions.
    //                                 const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
    //                                 assert((result.levels[res_tgt_inner] + between[num_of_sync_levels]) % result_num_of_levels == result.levels[res_tgt]);
    //                                 add_transition_with_target(result, res_tgt_inner, EPSILON, res_tgt, jump_mode, pred_map);
    //                                 commited_states.insert(res_tgt);
    //                             }
    //                         } else {
    //                             assert(result.levels[res_src] + level_diff == result_num_of_levels);
    //                             const State res_tgt = create_composition_state(orig_tgt, wait_root, 0, is_orig_lhs);
    //                             result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                             commited_states.insert(res_tgt);
    //                         }
    //                     } else {
    //                         // We are connecting to a non-zero-level state.
    //                         // Just copy the transition.
    //                         const Level level_diff = orig_tgt_level - orig_src_level;
    //                         assert(result.levels[res_src] + level_diff <= result_num_of_levels);
    //                         const State tgt_res = get_state(orig_tgt, (result.levels[res_src] + level_diff) % result_num_of_levels);
    //                         result.delta.add(res_src, symbol_post.symbol, tgt_res);
    //                         pred_map[tgt_res].insert(res_src);
    //                         if (!visited.contains(orig_tgt)) {
    //                             // Not visited, add to the worklist
    //                             worklist.push({tgt_res, orig_tgt});
    //                             visited.insert(orig_tgt);
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // };

    // // Maps result of a composition starting from zero-level states lhs_root and rhs_root.
    // // This mapping ends in next zero-level states.
    // auto map_combination_path = [&](const State res_root, const State lhs_root, const State rhs_root) {
    //     pred_map.clear();
    //     std::stack<std::tuple<State, State, State>> worklist;
    //     worklist.push({res_root, lhs_root, rhs_root});

    //     bool perform_lhs_wait = false;
    //     bool perform_rhs_wait = false;

    //     while (!worklist.empty()) {
    //         auto [res_src, lhs_src, rhs_src] = worklist.top();
    //         worklist.pop();

    //         const Level res_src_level = result.levels[res_src];
    //         const Level lhs_src_level = lhs.levels[lhs_src];
    //         const Level rhs_src_level = rhs.levels[rhs_src];
    //         const bool lhs_in_target_and_rhs_remains = lhs_src_level == 0 && rhs_src_level != 0;

    //         // Go in lhs all the way down to the sync level.
    //         if (!lhs_is_sync_level[lhs_src_level] && !lhs_in_target_and_rhs_remains && !lhs.delta[lhs_src].empty()) {
    //             for (const SymbolPost& symbol_post: lhs.delta[lhs_src]) {
    //                 for (const State lhs_tgt: symbol_post.targets) {
    //                     const Level lhs_tgt_level = lhs.levels[lhs_tgt];
    //                     if (symbol_post.symbol == EPSILON && lhs_src_level == 0 && lhs_tgt_level == 0 && lhs_num_of_levels > 1) {
    //                         // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                         // It has been already handled in the main loop.
    //                         continue;
    //                     }

    //                     if (lhs_tgt_level == 0) {
    //                         const size_t trans_len = lhs_num_of_levels - lhs_src_level;
    //                         // We are connecting to a zero-level state.
    //                         if (rhs_between[num_of_sync_levels] == 0) {
    //                             // There is no transition in rhs.
    //                             const State res_tgt = create_composition_state(lhs_tgt, rhs_src, 0);
    //                             result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                             // There are no unprocessed synchronization transitions, so we can commit the state.
    //                             // TODO: is it really necessary?
    //                             commited_states.insert(res_tgt);
    //                         } else {
    //                             // There is a transition in rhs.
    //                             const bool visited = get_state_from_product_storage(lhs_tgt, rhs_src) != Limits::max_state;
    //                             const State res_tgt = create_composition_state(lhs_tgt, rhs_src, trans_len + res_src_level);
    //                             result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                             pred_map[res_tgt].insert(res_src);
    //                             if (!visited) {
    //                                 // Not visited, add to the worklist
    //                                 worklist.push({res_tgt, lhs_tgt, rhs_src});
    //                             }
    //                         }
    //                     } else {
    //                         // We are connecting to a non-zero-level state.
    //                         const Level level_diff = lhs_tgt_level - lhs_src_level;
    //                         assert(res_src_level + level_diff <= result_num_of_levels);
    //                         bool visited = get_state_from_product_storage(lhs_tgt, rhs_src) != Limits::max_state;
    //                         const State res_tgt = create_composition_state(lhs_tgt, rhs_src, (res_src_level + level_diff) % result_num_of_levels);
    //                         result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                         pred_map[res_tgt].insert(res_src);
    //                         if (!visited) {
    //                             // Not visited, add to the worklist
    //                             worklist.push({res_tgt, lhs_tgt, rhs_src});
    //                         }
    //                     }
    //                 }
    //             }
    //             continue;
    //         }

    //         // Go in rhs all the way down to the sync level.
    //         if (!rhs_is_sync_level[rhs_src_level]) {
    //             for (const SymbolPost& symbol_post: rhs.delta[rhs_src]) {
    //                 for (const State rhs_tgt: symbol_post.targets) {
    //                     const Level rhs_tgt_level = rhs.levels[rhs_tgt];
    //                     if (symbol_post.symbol == EPSILON && rhs_src_level == 0 && rhs_tgt_level == 0 && rhs_num_of_levels > 1) {
    //                         // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                         // It has been already handled in the main loop.
    //                         continue;
    //                     }

    //                     if (rhs_tgt_level == 0) {
    //                         // We are connecting to a zero-level state.
    //                         const State res_tgt = create_composition_state(lhs_src, rhs_tgt, 0);
    //                         result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                         commited_states.insert(res_tgt);
    //                     } else {
    //                         // We are connecting to a non-zero-level state.
    //                         const Level level_diff = rhs_tgt_level - rhs_src_level;
    //                         assert(res_src_level + level_diff <= result_num_of_levels);
    //                         const bool visited = get_state_from_product_storage(lhs_src, rhs_tgt) != Limits::max_state;
    //                         const State res_tgt = create_composition_state(lhs_src, rhs_tgt, (res_src_level + level_diff) % result_num_of_levels);
    //                         result.delta.add(res_src, symbol_post.symbol, res_tgt);
    //                         pred_map[res_tgt].insert(res_src);
    //                         if (!visited) {
    //                             // Not visited, add to the worklist
    //                             worklist.push({res_tgt, lhs_src, rhs_tgt});
    //                         }
    //                     }
    //                 }
    //             }
    //             continue;
    //         }

    //         // Everythink is on sync level.
    //         const bool epsilon_in_lhs = lhs.delta[lhs_src].find(EPSILON) != lhs.delta[lhs_src].end();
    //         const bool epsilon_in_rhs = rhs.delta[rhs_src].find(EPSILON) != rhs.delta[rhs_src].end();
    //         perform_lhs_wait = perform_lhs_wait || (epsilon_in_rhs && !epsilon_in_lhs);
    //         perform_rhs_wait = perform_rhs_wait || (epsilon_in_lhs && !epsilon_in_rhs);

    //         // Processes the intersection of transitions in lhs and rhs.
    //         auto process_intersection = [&](const StateSet& lhs_targets, const StateSet& rhs_targets, const Symbol symbol) {
    //             State local_res_src = res_src;
    //             if (!project_out_sync_levels && !lhs_targets.empty() && !rhs_targets.empty()) {
    //                 local_res_src = add_transition(result, local_res_src, symbol, 1, jump_mode, pred_map);
    //             }
    //             for (const State lhs_tgt: lhs_targets) {
    //                 const Level lhs_tgt_level = lhs.levels[lhs_tgt];
    //                 if (symbol == EPSILON && lhs_src_level == 0 && lhs_tgt_level == 0 && lhs_num_of_levels > 1) {
    //                     // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                     // It has been already handled in the main loop.
    //                     continue;
    //                 }
    //                 for (const State rhs_tgt: rhs_targets) {
    //                     const Level rhs_tgt_level = rhs.levels[rhs_tgt];
    //                     if (symbol == EPSILON && rhs_src_level == 0 && rhs_tgt_level == 0 && rhs_num_of_levels > 1) {
    //                         // This is an NFA-like epsilon transition going from zero-level state to another zero-level state.
    //                         // It has been already handled in the main loop.
    //                         continue;
    //                     }

    //                     if (lhs_tgt_level == 0 && rhs_tgt_level == 0) {
    //                         // We are connecting to a zero-level state.
    //                         const State found_state = get_state_from_product_storage(lhs_tgt, rhs_tgt);
    //                         if (found_state != Limits::max_state) {
    //                             // The mapping exists, redirect transition goig to orig_src to the existing state.
    //                             duplicate_transitions(result, local_res_src, found_state, pred_map);
    //                         } else {
    //                             assert(found_state == Limits::max_state);
    //                             // The mapping does not exist. Update it.
    //                             create_composition_state(lhs_tgt, rhs_tgt, 0, true, local_res_src);
    //                             commited_states.insert(local_res_src);
    //                         }
    //                     } else {
    //                         const State found_state = get_state_from_product_storage(lhs_tgt, rhs_tgt);
    //                         if (found_state == Limits::max_state) {
    //                             // We have not visited this pair yet.
    //                             insert_to_product_storage(lhs_tgt, rhs_tgt, local_res_src);
    //                             worklist.push({local_res_src, lhs_tgt, rhs_tgt});
    //                         } else {
    //                             duplicate_transitions(result, local_res_src, found_state, pred_map);
    //                         }
    //                     }
    //                 }
    //             }
    //         };

    //         // Process symbol intersection
    //         mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
    //         mata::utils::push_back(sync_iterator, lhs.delta[lhs_src]);
    //         mata::utils::push_back(sync_iterator, rhs.delta[rhs_src]);
    //         while (sync_iterator.advance()) {
    //             const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
    //             assert(same_symbol_posts.size() == 2); // One move per state in the pair.

    //             process_intersection(
    //                 same_symbol_posts[0]->targets,
    //                 same_symbol_posts[1]->targets,
    //                 same_symbol_posts[0]->symbol
    //             );
    //         }

    //         // Process DONT_CARE symbol in lhs.
    //         if (lhs.delta[lhs_src].find(DONT_CARE) != lhs.delta[lhs_src].end()) {
    //             for (const SymbolPost& symbol_post: rhs.delta[rhs_src]) {
    //                 process_intersection(
    //                     lhs.delta[lhs_src].find(DONT_CARE)->targets,
    //                     symbol_post.targets,
    //                     symbol_post.symbol
    //                 );
    //             }
    //         }

    //         // Process DONT_CARE symbol in rhs.
    //         if (rhs.delta[rhs_src].find(DONT_CARE) != rhs.delta[rhs_src].end()) {
    //             for (const SymbolPost& symbol_post: lhs.delta[lhs_src]) {
    //                 process_intersection(
    //                     symbol_post.targets,
    //                     rhs.delta[rhs_src].find(DONT_CARE)->targets,
    //                     symbol_post.symbol
    //                 );
    //             }
    //         }
    //     }

    //     // Perform waiting if necessary.
    //     if (perform_lhs_wait) {
    //         map_epsilon_on_sync_path(res_root, rhs_root, lhs_root, false);
    //     }
    //     if (perform_rhs_wait) {
    //         map_epsilon_on_sync_path(res_root, lhs_root, rhs_root, true);
    //     }
    // };

    // // INITIALIZATION on a worklist (side effect of a create_composition_state)
    // for (const State lhs_root: lhs.initial) {
    //     for (const State rhs_root: rhs.initial) {
    //         // Get the root state in the result NFT
    //         const State res_root = create_composition_state(lhs_root, rhs_root, 0);
    //         commited_states.insert(res_root);
    //         result.initial.insert(res_root);
    //     }
    // }

    // // MAIN LOOP
    // while (!worklist.empty()) {
    //     auto [orig_lhs, orig_rhs] = worklist.top();
    //     worklist.pop();

    //     // Get the state in the result NFT
    //     const State res_root = get_state_from_product_storage(orig_lhs, orig_rhs);
    //     // TODO - is commited_states really necessary?
    //     if (!commited_states.contains(res_root)) {
    //         result.final.erase(res_root);
    //         continue;
    //     }

    //     // Process "long" EPSILON transitions leading from zero-level state to zero-level state.
    //     auto epsilon_post_a = lhs.delta[orig_lhs].find(EPSILON);
    //     if (epsilon_post_a != lhs.delta[orig_lhs].end()) {
    //         for (const State target: epsilon_post_a->targets) {
    //             if (lhs.levels[target] == 0) {
    //                 const State res_tgt = create_composition_state(target, orig_rhs, 0);
    //                 add_transition_with_target(result, res_root, EPSILON, res_tgt, jump_mode, pred_map);
    //                 // result.delta.add(res_root, EPSILON, res_tgt);
    //                 commited_states.insert(res_tgt);
    //             }
    //         }
    //     }
    //     auto epsilon_post_b = rhs.delta[orig_rhs].find(EPSILON);
    //     if (epsilon_post_b != rhs.delta[orig_rhs].end()) {
    //         for (const State target: epsilon_post_b->targets) {
    //             if (rhs.levels[target] == 0) {
    //                 const State res_tgt = create_composition_state(orig_lhs, target, 0);
    //                 add_transition_with_target(result, res_root, EPSILON, res_tgt, jump_mode, pred_map);
    //                 // result.delta.add(res_root, EPSILON, res_tgt);
    //                 commited_states.insert(res_tgt);
    //             }
    //         }
    //     }

    //     // Map path created by the composition of lhs and rhs NFTs.
    //     // Each iteration starts in zero-level states orig_lhs and orig_rhs
    //     // and ends in next zero-level states.
    //     map_combination_path(res_root, orig_lhs, orig_rhs);
    // }

    // return result.trim();


}

Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    if (jump_mode == JumpMode::NoJump) {
        // If we are not jumping, we can use the fast composition.
        return compose_fast(lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, true, jump_mode);
    }

    // TODO - add better unwinding
    throw std::runtime_error("Noodler should not go here. Noodler should always use JumpMode::NoJump.");
    return compose_fast(lhs.unwind_jumps({ DONT_CARE }, jump_mode), rhs.unwind_jumps({ DONT_CARE }, jump_mode), lhs_sync_levels, rhs_sync_levels,  project_out_sync_levels, true, JumpMode::NoJump);
}

} // mata::nft
