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

    struct SynchronizationProperties { 
        const Nft& nft;
        const std::vector<SynchronizationType> reachable_sync_types; ///< Types of synchronization for each state.
        InvertedProductStorage& composition_to_this_map; ///< Mapping from composition state to lhs state.
        InvertedProductStorage& composition_to_other_map; ///< Mapping from composition state to rhs state.
        const Level sync_level; ///< The synchronization level.
        const size_t num_of_trans_to_replace_sync; ///< Number of transitions to replace synchronization.

        SynchronizationProperties(const Nft& nft, const Level sync_level, const size_t num_of_trans_to_replace_sync,
                                  InvertedProductStorage& composition_to_this_map, InvertedProductStorage& composition_to_other_map)
            : nft(nft),
              reachable_sync_types(get_synchronization_types(nft, sync_level)),
              composition_to_this_map(composition_to_this_map),
              composition_to_other_map(composition_to_other_map),
              sync_level(sync_level),
              num_of_trans_to_replace_sync(num_of_trans_to_replace_sync) {}
    };

    static const bool perform_synchronization[4][4] = {
    /*                        *                            rhs_sync_type                                 */
    /*     lhs_sync_type      * UNINITIALIZED | ONLY_ON_SYMBOL | ONLY_ON_EPSILON | ON_EPSILON_AND_SYMBOL */
    /* =======================*========================================================================= */
    /*  UNINITIALIZED         */ {    true,          false,           false,                false        },
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
    const size_t lhs_num_of_states = lhs.num_of_states();
    const size_t rhs_num_of_states = rhs.num_of_states();
    const size_t result_num_of_levels = lhs.num_of_levels + rhs.num_of_levels - (project_out_sync_levels ? (2) : 1);
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

    // Calculate the number of epsilon transitions that will replace one epsilon synchronization transition during waiting.
    // This depends on whether we are projecting out the synchronization level and if there are any transitions in the waiting
    // transducer, that we need to place right before or right after the synchronization level.
    const size_t lhs_levels_before_sync = lhs_sync_level;
    const size_t rhs_levels_before_sync = rhs_sync_level;
    const size_t lhs_levels_after_sync = lhs.num_of_levels - lhs_sync_level - 1;
    const size_t rhs_levels_after_sync = rhs.num_of_levels - rhs_sync_level - 1;
    // If we are waiting on the RHS, we need to know how many epsilon transitions
    // will replace the synchronization transition in the LHS.
    // 1) Transitions from RHS will always go as last before the snychronization level.
    // 2) If the synchronization level is the last level in the LHS, we need to add 
    //    all remaining transitions from the RHS that goes after the synchronization level.
    // 3) If we are not projecting out the synchronization levels, we need to incorporate this into the count.
    const size_t lhs_num_of_trans_to_replace_sync = (rhs_levels_before_sync) +         
                                                    (lhs_sync_level == lhs.num_of_levels - 1 ? rhs_levels_after_sync : 0) +
                                                    (project_out_sync_levels ? 0 : 1); 
    // If we are waiting on the LHS, we need to know how many epsilon transitions
    // will replace the synchronization transition in the RHS.
    // 1) Because transtions from LHS always go as first, the only time when they go immediately before the synchronization level
    //    is when the synchronization level is the first level in the RHS.
    // 2) LHS transitions go always before RHS transitions, so after the synchronization level, 
    //    we know that there will be transitions from the LHS.
    // 3) If we are not projecting out the synchronization levels, we need to incorporate this into the count.
    const size_t rhs_num_of_trans_to_replace_sync = (rhs_sync_level == 0 ? lhs_levels_before_sync : 0) +
                                                    (lhs_levels_after_sync) +
                                                    (project_out_sync_levels ? 0 : 1);
     
    const SynchronizationProperties lhs_sync_props(lhs, lhs_sync_level, lhs_num_of_trans_to_replace_sync, composition_to_lhs, composition_to_rhs);
    const SynchronizationProperties rhs_sync_props(rhs, rhs_sync_level, rhs_num_of_trans_to_replace_sync, composition_to_rhs, composition_to_lhs);

    auto model_waiting = [&](const State composition_root_state, const bool is_lhs_waiting) {
        const SynchronizationProperties& running_sync_props = is_lhs_waiting ? rhs_sync_props : lhs_sync_props;
        const Level sync_level = running_sync_props.sync_level;
        const State running_root_state = running_sync_props.composition_to_this_map[composition_root_state];
        const State waiting_root_state = running_sync_props.composition_to_other_map[composition_root_state];
        const Levels& running_levels = running_sync_props.nft.levels;
        const Delta& running_delta = running_sync_props.nft.delta;
        const size_t num_of_trans_to_replace_sync = running_sync_props.num_of_trans_to_replace_sync;
        const std::vector<SynchronizationType>& reachable_sync_types = running_sync_props.reachable_sync_types;
        // If the synchronization level is the last level, and it cannot be replaced by any sequence of epsilon transitions
        // (i.e., it wanishes), we need to "in place" do the synchronization step and connect previous state to the next
        // zero-level state.
        const bool handle_sync_in_place = running_sync_props.sync_level == running_sync_props.nft.num_of_levels - 1 &&
                                          num_of_trans_to_replace_sync == 0;

        std::stack<std::pair<State, State>> stack;
        stack.push({ composition_root_state, running_root_state});
        while (!stack.empty()) {
            auto [composition_state, running_state] = stack.top();
            const Level running_state_level = running_levels[running_state];
            stack.pop();

            // If we are in RHS and LHS is waiting and also the synchronization level is not the first level,
            // then we need to add the epsilon transition from the waiting LHS, because transitions from the LHS
            // goes always before the transitions from the RHS.
            if (is_lhs_waiting && sync_level != 0 && running_state_level == 0 && lhs_levels_before_sync > 0) {
                composition_state = add_transition(result, composition_state, EPSILON, lhs_levels_before_sync, jump_mode);
            }

            if (running_state_level < sync_level) {
                // We are before the synchronization level.
                for (const SymbolPost& running_symbol_post : running_delta[running_state]) {
                    for (const State running_target : running_symbol_post.targets) {
                        if (reachable_sync_types[running_target] == SynchronizationType::ONLY_ON_SYMBOL) {
                            // The target state leads to only a synchronization on a symbol.
                            // We don't need this. We need to synchronize on epsilon.
                            continue;
                        }

                        const Level running_target_level = running_levels[running_target];
                        const size_t trans_len = running_target_level - running_state_level;
                        assert(running_target_level > running_state_level);
                        assert(trans_len > 0);
                        
                        if (running_target_level == sync_level && handle_sync_in_place) {
                            // We are at the synchronization level after which there will be nothing added.
                            // We need to connect running_state to epsilon successors of the running_target.
                            auto epsilon_sync_symbol_post = running_delta[running_target].find(EPSILON);
                            assert(epsilon_sync_symbol_post != running_delta[running_target].end());
                            for (const State epsilon_target : epsilon_sync_symbol_post->targets) {
                                assert(running_levels[epsilon_target] == 0);
                                add_transition_with_target(result, composition_state, running_symbol_post.symbol, create_composition_state(waiting_root_state, epsilon_target, 0, is_lhs_waiting), jump_mode);
                            }                                    
                        } else {
                            // It does not matter if the target is a synchronization level or not 
                            // (we can handle any synchronization later, because it will be replaced
                            // by epsilon transitions).
                            const State new_composition_state = add_transition(result, composition_state, running_symbol_post.symbol, trans_len, jump_mode);
                            stack.push({ std::move(new_composition_state), running_target });
                        }
                    }
                }

            } else if (running_state_level == sync_level) {
                // We are exactly at the synchronization level.
                assert(!handle_sync_in_place);
                auto epsilon_sync_symbol_post = running_delta[running_state].find(EPSILON);
                assert(epsilon_sync_symbol_post != running_delta[running_state].end());
                for (const State epsilon_target : epsilon_sync_symbol_post->targets) {
                    if (num_of_trans_to_replace_sync > 0) {
                        // We will add some transitions on the place of the synchronization transition.
                        if (running_levels[epsilon_target] == 0) {
                            // We are connecting directly to the zero-level state.
                            add_transition_with_target(result, composition_state, EPSILON, create_composition_state(waiting_root_state, epsilon_target, 0, is_lhs_waiting), jump_mode);
                        } else {
                            // We are connecting to an internal state. Create new auxiliar state for it.
                            const State new_composition_state = add_transition(result, composition_state, EPSILON, num_of_trans_to_replace_sync, jump_mode);
                            stack.push({ std::move(new_composition_state), epsilon_target });
                        }
                    } else {
                        // Just ignore this transition.
                        // (It is being projected out and there are no transitions in the waiting NFT
                        // that would be place directly before or after the synchronization level.)
                        stack.push({ composition_state, epsilon_target });
                    }
                }
            } else {
                // We are past the synchronization level.
                // We just need to get to the next zero-level state.
                for (const SymbolPost& symbol_post : running_delta[running_state]) {
                    for (const State target : symbol_post.targets) {
                        const Level target_level = running_levels[target];
                        if (target_level == 0) {
                            // We are connecting to a zero-level state.
                            if (!is_lhs_waiting && rhs_levels_after_sync > 0) {
                                // We are in the LHS and RHS waits. Because transitions from RHS goes as last,
                                // we need to add epsilon transitions from the waiting RHS after this transition.
                                const size_t trans_len = running_sync_props.nft.num_of_levels - running_levels[running_state];
                                const State new_composition_state = add_transition(result, composition_state, symbol_post.symbol, trans_len, jump_mode);
                                add_transition_with_target(result, new_composition_state, EPSILON, create_composition_state(waiting_root_state, target, 0, is_lhs_waiting), jump_mode);      
                            } else {
                                // We don't need to add any other transitions, just this one.
                                add_transition_with_target(result, composition_state, symbol_post.symbol, create_composition_state(waiting_root_state, target, 0, is_lhs_waiting), jump_mode);
                            }
                        } else {
                            // Otherwise just copy the transition.
                            const State new_composition_state = add_transition(result, composition_state, symbol_post.symbol, target_level - running_state_level, jump_mode);
                            stack.push({new_composition_state, target});
                        }
                    }
                }
            }
        }
    };

    auto synchronize = [&](const State lhs_state, const State rhs_state, const State composition_source_state, const Symbol symbol_when_projected_out = Limits::max_symbol) {
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];
        const Level composition_target_level = (result.levels[composition_source_state] + 1) % result.num_of_levels;

        // Do the normal synchronization on symbol.
        mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
        mata::utils::push_back(sync_iterator, lhs.delta[lhs_state]);
        mata::utils::push_back(sync_iterator, rhs.delta[rhs_state]);
        while (sync_iterator.advance()) {
            const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
            assert(same_symbol_posts.size() == 2); // One move per state in the pair.
            const Symbol symbol = project_out_sync_levels ? symbol_when_projected_out : same_symbol_posts[0]->symbol;
            for (const State lhs_sync_target : same_symbol_posts[0]->targets) {
                for (const State rhs_sync_target : same_symbol_posts[1]->targets) {
                    
                    add_transition_with_target(result, composition_source_state, symbol, 
                        create_composition_state(lhs_sync_target, rhs_sync_target, composition_target_level, true), jump_mode);
                }
            }
        }
        
        // Do the synchronization on DONT_CARE in the LHS.
        auto lhs_dont_care_sync_it = lhs.delta[lhs_state].find(DONT_CARE);
        if (lhs_dont_care_sync_it != lhs.delta[lhs_state].end()) {
            for (const SymbolPost& rhs_symbol_post : rhs.delta[rhs_state]) {
                if (rhs_symbol_post.symbol == EPSILON) {
                    // We don't want to synchronize on DONT_CARE with EPSILON.
                    continue;
                }
                const Symbol symbol = project_out_sync_levels ? symbol_when_projected_out : rhs_symbol_post.symbol;
                for (const State rhs_sync_target : rhs_symbol_post.targets) {
                    for (const State lhs_sync_target : lhs_dont_care_sync_it->targets) {
                        // Add the transition from the LHS DONT_CARE to the RHS target.
                        add_transition_with_target(result, composition_source_state, symbol, 
                            create_composition_state(lhs_sync_target, rhs_sync_target, composition_target_level, true), jump_mode);
                    }
                }
            }
        }

        // Do the synchronization on DONT_CARE in the RHS.
        auto rhs_dont_care_sync_it = rhs.delta[rhs_state].find(DONT_CARE);
        if (rhs_dont_care_sync_it != rhs.delta[rhs_state].end()) {
            for (const SymbolPost& lhs_symbol_post : lhs.delta[lhs_state]) {
                if (lhs_symbol_post.symbol == EPSILON) {
                    // We don't want to synchronize on DONT_CARE with EPSILON.
                    continue;
                }
                const Symbol symbol = project_out_sync_levels ? symbol_when_projected_out : lhs_symbol_post.symbol;
                for (const State lhs_sync_target : lhs_symbol_post.targets) {
                    for (const State rhs_sync_target : rhs_dont_care_sync_it->targets) {
                        // Add the transition from the RHS DONT_CARE to the LHS target.
                        add_transition_with_target(result, composition_source_state, symbol, 
                            create_composition_state(lhs_sync_target, rhs_sync_target, composition_target_level, true), jump_mode);
                    }
                }
            }
        }
    };

    // INITIALIZATION of a worklist (side effect of a create_composition_state)
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            const State res_root = create_composition_state(lhs_root, rhs_root, 0);
            result.initial.insert(res_root);
        }
    }

    const std::vector<SynchronizationType>& lhs_reachable_sync_types = lhs_sync_props.reachable_sync_types;
    const std::vector<SynchronizationType>& rhs_reachable_sync_types = rhs_sync_props.reachable_sync_types;

    while (!worklist.empty()) {
        const State composition_state = worklist.front();
        const State lhs_state = composition_to_lhs[composition_state];
        const State rhs_state = composition_to_rhs[composition_state];
        const Level composition_state_level = result.levels[composition_state];
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];
        const size_t lhs_sync_type_id = static_cast<size_t>(lhs_reachable_sync_types[lhs_state]);
        const size_t rhs_sync_type_id = static_cast<size_t>(rhs_reachable_sync_types[rhs_state]);
        // LHS can be before the synchronization level only if RHS is before the synchronization level as well.
        // Otherwise, there are both past the synchronization level.
        const bool is_lhs_before_sync = (lhs_level != 0 && rhs_level == 0) && lhs_level < lhs_sync_level;
        const bool is_rhs_before_sync = (lhs_level <= lhs_sync_level) && rhs_level < rhs_sync_level;
        worklist.pop_front();

        if (lhs_level == 0 and rhs_level == 0) {
            // We are at the zero level states.
            // It is now time to decide is we want to continue the synchronization and/or
            // to wait in the lhs_state and/or rhs_state, because we can not synchronize
            // on epsilon and on a symbol at the same time.
            // Note: Transduce that contains symbol that can not synchronize with epsilon will wait.

            if (perform_wait_on_lhs[lhs_sync_type_id][rhs_sync_type_id]) {
                model_waiting(composition_state, true); // LHS is waiting.
            }
            
            if (perform_wait_on_rhs[lhs_sync_type_id][rhs_sync_type_id]) {
                model_waiting(composition_state, false); // RHS is waiting.
            }

            if (!perform_synchronization[lhs_sync_type_id][rhs_sync_type_id]) {
                // No synchronization is needed.
                // There are no symbols that would synchronize.
                // There is total sismatch between lhs and rhs synchronization symbols.
                continue;
            }
        }

        assert(perform_synchronization[lhs_sync_type_id][rhs_sync_type_id]);

        if (is_lhs_before_sync) {
            // LHS is before the synchronization level, so we need to first get there.
            for (const SymbolPost& lhs_symbol_post : lhs.delta[lhs_state]) {
                for (const State lhs_target : lhs_symbol_post.targets) {
                    const size_t lhs_target_sync_type_id = static_cast<size_t>(lhs_reachable_sync_types[lhs_target]);
                    if (!perform_synchronization[lhs_sync_type_id][lhs_target_sync_type_id]) {
                        // The target does not lead to a synchronization.
                        // We don't need to explore this.
                        continue;
                    }
                    const Level lhs_target_level = lhs.levels[lhs_target];
                    const size_t trans_len = lhs_target_level - lhs_level;
                    assert(lhs_target_level > lhs_level);
                    
                    // We don't need to worry, if it's a last useful transition, and
                    // doint the synchronization in place, because we are sure that there is/will be
                    // at leas one unprocessed useful transition in the RHS.
                    add_transition_with_target(result, composition_state, lhs_symbol_post.symbol, 
                        create_composition_state(lhs_target, rhs_state, composition_state_level + trans_len, true), jump_mode);
                }
            }
        } else if (is_rhs_before_sync) {
            // LHS is at synchronization level, but RHS is before the synchronization level.
            // RHS needs to continue.
            for (const SymbolPost& rhs_symbol_post : rhs.delta[rhs_state]) {
                for (const State rhs_target : rhs_symbol_post.targets) {
                    const size_t rhs_target_sync_type_id = static_cast<size_t>(rhs_reachable_sync_types[rhs_target]);
                    if (!perform_synchronization[rhs_sync_type_id][rhs_target_sync_type_id]) {
                        // The target does not lead to a synchronization.
                        // We don't need to explore this.
                        continue;
                    }
                    const Level rhs_target_level = rhs.levels[rhs_target];
                    const size_t trans_len = rhs_target_level - rhs_level;
                    assert(rhs_target_level > rhs_level);
                    
                    const bool lhs_level_is_last = lhs_level == lhs.num_of_levels - 1;
                    const bool rhs_target_level_is_last = rhs_target_level == rhs.num_of_levels - 1;
                    if (lhs_level_is_last && rhs_target_level_is_last && project_out_sync_levels) {
                        // LHS is at the synchronization level. There is no transition after the synchronization in LHS.
                        // The target in RHS is at synchronization level, at there is also no transition after the synchronization in RHS.
                        // Also we are projecting out the synchronization levels.
                        // This means, that we need now to synchronize and make a transition to the zero-level state in the result NFT.
                        synchronize(lhs_state, rhs_target, composition_state, rhs_symbol_post.symbol);
                    } else {
                        // Just copy the transition.
                        add_transition_with_target(result, composition_state, rhs_symbol_post.symbol, 
                            create_composition_state(lhs_state, rhs_target, composition_state_level + trans_len, false), jump_mode);
                    }
                }
            }

        } else if (lhs_level == lhs_sync_level && rhs_level == rhs_sync_level) {
            assert(lhs_level != lhs.num_of_levels - 1 && rhs_level != rhs.num_of_levels - 1);
            synchronize(lhs_state, rhs_state, composition_state);
        } else if (lhs_level != 0) {
            assert(!is_lhs_before_sync && !is_rhs_before_sync);
        } else {
            assert(!is_lhs_before_sync && !is_rhs_before_sync);
            assert(lhs_level == 0 && rhs_level != 0);
        }
        
        
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
