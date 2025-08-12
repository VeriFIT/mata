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

    enum class SynchronizationType : uint8_t {
        UNINITIALIZED = 0,         ///< Default initialization value
        ONLY_ON_SYMBOL = 1,        ///< Epsilon symbol for synchronization
        ONLY_ON_EPSILON = 2,       ///< Non-epsilon symbol for synchronization
        ON_EPSILON_AND_SYMBOL = 3, ///< Both epsilon and non-epsilon
        UNDER_COMPUTATION = 4      ///< Unknown synchronization type
    };

    inline SynchronizationType operator|(SynchronizationType lhs, SynchronizationType rhs) {
        return static_cast<SynchronizationType>(
            static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
        );
    }

    inline SynchronizationType& operator|=(SynchronizationType& lhs, SynchronizationType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    static const bool perform_synchronization[4][4] = {
    /*                        *                            rhs_sync_type                                 */
    /*     lhs_sync_type      * UNINITIALIZED | ONLY_ON_SYMBOL | ONLY_ON_EPSILON | ON_EPSILON_AND_SYMBOL */
    /* =======================*========================================================================= */
    /*  UNINITIALIZED         */ {   false,          false,           false,                false        },
    /*  ONLY_ON_SYMBOL        */ {   false,           true,           false,                 true        },
    /*  ONLY_ON_EPSILON       */ {   false,          false,            true,                 true        },
    /*  ON_EPSILON_AND_SYMBOL */ {   false,           true,            true,                 true        }
    };

    static const bool perform_wait_on_lhs[4][4] = {
    /*                        *                            rhs_sync_type                                 */
    /*     lhs_sync_type      * UNINITIALIZED | ONLY_ON_SYMBOL | ONLY_ON_EPSILON | ON_EPSILON_AND_SYMBOL */
    /* =======================*========================================================================= */
    /*  UNINITIALIZED         */ {   false,          false,           false,                false        },
    /*  ONLY_ON_SYMBOL        */ {   false,          false,            true,                 true        },
    /*  ONLY_ON_EPSILON       */ {   false,          false,           false,                 false       },
    /*  ON_EPSILON_AND_SYMBOL */ {   false,          false,            true,                 true        }
    };

    static const bool perform_wait_on_rhs[4][4] = {
    /*                        *                            rhs_sync_type                                 */
    /*     lhs_sync_type      * UNINITIALIZED | ONLY_ON_SYMBOL | ONLY_ON_EPSILON | ON_EPSILON_AND_SYMBOL */
    /* =======================*========================================================================= */
    /*  UNINITIALIZED         */ {   false,          false,           false,                false        },
    /*  ONLY_ON_SYMBOL        */ {   false,          false,           false,                false        },
    /*  ONLY_ON_EPSILON       */ {   false,           true,           false,                 true        },
    /*  ON_EPSILON_AND_SYMBOL */ {   false,           true,           false,                 true        }
    };

    /**
     * @brief For each state sets a type of synchronization that follows.
     *
     * @param nft The NFT to analyze.
     * @param sync_level The level for which to compute the synchronization types.
     * @return A vector of synchronization types for each state in the NFT.
     */
    std::vector<SynchronizationType> get_synchronization_types(const Nft& nft, const size_t sync_level) {
        const size_t num_of_states = nft.num_of_states();
        std::vector<SynchronizationType> sync_types(num_of_states, SynchronizationType::UNINITIALIZED);

        for (State root = 0; root < num_of_states; ++root) {
            if (nft.levels[root] != sync_level) {
                continue; // Skip states not on the sync level.
            }

            std::stack<State> stack;
            stack.push(root);
            while (!stack.empty()) {
                const State state = stack.top();
                const Level state_level = nft.levels[state];
                const StatePost& state_post = nft.delta[state];
                SynchronizationType current_sync_type = sync_types[state];
                assert(current_sync_type == SynchronizationType::UNINITIALIZED || current_sync_type == SynchronizationType::UNDER_COMPUTATION);
                stack.pop();

                // If it has been visited, than by now we know that it has been already computed for its children.
                if (current_sync_type == SynchronizationType::UNDER_COMPUTATION) {
                    current_sync_type = SynchronizationType::UNINITIALIZED;
                    for (const SymbolPost& symbol_post : state_post) {
                        for (const State target : symbol_post.targets) {
                            current_sync_type |= sync_types[target];
                        }
                    }
                    sync_types[state] = current_sync_type;
                    continue; // We already visited its children.
                }

                // If we are on the sync level, we can compute the synchronization type.
                if (state_level == sync_level) {
                    if (state_post.find(EPSILON) != state_post.end()) {
                        current_sync_type = SynchronizationType::ONLY_ON_EPSILON;
                        if (state_post.size() > 1) {
                            current_sync_type |= SynchronizationType::ONLY_ON_SYMBOL;
                        }
                    } else if (!state_post.empty()) {
                        current_sync_type = SynchronizationType::ONLY_ON_SYMBOL;
                    }
                    sync_types[state] = current_sync_type;
                    continue; // No need to visit its children.
                }

                // We need to visit its children.
                sync_types[state] = SynchronizationType::UNDER_COMPUTATION; // Mark this state as under computation.
                stack.push(state); // Push it back to the stack to compute it later.
                for (const SymbolPost& symbol_post : state_post) {
                    for (const State target : symbol_post.targets) {
                        assert(nft.levels[target] > nft.levels[state]);
                        assert(sync_types[target] != SynchronizationType::UNDER_COMPUTATION);
                        if (sync_types[target] != SynchronizationType::UNINITIALIZED) {
                            current_sync_type |= sync_types[target];
                        } else {
                            stack.push(target); // Push the target state to visit it later.
                        }
                    }
                }
            }
        }

        assert(std::all_of(sync_types.begin(), sync_types.end(),
               [](SynchronizationType type) { return type != SynchronizationType::UNDER_COMPUTATION; }));

        return sync_types;
    }

    using ProductMap = std::unordered_map<std::pair<State,State>,State>;
    using MatrixProductStorage = std::vector<std::vector<State>>;
    using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
    using InvertedProductStorage = std::vector<State>;

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
            return target;
        }

        State inner_src = source;
        Level inner_level = nft.levels[inner_src] + 1;
        for (size_t i = 0; i < len - 1; ++i, ++inner_level) {
            assert(inner_level < nft.num_of_levels);
            const State inner_target = nft.add_state_with_level(inner_level);
            nft.delta.add(inner_src, symbol, inner_target);
            inner_src = inner_target;
        }
        assert(inner_level == nft.levels[source] + len);
        nft.delta.add(inner_src, symbol, target);

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
            return;
        }

        State inner_src = source;
        Level inner_level = nft.levels[inner_src] + 1;
        for (size_t i = 0; i < trans_len - 1; ++i, ++inner_level) {
            assert(inner_level < nft.num_of_levels);
            const State inner_target = nft.add_state_with_level(inner_level);
            nft.delta.add(inner_src, symbol, inner_target);
            inner_src = inner_target;
        }
        assert(inner_level == nft.levels[source] + trans_len);
        nft.delta.add(inner_src, symbol, target);
    }
}

