/* composition.cc -- Composition of two NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include "mata/utils/two-dimensional-map.hh"
#include <cassert>
#include <numeric>


using namespace mata::utils;


namespace {
    using mata::Symbol;
    using namespace mata::nft;

    // Enum class for a state flag to indicate which synchronization
    //types (synchronization on epsilon or symbol) can be reached from the state.
    enum class SynchronizationType : uint8_t {
        UNINITIALIZED         = 0b0000'0000, ///< Default value.
        ONLY_ON_SYMBOL        = 0b0000'0001, ///< Synchronization on EPSILON.
        ONLY_ON_EPSILON       = 0b0000'0010, ///< Synchronization on symbol.
        ON_EPSILON_AND_SYMBOL = 0b0000'0011, ///< Synchronization on EPSILON and symbol.
        UNDER_COMPUTATION     = 0b0000'0100  ///< The synchronization is being computed (only helper value).
    };
    inline SynchronizationType operator|(SynchronizationType lhs, SynchronizationType rhs) {
        return static_cast<SynchronizationType>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }
    inline SynchronizationType& operator|=(SynchronizationType& lhs, SynchronizationType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    /**
     * @brief A class to hold properties related to synchronization during the composition of NFTs.
     * This simplifies the passing of multiple synchronization-related parameters.
     */
    class SynchronizationProperties {
    public:
        const Nft& nft;
        const Level sync_level;
        const size_t num_of_levels_before_sync;
        const size_t num_of_levels_after_sync;
        std::vector<SynchronizationType> sync_types_v{};

        SynchronizationProperties(const Nft& nft, const Level sync_level)
            : nft(nft), 
              sync_level(sync_level), 
              num_of_levels_before_sync(sync_level),
              num_of_levels_after_sync(nft.num_of_levels - sync_level - 1)
        {
            get_synchronization_types();
        }

    private:
        /**
         * @brief For each state sets a type of synchronization that follows.
         */
        void get_synchronization_types() {
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
        }
    };
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

    // Initialize the synchronization properties for both NFTs.
    const SynchronizationProperties lhs_sync_props(lhs, lhs_sync_level);
    const SynchronizationProperties rhs_sync_props(rhs, rhs_sync_level);
    const std::vector<SynchronizationType>& lhs_sync_types_v = lhs_sync_props.sync_types_v;
    const std::vector<SynchronizationType>& rhs_sync_types_v = rhs_sync_props.sync_types_v;

    Nft result;
    result.num_of_levels = result_num_of_levels;
    // Use composition storage without tracking inverted indices,
    // as the waiting in virtual states would spoit the inverse mapping.
    TwoDimensionalMap<State, false> composition_storage(lhs_num_of_states, rhs_num_of_states);
    std::deque<std::pair<State, State>> worklist;

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
        const State found_state = composition_storage.get(lhs_state, rhs_state);
        if (found_state != Limits::max_state) {
            assert(found_state == composition_state_to_add || composition_state_to_add == Limits::max_state);
            return found_state;
        }

        // If not found, add a new state to the result NFT.
        State new_state = (composition_state_to_add != Limits::max_state) ? composition_state_to_add : result.add_state_with_level(level);
        composition_storage.insert(lhs_state, rhs_state, new_state);

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
     * @brief Perform the synchronization of LHS and RHS at the given state pair.
     * 
     * @param composition_state The state in the composition NFT from where the synchronization result will lead.
     * @param lhs_state The state in the LHS NFT that is being synchronized.
     * @param rhs_state The state in the RHS NFT that is being synchronized.
     * @param reconnect If true, the synchronization will reconnect the predecessors of the synchronization level to
     *                  its successors, otherwise it will just synchronize and keep/remove the synchronization level
     *                  depending on the project_out_sync_levels flag.
     * @param reconnection_symbol The symbol to use for the reconnection transition if reconnect is true. 
     */
    auto synchronize = [&](const State composition_state,
                           const State lhs_state,
                           const State rhs_state,
                           const bool reconnect = false,
                           const Symbol reconnection_symbol = Limits::max_symbol) 
    {
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];
        const Level composition_target_level = (result.levels[composition_state] + 1) % result.num_of_levels;
        // When we are projecting out the synchronization levels, and we don't want to connect its
        // predecessors to its successors, we will just do the synchronization, note reached targets
        // and remove (vanish) the synchronization level with its transition from the result NFT.
        const bool vanish_sync_level = !reconnect && project_out_sync_levels;

        // Helper function to combine LHS and RHS targets over the given symbol.
        auto combine_targets = [&](const StateSet& lhs_sync_targets, const StateSet& rhs_sync_targets, const Symbol symbol) {
            for (const State lhs_sync_target : lhs_sync_targets) {
                for (const State rhs_sync_target : rhs_sync_targets) {
                    if (vanish_sync_level) {
                        worklist.push_back({ lhs_sync_target, rhs_sync_target });
                    } else {
                        result.add_transition_with_target(composition_state, symbol,
                            create_composition_state(lhs_sync_target, rhs_sync_target, composition_target_level, true), jump_mode);
                    }
                }
            }
        };

        // Synchronization using SynchronizedUniversalIterator.
        mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
        mata::utils::push_back(sync_iterator, lhs.delta[lhs_state]);
        mata::utils::push_back(sync_iterator, rhs.delta[rhs_state]);
        while (sync_iterator.advance()) {
            const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
            assert(same_symbol_posts.size() == 2); // One move per state in the pair.
            const Symbol symbol = project_out_sync_levels ? reconnection_symbol : same_symbol_posts[0]->symbol;
            combine_targets(same_symbol_posts[0]->targets, same_symbol_posts[1]->targets, symbol);
        }

        // Synchronization on DONT_CARE in the LHS.
        auto lhs_dont_care_sync_it = lhs.delta[lhs_state].find(DONT_CARE);
        if (lhs_dont_care_sync_it != lhs.delta[lhs_state].end()) {
            for (const SymbolPost& rhs_symbol_post : rhs.delta[rhs_state]) {
                if (rhs_symbol_post.symbol == EPSILON) {
                    // We don't want to synchronize DONT_CARE with EPSILON.
                    continue;
                }
                const Symbol symbol = project_out_sync_levels ? reconnection_symbol : rhs_symbol_post.symbol;
                combine_targets(lhs_dont_care_sync_it->targets, rhs_symbol_post.targets, symbol);
            }
        }

        // Do the synchronization on DONT_CARE in the RHS.
        auto rhs_dont_care_sync_it = rhs.delta[rhs_state].find(DONT_CARE);
        if (rhs_dont_care_sync_it != rhs.delta[rhs_state].end()) {
            for (const SymbolPost& lhs_symbol_post : lhs.delta[lhs_state]) {
                if (lhs_symbol_post.symbol == EPSILON) {
                    // We don't want to synchronize DONT_CARE with EPSILON.
                    continue;
                }
                const Symbol symbol = project_out_sync_levels ? reconnection_symbol : lhs_symbol_post.symbol;
                combine_targets(lhs_symbol_post.targets, rhs_dont_care_sync_it->targets, symbol);
            }
        }
    };

    /**
     * @brief Copy transitions from the copy NFT to the composition NFT.
     * 
     * @param composition_state The source state in the composition NFT where the transitions will be connected to.
     * @param copy_state The state in the copy NFT from which the transitions will be copied.
     * @param stationar_state The state in the other NFT (stationary NFT) that does not move.
     * @param is_copy_state_lhs If true, the copy_state is from the LHS NFT, otherwise it is from the RHS NFT.
     * @param waiting_worklist If set, the function is called from the waiting simulation,
                               meaning that we are waiting in the virtual loop at the stationary state,
                               and we will use the waiting_worklist to put next state pairs to.
     */
    auto copy_transition = [&](const State composition_state,
                               const State copy_state,
                               const State stationar_state, 
                               const bool is_copy_state_lhs,
                               std::queue<std::pair<State, State>>* waiting_worklist = nullptr) 
    {
        const SynchronizationProperties& copy_sync_props = is_copy_state_lhs ? lhs_sync_props 
                                                                             : rhs_sync_props;
        const SynchronizationProperties& stationar_sync_props = is_copy_state_lhs ? rhs_sync_props 
                                                                                  : lhs_sync_props;
        const Nft& copy_nft = copy_sync_props.nft;
        const Level composition_state_level = result.levels[composition_state];
        const Level copy_state_level = copy_nft.levels[copy_state];
        const Level stationar_state_level = stationar_sync_props.nft.levels[stationar_state];
        const Level copy_sync_level = copy_sync_props.sync_level;
        const std::vector<SynchronizationType>& copy_sync_types = copy_sync_props.sync_types_v;
        const SynchronizationType stationar_state_sync_type = stationar_sync_props.sync_types_v[stationar_state];
        
        // We need to handle any synchronization here in place, if we want to project out
        // the synchronization level and if there is no transition remainging after the 
        // synchronization level in any of the NFTs.
        const bool handle_synchronization_in_place = project_out_sync_levels &&
                                                     stationar_sync_props.num_of_levels_after_sync == 0 &&
                                                     copy_sync_props.num_of_levels_after_sync == 0 && (
                                                        waiting_worklist != nullptr ||                                                                                      
                                                        stationar_state_level == stationar_sync_props.sync_level
                                                     );
                                                        
        for (const SymbolPost& copy_symbol_post : copy_nft.delta[copy_state]) {
            for (const State copy_target : copy_symbol_post.targets) {
                const SynchronizationType copy_target_sync_type = copy_sync_types[copy_target];
                // It makes sence to onyl continue if we beleave that we can synchronize.
                // When the function has been called from the waiting simulation, we are
                // interested in synchronizations on EPSILON.
                // This should also work for the case when we are past the synchronization level.
                const bool can_synchronize_in_the_future = (
                    waiting_worklist != nullptr ? copy_target_sync_type != SynchronizationType::ONLY_ON_SYMBOL
                                                : stationar_state_sync_type == copy_target_sync_type ||
                                                  stationar_state_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL ||
                                                  copy_target_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL
                );
                if (!can_synchronize_in_the_future) {
                    // There is no way we would be able to synchronize.
                    // We don't need to explore this path.
                    continue;
                }

                const Level target_level = copy_nft.levels[copy_target];
                const size_t trans_len = (target_level == 0 ? copy_nft.num_of_levels : target_level) - copy_state_level;
                assert(target_level == 0 || target_level > copy_state_level);
                assert(trans_len > 0);

                if (handle_synchronization_in_place && target_level == copy_sync_level) {
                    // The target is at the synchronization level that will be projected out.
                    // We know that there are no transitions after the synchronization level
                    // in any of the NFTs. Therefore, we have to do the synchronization and
                    // make this transition go all the way to the next zero-level state.
                    if (waiting_worklist != nullptr) {
                        // We have beed called from the simulation of the waiting.
                        // We are waiting in the stationar state. We need to handle the waiting
                        // synchronization over EPSILON here.
                        const auto& copy_symbol_post_eps = copy_nft.delta[copy_target].find(EPSILON);
                        assert(copy_symbol_post_eps != copy_nft.delta[copy_target].end());
                        for (const State inplace_copy_target : copy_symbol_post_eps->targets) {
                            assert(copy_nft.levels[inplace_copy_target] == 0);
                            result.add_transition_with_target(composition_state, copy_symbol_post.symbol,
                                create_composition_state(inplace_copy_target, stationar_state, 0, is_copy_state_lhs), jump_mode);
                        }
                    } else {
                        // We encountered normal synchronization. Call the synchronization function.
                        // The only way we can reach synchronization point in both states, while
                        // waiting in one state them, is when the waiting state is the LHS state.
                        // Wecause each transition in the LHS goes before the transitions in the RHS.
                        assert(!is_copy_state_lhs);
                        synchronize(composition_state, stationar_state, copy_target, true, copy_symbol_post.symbol);
                    }

                } else {
                    // We are not at the synchronization point.
                    // We just need to copy the transtion.
                    const Level new_composition_state_level = (composition_state_level + trans_len) % result.num_of_levels;
                    // The creation of new composition state depends on whether we were
                    // called from the waiting simulation or not.
                    // If we were called from the waiting simulation, we need to create state that copy_target
                    // and a virtual state over the virtual waiting loop over the stationar state.
                    // Because the virtual state is not part of the waiting NFT, we can not use create_composition_state
                    // function, because it uses the composition_storage that does not contain the virtual state.
                    if (waiting_worklist != nullptr && new_composition_state_level != 0) {
                        const State new_composition_state = result.add_state_with_level(new_composition_state_level);
                        result.add_transition_with_target(composition_state, copy_symbol_post.symbol, new_composition_state, jump_mode);
                        waiting_worklist->push({ new_composition_state, stationar_state });
                    } else {
                        result.add_transition_with_target(
                            composition_state, copy_symbol_post.symbol,
                            create_composition_state(copy_target, stationar_state, new_composition_state_level, is_copy_state_lhs), jump_mode
                        );
                    }
                }
            }
        }
    };

    /**
     * @brief Model the waiting in one of the NFTs if they could not synchronize due to the EPSILON or symbol transition.
     *
     * For example, if there is EPSILON on the synchronization level in the RHS and LHS can not synchronize on it,
     * then the LHS needs to wait in the root (zero-level) state that precedes the synchronization and RHS will
     * continue in the corresponding RHS root ower the problematic EPSILON synchronization to the next zero-level state.
     * The RHS is going to act al if it is interleaving with the LHS transitions (LHS transitions always go before RHS transitions),
     * however it will place the EPSILON on each such a transition from the LHS.
     *
     * The problem is if we want to project out the synchronization levels, but there are not transitions following them
     * in the LHS not RHS (the synchronization level is the last level in both of them). Then one level before the
     * synchronization level, we need to connect successors of such states to the zero-level state that lies after the synchronization level.
     *
     * @param composition_root_state The root state of the composition NFT.
     * @param waiting_root_state The root state of the NFT that is waiting (LHS or RHS).
     * @param running_root_state The root state of the NFT that is running (LHS or RHS).
     * @param is_lhs_waiting If true, the LHS is waiting, otherwise the RHS is waiting.
     */
    auto model_waiting = [&](const State composition_root_state, const State waiting_root_state, const State running_root_state, const bool is_lhs_waiting) {
        const SynchronizationProperties& running_sync_props = is_lhs_waiting ? rhs_sync_props 
                                                                             : lhs_sync_props;
        const SynchronizationProperties& waiting_sync_props = is_lhs_waiting ? lhs_sync_props 
                                                                             : rhs_sync_props;
        const Level sync_level = running_sync_props.sync_level;
        const Levels& running_levels = running_sync_props.nft.levels;
        const Delta& running_delta = running_sync_props.nft.delta;
        const size_t waiting_trans_before_sync = waiting_sync_props.num_of_levels_before_sync;
        const size_t waiting_trans_after_sync = waiting_sync_props.num_of_levels_after_sync;
        const size_t num_of_epsilons_to_replace_sync = (
            (is_lhs_waiting ? waiting_trans_after_sync : waiting_trans_before_sync) +
            (project_out_sync_levels ? 0 : 1)
        ); 

        // We cannot use inverted storage here, because there can be one composition
        // state for multiple original pairs of states. Therefore, we need to keep 
        // track of it in the queue instead.
        std::queue<std::pair<State, State>> worklist;

        // Initialization of the worklist.
        // If we know at this point, that waiting is LHS and there are transtiion before
        // the synchronization level, that we need to represent in the resulting NFT,
        // as EPSILON transitions, and because LHS goes before RHS, we can add this
        // transition immediately here during the initialization of the worklist.
        if (!is_lhs_waiting || waiting_trans_before_sync == 0) {
            worklist.push({ composition_root_state, running_root_state});
        } else {
            const State new_composition_state = result.add_transition_with_lenght(composition_root_state, EPSILON, waiting_trans_before_sync, jump_mode);
            worklist.push({ std::move(new_composition_state), running_root_state });
        }

        // The main loop of the waiting simulation.
        while (!worklist.empty()) {
            auto [composition_state, running_state] = worklist.front();
            worklist.pop();
            const Level running_state_level = running_levels[running_state];

            if (running_state_level == sync_level) {
                // We are at the synchronization level.
                if (num_of_epsilons_to_replace_sync > 1) {
                    // If the synchronization is going to be replace by multiple EPSILON transitions,
                    // we can reduce the number of redundant EPSILON transtions by adding n-1 transitions
                    // now and then use the last one to connect with the target state.
                    composition_state = result.add_transition_with_lenght(composition_state, EPSILON, num_of_epsilons_to_replace_sync - 1, jump_mode);
                }

                const bool is_last_transition = (running_state_level == running_sync_props.nft.num_of_levels - 1);
                const auto& running_epsilon_post_it = running_delta[running_state].find(EPSILON);
                assert(running_epsilon_post_it != running_delta[running_state].end());
                for (const State running_epsilon_target : running_epsilon_post_it->targets) {
                    if (num_of_epsilons_to_replace_sync == 0) {
                        // This synchronization level is being projected out.
                        // We don't need to worry about if there is any useful transition
                        // after this, because if not, that it would be already handled
                        // by the copy_transition function.
                        assert(!is_last_transition);
                        worklist.push({ composition_state, running_epsilon_target });
                    } else if (is_last_transition) {
                        // We are connecting to the zero-level state, therefore we have to
                        // create a new composition state that will be put into the main worklist
                        // (side effect of the create_composition_state).
                        result.add_transition_with_target(composition_state, EPSILON,
                            create_composition_state(running_epsilon_target, running_root_state, 0, is_lhs_waiting), jump_mode);
                    } else {
                        // We are connecting to the next state in the waiting loop,
                        // therefore we can just add a transition to the next state and
                        // continue the waiting simulation.
                        const State new_composition_state = result.add_transition_with_lenght(composition_state, EPSILON, num_of_epsilons_to_replace_sync, jump_mode);
                        worklist.push({ new_composition_state, running_epsilon_target });
                    }
                }
            } else if (running_state_level == 0 && running_state != running_root_state) {
                // We are at the zero-level state at the running NFT.
                // Because of previous checks, we know that this could only happen,
                // if the running NFT is the LHS NFT and there is are remaining transitions
                // in the waiting RHS NFT. Because RHS always goes after LHS, we can
                // just add a transition to the zero-level state and continue from there
                // in the main composition loop.
                assert(!is_lhs_waiting);
                assert(waiting_trans_after_sync > 0);
                assert((running_state_level + waiting_trans_after_sync) % result.num_of_levels == 0);
                result.add_transition_with_target(composition_state, EPSILON,
                    create_composition_state(running_state, running_root_state, 0, is_lhs_waiting), jump_mode);
            } else {
                // We are at some internal level in the running NFT.
                // We can just copy the transitions to next states.
                // We don't need to worry about potential synchronization just
                // after this level that would go to zero-level state,
                // because if such a synchronization exists,
                // it will be handled by the copy_transition function.
                copy_transition(composition_state, running_state, running_root_state, is_lhs_waiting, &worklist);
            }
        }
    };

    // Initialization of the worklist.
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            result.initial.insert(create_composition_state(lhs_root, rhs_root, 0));
        }
    }

    // The maing loop of the composition algorithm.
    while (!worklist.empty()) {
        const auto [lhs_state, rhs_state] = worklist.front();
        worklist.pop_front();
        const State composition_state = composition_storage.get(lhs_state, rhs_state);  
        assert(composition_state != Limits::max_state);
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];  
        
        const SynchronizationType lhs_sync_type = lhs_sync_types_v[lhs_state];
        const SynchronizationType rhs_sync_type = rhs_sync_types_v[rhs_state];
        if (lhs_level == 0 and rhs_level == 0) {
            // We are at the zero level states before any synchronization level.
            // It is now time to decide is we want to continue the synchronization and/or
            // to wait in the lhs_state and/or rhs_state, because there is a synchronization,
            // where on one of the transducers is EPSILON and on the other one is an alphabet symbol.
            // Note that if we can not synchronize in the future due to the EPSILON,
            // we will simulate waiting in the zero-level state of the transducer that does not
            // have the EPSILON transition that cause the synchronization problem.

            // You can see the table defining operations for pair of synchronization types
            // in the documentation of the SynchronizationType enum class.
            const bool perform_wait_on_lhs = lhs_sync_type != SynchronizationType::ONLY_ON_EPSILON &&
                                             rhs_sync_type != SynchronizationType::ONLY_ON_SYMBOL;
            const bool perform_wait_on_rhs = lhs_sync_type != SynchronizationType::ONLY_ON_SYMBOL &&
                                             rhs_sync_type != SynchronizationType::ONLY_ON_EPSILON;
            const bool can_synchronize_in_the_future = lhs_sync_type == rhs_sync_type ||
                                                      lhs_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL ||
                                                      rhs_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL;
            if (perform_wait_on_lhs) {
                // LHS is waiting (i.e., there is an EPSILON in the RHS that we can not synchronize on).
                model_waiting(composition_state, lhs_state, rhs_state, true);
            }
            if (perform_wait_on_rhs) {
                // RHS is waiting (i.e., there is an EPSILON in the LHS that we can not synchronize on).
                model_waiting(composition_state, rhs_state, lhs_state, false); 
            }
            if (!can_synchronize_in_the_future) {
                // There is no way we would be able to synchronize in the future.
                // It does not make sence to continue exploration of this pair.
                continue;
            }
        }

        // It makes sence to onyl continue if we beleave that we can synchronize,
        // or we already passed the synchronization level (i.e., lhs_sync_type ==
        // rhs_sync_type == SynchronizationType::UNINITIALIZED).
        assert(lhs_sync_type == rhs_sync_type ||
               lhs_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL ||
               rhs_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL);
        
        // Both LHS and RHS can do a step if they are not at the synchronization level.
        // However, there is a special case when LHS already reached the last zero-level state,
        // because there was no transition after the synchronization level in the LHS. Because
        // of that, we need to not allow LHS to do a step in this case.
        // The same does not apply to the RHS, because it always goes after the LHS.
        const bool do_step_in_lhs = lhs_level != lhs_sync_level && (lhs_level != 0 || rhs_level == 0);
        const bool do_step_in_rhs = rhs_level != rhs_sync_level;

        if (do_step_in_lhs) {
            // LHS alway goes before the RHS.
            copy_transition(composition_state, lhs_state, rhs_state, true);
        } else if (do_step_in_rhs) {
            // LHS is probably at the synchronization level or at the final zero-level state.
            // It's time for RHS to make a step.
            copy_transition(composition_state, rhs_state, lhs_state, false);
        } else {
            // Both LHS and RHS are at the synchronization level.
            // Perform the synchronization this way is only possible if:
            // 1) Some transition in the LHS or RHS that follows after
            //    the synchronization level (so we can connect to the successors), or
            // 2) There is not transition in the LHS or RHS that follows after
            //    the synchronization level, but we are not projecting out the 
            //    synchronization levels. We need to use the synchronization
            //    transition to connect to the zero-level successors of the synchronization.
            assert(lhs_level == lhs_sync_level && rhs_level == rhs_sync_level);
            synchronize(composition_state, lhs_state, rhs_state);
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