namespace mata::nft
{


Nft compose(const Nft& lhs, const Nft& rhs, const Level lhs_sync_level, const Level rhs_sync_level, const bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(lhs_sync_level < lhs.num_of_levels && rhs_sync_level < rhs.num_of_levels);

    // Number of Levels and States
    const size_t lhs_num_of_levels = lhs.num_of_levels;
    const size_t rhs_num_of_levels = rhs.num_of_levels;
    const size_t lhs_num_of_states = lhs.num_of_states();
    const size_t rhs_num_of_states = rhs.num_of_states();
    const size_t result_num_of_levels = lhs_num_of_levels + rhs_num_of_levels - (project_out_sync_levels ? (2) : 1);
    assert(result_num_of_levels > 0);

    Nft result;
    result.num_of_levels = result_num_of_levels;
    std::deque<State> worklist;

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
            worklist.push_back(new_state);
            if ((is_a_lhs && lhs.final.contains(state_a) && rhs.final.contains(state_b)) ||
                (!is_a_lhs && rhs.final.contains(state_a) && lhs.final.contains(state_b))) {
                result.final.insert(new_state);
            }
        } else {
            worklist.push_front(new_state); // Push to the front for non-zero levels.
        }

        return new_state;
    };

    // INITIALIZATION of a worklist (side effect of a create_composition_state)
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            const State res_root = create_composition_state(lhs_root, rhs_root, 0);
            result.initial.insert(res_root);
        }
    }

    const std::vector<SynchronizationType> lhs_sync_types = get_synchronization_types(lhs, lhs_sync_level);
    const std::vector<SynchronizationType> rhs_sync_types = get_synchronization_types(rhs, rhs_sync_level);

    while (!worklist.empty()) {
        const State composition_state = worklist.front();
        const State lhs_state = composition_to_lhs[composition_state];
        const State rhs_state = composition_to_rhs[composition_state];
        const Level composition_state_level = result.levels[composition_state];
        assert((composition_state_level == 0) == (lhs.levels[lhs_state] == 0 && rhs.levels[rhs_state] == 0));
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];
        worklist.pop_front();

        if (composition_state_level == 0) {
            // We are at the zero level.
            // It is now time to decide is we want to continue the synchronization and/or
            // to wait in the lhs_state and/or rhs_state, because we can not synchronize
            // on epsilon and on a symbol at the same time.
            // Note: Transduce that contains symbol that can not synchronize with epsilon will wait.
            const size_t lhs_sync_type_id = static_cast<size_t>(lhs_sync_types[lhs_state]);
            const size_t rhs_sync_type_id = static_cast<size_t>(rhs_sync_types[rhs_state]);

            if (perform_wait_on_lhs[lhs_sync_type_id][rhs_sync_type_id]) {
                // TODO: wait on lhs
            }

            if (perform_wait_on_rhs[lhs_sync_type_id][rhs_sync_type_id]) {
                // TODO: wait on rhs
            }

            if (!perform_synchronization[lhs_sync_type_id][rhs_sync_type_id]) {
                // No synchronization is needed.
                // There are no symbols that would synchronize.
                // There is total sismatch between lhs and rhs synchronization symbols.
                continue;
            }
        }

        assert(perform_synchronization[static_cast<size_t>(lhs_sync_types[lhs_state])][static_cast<size_t>(rhs_sync_types[rhs_state])]);

        // TODO: Synchronization body.
    }

    return result.trim(); // Trim the result NFT to remove dead-end paths.
}

Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    if (lhs_sync_levels.size() == 1 && rhs_sync_levels.size() == 1) {
        // If we have only one synchronization level we can do it faster.
        return compose(lhs, rhs, lhs_sync_levels.front(), lhs_sync_levels.front(), project_out_sync_levels, jump_mode);
    }

    throw std::runtime_error("Noodler should not go here. Noodler should always use JumpMode::NoJump.");

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

} // mata::nft
