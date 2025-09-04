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

    /**
     * Enum class for a state flag to indicate which synchronization
     * types (synchronization on epsilon or symbol) can be reached from the state.
     * The synchronization type helps to determine if a state from lhs can
     * at some point in the next levels synchronize with a state from rhs.
     * The following table shows all possible combinations of synchronization types
     * for lhs and rhs states, and whether they can synchronize or if one of them will wait
     * because there is an EPSILON on the synchronization in the other that it
     * could not synchronize with.
     *                       ||                   RHS sync type
     *    LHS sync type      || ONLY_ON_SYMBOL | ONLY_ON_EPSILON | ON_EPSILON_AND_SYMBOL
     * ======================||================|=================|======================
     *                       ||   synchornize  |                 |      synchornize
     *    ONLY_ON_SYMBOL     ||                |   wait on LHS   |      wait on LHS
     *                       ||                |                 |
     * ----------------------||----------------|-----------------|----------------------
     *                       ||                |   synchronize   |      synchornize
     *   ONLY_ON_EPSILON     ||                |   lait on LHS   |      lait on LHS
     *                       ||   wait on RHS  |   wait on RHS   |      wait on RHS
     * ----------------------||----------------|-----------------|----------------------
     *                       ||   synchronize  |   synchronize   |      synchornize
     * ON_EPSILON_AND_SYMBOL ||                |   wait on LHS   |      wait on LHS
     *                       ||   wait on RHS  |   wait on RHS   |      wait on RHS
     * =================================================================================
     */
    enum class SynchronizationType : uint8_t {
        UNDEFINED             = 0b0000'0000, ///< Default value. Or used if we are past the synchronization.
        ONLY_ON_SYMBOL        = 0b0000'0001, ///< Synchronization on EPSILON.
        ONLY_ON_EPSILON       = 0b0000'0010, ///< Synchronization on symbol.
        ON_EPSILON_AND_SYMBOL = 0b0000'0011, ///< Synchronization on EPSILON and symbol.
        UNDER_COMPUTATION     = 0b0000'0100  ///< The synchronization is being computed (only a helper value).
    };
    inline SynchronizationType operator|(SynchronizationType lhs, SynchronizationType rhs) {
        return static_cast<SynchronizationType>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }
    inline SynchronizationType& operator|=(SynchronizationType& lhs, SynchronizationType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }
    inline bool exist_intersection_of_sync_types(SynchronizationType lhs, SynchronizationType rhs) {
        return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
    }

    /**
     * @brief A class to hold properties related to synchronization during the composition of NFTs.
     * This simplifies passing multiple synchronization-related parameters.
     */
    class SynchronizationProperties {
    public:
        const Nft& nft;
        const Level sync_level;
        const size_t num_of_levels_before_sync;
        const size_t num_of_levels_after_sync;
        std::vector<SynchronizationType> sync_types_v;

        SynchronizationProperties(const Nft& nft, const Level sync_level)
            : nft(nft),
              sync_level(sync_level),
              num_of_levels_before_sync(sync_level),
              num_of_levels_after_sync(nft.num_of_levels - sync_level - 1),
              sync_types_v(nft.num_of_states(), SynchronizationType::UNDEFINED)
        {
            get_synchronization_types();
        }

    private:
        /**
         * @brief For each state, sets the type of synchronization that follows.
         *
         * The scope of the synchronization type ends at the next zero-level state.
         * Each state has a synchronization type that is a combination of the
         * synchronization types of its children.
         */
        void get_synchronization_types() {
            const size_t num_of_states = nft.num_of_states();
            for (State root = 0; root < num_of_states; ++root) {
                if (nft.levels[root] != 0) {
                    continue; // Skip states not on the zero level.
                }

                // To perform the computation, which is a post-order traversal, we do not
                // use recursion but a stack for better performance. To determine if a parent state
                // has already been visited, we use the helper value SynchronizationType::UNDER_COMPUTATION.
                std::stack<State> stack;
                stack.push(root);
                while (!stack.empty()) {
                    const State state = stack.top();
                    stack.pop();
                    const Level state_level = nft.levels[state];
                    const StatePost& state_post = nft.delta[state];
                    SynchronizationType current_sync_type = sync_types_v[state];
                    assert(current_sync_type == SynchronizationType::UNDEFINED || current_sync_type == SynchronizationType::UNDER_COMPUTATION);

                    // If it has been visited, then we know that it has already been computed for its children.
                    if (current_sync_type == SynchronizationType::UNDER_COMPUTATION) {
                        // Combine the synchronization types of its children.
                        current_sync_type = SynchronizationType::UNDEFINED;
                        for (const SymbolPost& symbol_post : state_post) {
                            for (const State target : symbol_post.targets) {
                                // Skip fast EPSILON transitions.
                                if (symbol_post.symbol == EPSILON && nft.num_of_levels != 1 && nft.levels[target] == 0) {
                                    assert(state_level == 0);
                                    continue;
                                }
                                assert(nft.levels[target] > nft.levels[state]);
                                current_sync_type |= sync_types_v[target];
                            }
                        }
                        sync_types_v[state] = current_sync_type;
                        continue; // We already visited its children.
                    }

                    // If we are on the sync level, we can determine the synchronization type.
                    if (state_level == sync_level) {
                        const auto epsilon_post_it = state_post.find(EPSILON);
                        if (epsilon_post_it != state_post.end()) {
                            // We don't want to count fast EPSILON transitions.
                            const bool has_not_only_fast_epsilon = (
                                state_level != 0 || nft.num_of_levels == 1 ||
                                std::any_of(
                                    epsilon_post_it->targets.cbegin(),
                                    epsilon_post_it->targets.cend(),
                                    [this](State target) {
                                        return nft.levels[target] != 0;
                                    }
                                )
                            );
                            if (has_not_only_fast_epsilon) {
                                current_sync_type = SynchronizationType::ONLY_ON_EPSILON;
                            }
                            if (state_post.size() > 1) {
                                current_sync_type |= SynchronizationType::ONLY_ON_SYMBOL;
                            }
                        } else if (!state_post.empty()) {
                            current_sync_type = SynchronizationType::ONLY_ON_SYMBOL;
                        }
                        sync_types_v[state] = current_sync_type;
                        continue; // No need to visit its children.
                    }

                    // We need to visit its children.
                    sync_types_v[state] = SynchronizationType::UNDER_COMPUTATION; // Mark this state as under computation.
                    stack.push(state); // Push it back to the stack to compute it later.
                    for (const SymbolPost& symbol_post : state_post) {
                        for (const State target : symbol_post.targets) {
                            // Skip fast EPSILON transitions.
                            if (symbol_post.symbol == EPSILON && nft.num_of_levels != 1 && nft.levels[target] == 0) {
                                assert(state_level == 0);
                                continue;
                            }
                            assert(nft.levels[target] > nft.levels[state]);
                            assert(sync_types_v[target] != SynchronizationType::UNDER_COMPUTATION);
                            if (sync_types_v[target] != SynchronizationType::UNDEFINED) {
                                current_sync_type |= sync_types_v[target];
                            } else {
                                stack.push(target); // Push the target state onto the stack to visit it.
                            }
                        }
                    }
                }
            }

            assert(std::all_of(sync_types_v.begin(), sync_types_v.end(),
                [](SynchronizationType type) { return type != SynchronizationType::UNDER_COMPUTATION; }));
        }
    };
}

namespace mata::nft
{


Nft compose(const Nft& lhs, const Nft& rhs, const Level lhs_sync_level, const Level rhs_sync_level, const bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(lhs_sync_level < lhs.num_of_levels && rhs_sync_level < rhs.num_of_levels);

    // TODO: Modify it to work even with other jump modes.
    if (jump_mode != JumpMode::NoJump) {
        return compose(lhs, rhs, OrdVector<Level>{ lhs_sync_level }, OrdVector<Level>{ rhs_sync_level }, project_out_sync_levels);
    }

    // Check that there are only explicit synchronization transitions of length 1 with exception for fast EPSILON transitions.
    assert(
        std::all_of(
            lhs.delta.transitions().begin(),
            lhs.delta.transitions().end(),
            [&](const Transition& transition) {
                return (lhs.levels[transition.source] < lhs_sync_level && lhs.levels[transition.target] <= lhs_sync_level && lhs.levels[transition.target] != 0) ||
                       (lhs.levels[transition.source] == lhs_sync_level && lhs.levels[transition.target] == static_cast<Level>((lhs_sync_level + 1) % lhs.num_of_levels)) ||
                       (lhs.levels[transition.source] > lhs_sync_level) ||
                       (lhs.levels[transition.source] == 0 && lhs.levels[transition.target] == 0 && transition.symbol == EPSILON);
            }
        )
    );
    // Check that there are only explicit synchronization transitions of length 1 with exception for fast EPSILON transitions.
    assert(
        std::all_of(
            rhs.delta.transitions().begin(),
            rhs.delta.transitions().end(),
            [&](const Transition& transition) {
                return (rhs.levels[transition.source] < rhs_sync_level && rhs.levels[transition.target] <= rhs_sync_level && rhs.levels[transition.target] != 0) ||
                       (rhs.levels[transition.source] == rhs_sync_level && rhs.levels[transition.target] == static_cast<Level>((rhs_sync_level + 1) % rhs.num_of_levels)) ||
                       (rhs.levels[transition.source] > rhs_sync_level) ||
                       (rhs.levels[transition.source] == 0 && rhs.levels[transition.target] == 0 && transition.symbol == EPSILON);
            }
        )
    );

    // Number of Levels and States
    const size_t lhs_num_of_states = lhs.num_of_states();
    const size_t rhs_num_of_states = rhs.num_of_states();
    const size_t result_num_of_levels = lhs.num_of_levels + rhs.num_of_levels - (project_out_sync_levels ? (2) : 1);
    assert(result_num_of_levels > 0);

    // Initialize the synchronization properties for both NFTs.
    const SynchronizationProperties lhs_sync_props(lhs, lhs_sync_level);
    const SynchronizationProperties rhs_sync_props(rhs, rhs_sync_level);

    Nft result;
    result.num_of_levels = result_num_of_levels;
    // Use composition storage without tracking inverted indices,
    // because waiting in virtual states would spoil the inverse mapping.
    TwoDimensionalMap<State, false> composition_storage(lhs_num_of_states, rhs_num_of_states);
    // I use a queue for the worklist to process states in a breadth-first manner.
    // This helps the branch prediction in the CPU because all processed states
    // are likely to be at the same level and thus enter the same branch.
    std::queue<std::pair<State, State>> worklist;

    /**
     * @brief Creates a new composition state for the given pair of states, if it does not already exist.
     *
     * @param first The first state (from lhs or rhs).
     * @param second The second state (from rhs or lhs).
     * @param level The level of the new composition state.
     * @param is_first_lhs If true, @p first is from the lhs NFT; otherwise, it is from the rhs NFT.
     * @param composition_state_to_add If provided, this is the state to be added to the result NFT;
     *                                 if not provided, a new state will be created.
     *
     * @return The composition state for the given pair of states.
     */
    auto create_composition_state = [&](const State first,
                                        const State second,
                                        const Level level,
                                        const bool is_first_lhs = true,
                                        const State composition_state_to_add = Limits::max_state)
    {
        const auto key = is_first_lhs ? std::make_pair(first, second)
                                      : std::make_pair(second, first);

        // Try to find the entry in the state map.
        const State found_state = composition_storage.get(key.first, key.second);
        if (found_state != Limits::max_state) {
            assert(result.levels[found_state] == level);
            return found_state;
        }
        assert(composition_state_to_add == Limits::max_state || result.levels[composition_state_to_add] == level);

        // If not found, add a new state to the result NFT.
        // Since the key pair was not found in the map, we can be certain that the state is not yet in the worklist.
        const State new_state = (composition_state_to_add != Limits::max_state) ? composition_state_to_add
                                                                                : result.add_state_with_level(level);
        composition_storage.insert(key.first, key.second, new_state);
        if (level == 0) {
            // If the level is zero, check for final states and add the state to the worklist.
            if ((is_first_lhs && lhs.final.contains(first) && rhs.final.contains(second)) ||
                (!is_first_lhs && rhs.final.contains(first) && lhs.final.contains(second)))
            {
                result.final.insert(new_state);
            }
        }
        worklist.push(key);
        return new_state;
    };

    /**
     * @brief Perform the synchronization of the LHS and RHS at the given state pair.
     *
     * @param composition_state The state in the composition NFT from which the synchronization result will proceed.
     * @param lhs_state The state in the LHS NFT being synchronized.
     * @param rhs_state The state in the RHS NFT being synchronized.
     * @param reconnect If true, the synchronization will use the specified symbol on the resulting
     *                  synchronization transition, virtually reconnecting the predecessors of
     *                  the synchronization level to its successors; otherwise, it will simply
     *                  synchronize and keep or remove the synchronization level depending on
     *                  the project_out_sync_levels flag.
     * @param reconnection_symbol The symbol to use for the reconnection transition if reconnect is true.
     */
    auto synchronize = [&](const State composition_state,
                           const State lhs_state,
                           const State rhs_state,
                           const bool reconnect = false,
                           const Symbol reconnection_symbol = Limits::max_symbol)
    {
        const Level composition_state_level = result.levels[composition_state];
        const Level composition_target_level = static_cast<Level>((composition_state_level + 1) % result.num_of_levels);
        // When projecting out the synchronization levels, and we do not want to connect their
        // predecessors to their successors, we simply perform the synchronization, note the reached targets,
        // and remove (vanish) the synchronization level and its transition from the result NFT.
        const bool vanish_sync_level = !reconnect && project_out_sync_levels;

        // Helper function to combine LHS and RHS targets over the given symbol.
        auto combine_targets = [&](const StateSet& lhs_sync_targets, const StateSet& rhs_sync_targets, const Symbol symbol) {
            for (const State lhs_sync_target : lhs_sync_targets) {
                for (const State rhs_sync_target : rhs_sync_targets) {
                    if (vanish_sync_level) {
                        create_composition_state(lhs_sync_target,
                                                 rhs_sync_target,
                                                 composition_state_level,
                                                 true,
                                                 composition_state);
                    } else {
                        result.add_transition_with_target(
                            composition_state,
                            symbol,
                            create_composition_state(lhs_sync_target,
                                                     rhs_sync_target,
                                                     composition_target_level,
                                                     true),
                            jump_mode
                        );
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
            const Symbol symbol = reconnect ? reconnection_symbol
                                            : same_symbol_posts[0]->symbol;

            if (same_symbol_posts[0]->symbol == EPSILON) {
                // We need to be careful not to use fast EPSILON transitions.
                OrdVector<State> filtered_lhs_targets;
                if (lhs.levels[lhs_state] == 0 && lhs.num_of_levels != 1) {
                    std::copy_if(
                        same_symbol_posts[0]->targets.cbegin(),
                        same_symbol_posts[0]->targets.cend(),
                        std::back_inserter(filtered_lhs_targets),
                        [&](State s) { return lhs.levels[s] != 0; }
                    );
                }
                OrdVector<State> filtered_rhs_targets;
                if (rhs.levels[rhs_state] == 0 && rhs.num_of_levels != 1) {
                    std::copy_if(
                        same_symbol_posts[1]->targets.cbegin(),
                        same_symbol_posts[1]->targets.cend(),
                        std::back_inserter(filtered_rhs_targets),
                        [&](State s) { return rhs.levels[s] != 0; }
                    );
                }
                combine_targets(filtered_lhs_targets, filtered_rhs_targets, EPSILON);
            } else {
                combine_targets(same_symbol_posts[0]->targets, same_symbol_posts[1]->targets, symbol);
            }
        }

        // Synchronization on DONT_CARE in the LHS.
        auto lhs_dont_care_sync_it = lhs.delta[lhs_state].find(DONT_CARE);
        if (lhs_dont_care_sync_it != lhs.delta[lhs_state].end()) {
            for (const SymbolPost& rhs_symbol_post : rhs.delta[rhs_state]) {
                if (rhs_symbol_post.symbol == EPSILON) {
                    // We don't want to synchronize DONT_CARE with EPSILON.
                    continue;
                }
                const Symbol symbol = reconnect ? reconnection_symbol
                                                : rhs_symbol_post.symbol;
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
                const Symbol symbol = reconnect ? reconnection_symbol
                                                : lhs_symbol_post.symbol;
                combine_targets(lhs_symbol_post.targets, rhs_dont_care_sync_it->targets, symbol);
            }
        }
    };


    /**
     * @brief Copy transitions from the copy NFT to the composition NFT.
     *
     * @param composition_state The source state in the composition NFT from which the transitions will be connected.
     * @param copy_state The state in the copy NFT from which the transitions will be copied.
     * @param stationar_state The state in the other NFT (the stationary NFT) that does not move.
     * @param is_copy_state_lhs If true, the copy_state is from the LHS NFT; otherwise, it is from the RHS NFT.
     * @param waiting_worklist If set, the function is called from the waiting simulation,
     *                         meaning that we are waiting in the virtual loop at the stationary state,
     *                         and we will use the waiting_worklist to store the next state pairs.
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


        // It may happen that we encounter a transition whose target is at the synchronization level,
        // the state in the stationary NFT is also at the synchronization level (or we came from the waiting simulation),
        // project_out_sync_levels is true, and there is no other transition that would be added
        // before the next zero-level state. In this case, we need to handle this synchronization
        // here in place and make the connection directly to the next zero-level state.
        const bool handle_synchronization_in_place = project_out_sync_levels &&
                                                     stationar_sync_props.num_of_levels_after_sync == 0 &&
                                                     copy_sync_props.num_of_levels_after_sync == 0 && (
                                                        waiting_worklist != nullptr ||
                                                        stationar_state_level == stationar_sync_props.sync_level
                                                     );

        for (const SymbolPost& copy_symbol_post : copy_nft.delta[copy_state]) {
            for (const State copy_target : copy_symbol_post.targets) {
                if (copy_symbol_post.symbol == EPSILON && copy_state_level == 0 && copy_nft.num_of_levels != 1 && copy_nft.levels[copy_target] == 0) {
                    // Skip fast EPSILON transitions.
                    continue;
                }

                const SynchronizationType copy_target_sync_type = copy_sync_types[copy_target];
                // It makes sense to continue only if we believe that we will not get stuck
                // later due to an inability to synchronize. When this function is called
                // from the waiting simulation, we are interested in onyl synchronizations on EPSILON.
                const bool can_synchronize_in_the_future = (
                    waiting_worklist != nullptr ? copy_target_sync_type != SynchronizationType::ONLY_ON_SYMBOL
                                                : stationar_state_sync_type == copy_target_sync_type ||
                                                  stationar_state_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL ||
                                                  copy_target_sync_type == SynchronizationType::ON_EPSILON_AND_SYMBOL
                );
                if (!can_synchronize_in_the_future) {
                    // There is no way we would be able to synchronize in the future.
                    // We do not need to explore this path further.
                    continue;
                }

                const Level target_level = copy_nft.levels[copy_target];
                const size_t trans_len = (target_level == 0 ? copy_nft.num_of_levels : target_level) - copy_state_level;
                assert(target_level == 0 || target_level > copy_state_level);
                assert(trans_len > 0);

                if (handle_synchronization_in_place && target_level == copy_sync_level) {
                    // The target is at the synchronization level that will be projected out.
                    // We know that there are no transitions after the synchronization level
                    // in any of the NFTs. Therefore, we must perform the synchronization here
                    // and connect this transition directly to the next zero-level state.
                    if (waiting_worklist != nullptr) {
                        // We have been called from the waiting simulation.
                        // We are waiting in the stationary state.
                        // During this, only EPSILON synchronizations are allowed.
                        const auto& copy_eps_symbol_post = copy_nft.delta[copy_target].find(EPSILON);
                        assert(copy_eps_symbol_post != copy_nft.delta[copy_target].end());
                        for (const State sync_target : copy_eps_symbol_post->targets) {
                            assert(copy_nft.levels[sync_target] == 0);
                            result.add_transition_with_target(
                                composition_state,
                                copy_symbol_post.symbol,
                                create_composition_state(sync_target,
                                                         stationar_state,
                                                         0,
                                                         is_copy_state_lhs),
                                jump_mode
                            );
                        }
                        // We do not need to add new pairs to the waiting_worklist,
                        // because, if necessary, they have already been added to the main
                        // worklist by the create_composition_state function.
                    } else {
                        // We encountered a standard synchronization and need to synchronize the LHS and RHS.
                        // Call the synchronization function with the reconnection parameters set.
                        // This point can be reached only if there is not transition in the LHS and we are
                        // copying transitions from the RHS, or if the synchronization in the RHS occurs
                        // on the 0-level, and we are copying transitions from the LHS.
                        assert(!is_copy_state_lhs || rhs_sync_level == 0);
                        if (!is_copy_state_lhs) {
                            synchronize(composition_state, stationar_state, copy_target, true, copy_symbol_post.symbol);
                        } else {
                            synchronize(composition_state, copy_target, stationar_state, true, copy_symbol_post.symbol);
                        }
                    }
                } else {
                    // We are not at a synchronization point.
                    // We just need to copy the transition.
                    const Level new_composition_state_level = static_cast<Level>((composition_state_level + trans_len) % result.num_of_levels);
                    if (waiting_worklist != nullptr && new_composition_state_level != 0) {
                        // We are not connecting to the zero-level state, and we were
                        // called from the waiting simulation. Therefore, we need to create an auxiliary state
                        // that will not be tracked in the composition_storage. We cannot use composition_storage
                        // because, while waiting in the stationary state, we are not exactly at that state,
                        // but rather in virtual (nonexistent) states of the waiting loop over it.
                        const State new_composition_state = result.add_state_with_level(new_composition_state_level);
                        result.add_transition_with_target(composition_state, copy_symbol_post.symbol, new_composition_state, jump_mode);
                        // Because we are waiting, use the waiting_worklist.
                        waiting_worklist->push({ new_composition_state, stationar_state });
                    } else {
                        // Just copy this transition.
                        assert(waiting_worklist == nullptr);
                        result.add_transition_with_target(
                            composition_state,
                            copy_symbol_post.symbol,
                            create_composition_state(copy_target,
                                                     stationar_state,
                                                     new_composition_state_level,
                                                     is_copy_state_lhs),
                            jump_mode
                        );
                    }
                }
            }
        }
    };

    /**
     * @brief Models the "waiting" in one of the NFTs when it cannot synchronize
     * with the other NFT doe to a lacking EPSILON on the synchronization level.
     *
     * For example, if there is an EPSILON transition at the synchronization level
     * in the RHS, and the LHS cannot synchronize on it, then the LHS must "wait"
     * in its root (zero-level) state that precedes the synchronization. Meanwhile,
     * the RHS continues from its corresponding root state, traversing the problematic
     * EPSILON synchronization to the next zero-level state. The RHS behaves as if it
     * is interleaving with the LHS transitions (LHS transitions always occur before RHS
     * transitions), but it will insert an EPSILON for each such transition from the LHS.
     *
     * A special case arises when we want to project out synchronization levels,
     * but there are no transitions following them in either the LHS or RHS (i.e.,
     * the synchronization level is the last level in both NFTs). In this situation,
     * one state before the synchronization level, we need to connect the successors
     * of such states to the zero-level state that follows the synchronization level.
     *
     * @param composition_root_state The root state of the composition NFT.
     * @param waiting_root_state The root state of the NFT that is waiting (either LHS or RHS).
     * @param running_root_state The root state of the NFT that is running (either LHS or RHS).
     * @param is_lhs_waiting If true, the waiting root is from the LHS;
     *                       otherwise, it is from the RHS.
     */
    auto model_waiting = [&](const State composition_root_state,
                             const State waiting_root_state,
                             const State running_root_state,
                             const bool is_lhs_waiting)
    {
        const SynchronizationProperties& running_sync_props = is_lhs_waiting ? rhs_sync_props
                                                                             : lhs_sync_props;
        const SynchronizationProperties& waiting_sync_props = is_lhs_waiting ? lhs_sync_props
                                                                             : rhs_sync_props;
        const Level sync_level = running_sync_props.sync_level;
        const Levels& running_levels = running_sync_props.nft.levels;
        const Delta& running_delta = running_sync_props.nft.delta;
        const size_t running_num_of_levels = running_sync_props.nft.num_of_levels;
        const size_t waiting_trans_before_sync = waiting_sync_props.num_of_levels_before_sync;
        const size_t waiting_trans_after_sync = waiting_sync_props.num_of_levels_after_sync;
        // EPSILON synchronization is handled differently here than in the main loop.
        // To avoid duplicate transitions and speed up the algorithm, we replace each
        // EPSILON synchronization with N EPSILON transitions, where:
        // - Add 1 to N if we are NOT projecting out synchronization levels.
        // - Add waiting_trans_before_sync to N if waiting in RHS
        //   (RHS transitions go after LHS, just before the synchronization).
        // - Add waiting_trans_after_sync to N if waiting in LHS
        //   (LHS transitions go before RHS, just after the synchronization).
        const size_t num_of_epsilons_to_replace_sync = (
            (is_lhs_waiting ? waiting_trans_after_sync : waiting_trans_before_sync) +
            (project_out_sync_levels ? 0 : 1)
        );

        // Initialization of the local worklist.
        // I chose to use a queue instead of a stack to help the branch predictor
        // with conditions inside the loop. With the queue, the algorithm hits the
        // same branch more ofthen, because all states being explored advance "together".
        std::queue<std::pair<State, State>> worklist;
        if (is_lhs_waiting && waiting_trans_before_sync > 0) {
            // Since LHS is waiting and LHS transitions have to be put before
            // RHS transitions, we now add waiting_trans_before_sync EPSILON
            // transitions. Doing this here simplifies the conditions in the loop below.
            const State new_composition_state = result.add_transition_with_lenght(composition_root_state, EPSILON, waiting_trans_before_sync, jump_mode);
            worklist.push({ std::move(new_composition_state), running_root_state });
        } else {
            worklist.push({ composition_root_state, running_root_state });
        }

        // The main loop of the waiting simulation.
        while (!worklist.empty()) {
            auto [composition_state, running_state] = worklist.front();
            worklist.pop();
            const Level running_state_level = running_levels[running_state];

            if (running_state_level == sync_level) {
                // We are at the synchronization level.
                const bool is_last_running_transition = (running_state_level == running_sync_props.nft.num_of_levels - 1);

                // It cannot happen that the synchronization level is projected out
                // and there are no other transitions to addbefore reaching the next zero-level state.
                // This is because such a scenario would already be handled by the copy_transition function.
                assert(!project_out_sync_levels || !is_last_running_transition || waiting_trans_after_sync > 0);

                if (num_of_epsilons_to_replace_sync > 1) {
                    // The synchronization will be replaced by an EPSILON transition anyway.
                    // To reduce the number of redundant EPSILON transitions in the resulting NFT,
                    // we can add N-1 EPSILON transitions now, and then use the last one to connect
                    // to the adequate target state, where N is num_of_epsilons_to_replace_sync.
                    composition_state = result.add_transition_with_lenght(composition_state, EPSILON, num_of_epsilons_to_replace_sync - 1, jump_mode);
                }

                // Do the EPSILON synchronization.
                // We cannot use the synchronize function here, because we want to
                // focus only on the EPSILON transitions.
                const auto& running_epsilon_post_it = running_delta[running_state].find(EPSILON);
                // There should be an EPSILON transition. It has been told by the SynchronizationType.
                assert(running_epsilon_post_it != running_delta[running_state].end());
                for (const State running_epsilon_target : running_epsilon_post_it->targets) {
                    if (running_state_level == 0 && running_num_of_levels != 1 && running_levels[running_epsilon_target]) {
                        // Skip fast EPSILON transitions.
                        continue;
                    }
                    assert(running_levels[running_epsilon_target] == 0 || running_levels[running_epsilon_target] > running_state_level);
                    if (num_of_epsilons_to_replace_sync == 0) {
                        // This synchronization level is being projected out (it has vanished).
                        // There are no EPSILON transitions to add before or after the synchronization level.
                        // However, we are sure, that there will be a least one "running" transition after this.
                        assert(!is_last_running_transition);
                        worklist.push({ composition_state, running_epsilon_target });
                    } else if (is_last_running_transition) {
                        // We are connecting to the zero-level state, therefore we have to
                        // create a new composition state that will be put into the main worklist
                        // (side effect of the create_composition_state).
                        assert(result.levels[composition_state] + 1 == result.num_of_levels);
                        result.add_transition_with_target(
                            composition_state,
                            EPSILON,
                            create_composition_state(waiting_root_state,
                                                     running_epsilon_target,
                                                     0,
                                                     is_lhs_waiting),
                            jump_mode
                        );
                    } else {
                        // We are connecting to the next state in the waiting loop,
                        // so we simply add a transition to the next state and
                        // continue the waiting
                        const State new_composition_state = result.add_transition_with_lenght(composition_state, EPSILON, 1, jump_mode);
                        worklist.push({ new_composition_state, running_epsilon_target });
                    }
                }
            } else if (running_state_level == 0 && running_state != running_root_state) {
                // We are at a leaf zero-level state in the running NFT.
                // We will create a connection to the zero-level state in the composition NFT
                // and continue exploring that state in the main composition loop
                // (i.e., we push it to the main worklist, if not already visited).
                // Note: This situation can only occur if the waiting NFT is the RHS,
                // because RHS transitions follow LHS transitions. Moreover, the last
                // transition in the running LHS NFT should not be at the synchronization
                // level; otherwise, it would be handled by the condition above.
                assert(!is_lhs_waiting);
                assert(sync_level != running_sync_props.nft.num_of_levels - 1);
                assert(waiting_trans_after_sync > 0);
                assert((running_state_level + waiting_trans_after_sync) % result.num_of_levels == 0);
                result.add_transition_with_target(
                    composition_state,
                    EPSILON,
                    create_composition_state(running_state,
                                             running_root_state,
                                             0,
                                             is_lhs_waiting),
                    jump_mode
                );
            } else {
                // We are at an internal (non-sync) level in the running NFT.
                // We can simply copy the transitions to the next states.
                // We do not need to worry that the target state might be at the
                // synchronization level with no further transitions in either
                // the LHS or RHS after it, because such cases will be handled
                // inside the copy_transition function.
                copy_transition(composition_state, running_state, waiting_root_state, !is_lhs_waiting, &worklist);
            }
        }
    };

    /**
     * @brief Process potential fast EPSILON transitions (transitions over EPSILON,
     * between two zero-level states).
     *
     * @param composition_state The state in the resulting composition NFT.
     * @param lhs_src The source state of a potential EPSILON transition in the LHS NFT.
     * @param rhs_src The source state of a potential EPSILON transition in the RHS NFT.
     */
    auto process_fast_epsilon_transitions = [&](const State composition_state, const State lhs_src, const State rhs_src) {
        assert(lhs.levels[lhs_src] == 0 && rhs.levels[rhs_src] == 0);
        const auto lhs_eps_post_it = lhs.delta[lhs_src].find(EPSILON);
        const auto rhs_eps_post_it = rhs.delta[rhs_src].find(EPSILON);
        const bool lhs_eps_exists = lhs_eps_post_it != lhs.delta[lhs_src].end();
        const bool rhs_eps_exists = rhs_eps_post_it != rhs.delta[rhs_src].end();

        // Create combinations of rhs_src with all zero-level targets of lhs_src.
        if (lhs_eps_exists) {
            for (const State lhs_target : lhs_eps_post_it->targets) {
                if (lhs.levels[lhs_target] != 0) {
                    continue;
                }
                create_composition_state(lhs_target, rhs_src, 0, true, composition_state);
            }
        }

        // Create combinations of lhs_src with all zero-level targets of rhs_src.
        if (rhs_eps_exists) {
            for (const State rhs_target : rhs_eps_post_it->targets) {
                if (rhs.levels[rhs_target] != 0) {
                    continue;
                }
                create_composition_state(lhs_src, rhs_target, 0, true, composition_state);
            }
        }

        // Create combinations of all zero-level targets of lhs_src with all zero-level targets of rhs_src.
        if (lhs_eps_exists && rhs_eps_exists) {
            for (const State lhs_target : lhs_eps_post_it->targets) {
                if (lhs.levels[lhs_target] != 0) {
                    continue;
                }
                for (const State rhs_target : rhs_eps_post_it->targets) {
                    if (rhs.levels[rhs_target] != 0) {
                        continue;
                    }
                    create_composition_state(lhs_target, rhs_target, 0, true, composition_state);
                }
            }
        }
    };

    // Initialization of the main worklist.
    for (const State lhs_root: lhs.initial) {
        for (const State rhs_root: rhs.initial) {
            // Get the root state in the result NFT
            result.initial.insert(create_composition_state(lhs_root, rhs_root, 0));
        }
    }

    // The maing loop of the composition algorithm.
    while (!worklist.empty()) {
        const auto [lhs_state, rhs_state] = worklist.front();
        worklist.pop();
        const State composition_state = composition_storage.get(lhs_state, rhs_state);
        assert(composition_state != Limits::max_state);
        const Level lhs_level = lhs.levels[lhs_state];
        const Level rhs_level = rhs.levels[rhs_state];
        const SynchronizationType lhs_sync_type = lhs_sync_props.sync_types_v[lhs_state];
        const SynchronizationType rhs_sync_type = rhs_sync_props.sync_types_v[rhs_state];

        if (lhs_level == 0 and rhs_level == 0) {
            // We are at the zero-level states before any synchronization level.
            // Now is the time to decide whether to continue synchronization and/or
            // to wait in the lhs_state and/or rhs_state. This is necessary when
            // there is a synchronization where one of the transducers has an EPSILON
            // and the other has an alphabet symbol. Note that if future synchronization
            // is impossible due to the EPSILON, we will simulate waiting in the zero-level
            // state of the transducer that does not have that EPSILON.

            // Process potential fast EPSILON transitions.
            process_fast_epsilon_transitions(composition_state, lhs_state, rhs_state);

            // You can see the table defining operations for pair of synchronization types
            // in the documentation of the SynchronizationType enum class.
            const bool perform_wait_on_lhs = exist_intersection_of_sync_types(rhs_sync_type, SynchronizationType::ONLY_ON_EPSILON);
            const bool perform_wait_on_rhs = exist_intersection_of_sync_types(lhs_sync_type, SynchronizationType::ONLY_ON_EPSILON);
            const bool can_synchronize_in_the_future = exist_intersection_of_sync_types(lhs_sync_type, rhs_sync_type);
            if (perform_wait_on_lhs) {
                // LHS is waiting (i.e., there is an EPSILON in the RHS that LHS can not synchronize on).
                model_waiting(composition_state, lhs_state, rhs_state, true);
            }
            if (perform_wait_on_rhs) {
                // RHS is waiting (i.e., there is an EPSILON in the LHS that RHS can not synchronize on).
                model_waiting(composition_state, rhs_state, lhs_state, false);
            }
            if (!can_synchronize_in_the_future) {
                // There is no way to perform the synchronization after these states.
                // Therefore, it does not make sense to continue exploring this pair.
                continue;
            }
        }

        // It makes sense to continue only if we believe that synchronization is possible,
        // or if we have already passed the synchronization level (i.e., lhs_sync_type ==
        // rhs_sync_type == SynchronizationType::UNDEFINED).
        assert(exist_intersection_of_sync_types(lhs_sync_type, rhs_sync_type) ||
               (lhs_level == 0 && rhs_level > 0 && rhs_sync_type == SynchronizationType::UNDEFINED) ||
               (rhs_level == 0 && lhs_level > 0 && lhs_sync_type == SynchronizationType::UNDEFINED)
        );

        // Both LHS and RHS can take a step if they are not at the synchronization level.
        // The LHS always moves before the RHS (i.e., lhs_level < rhs_level).
        // However, there is a special case when lhs_level < rhs_level, but the LHS cannot move.
        // This occurs when the LHS has already reached its last (leave) zero-level state,
        // which happens if there are no transitions after the synchronization level in the LHS.
        const bool do_step_in_lhs = lhs_level != lhs_sync_level && (lhs_level != 0 || rhs_level == 0);
        const bool do_step_in_rhs = rhs_level != rhs_sync_level;

        if (do_step_in_lhs) {
            // Always try to do the step in the LHS first.
            // We don't need to worry about the case when this is the last
            // transition in the LHS, the RHS is already at the synchronization level,
            // and we are projecting out the synchronization levels.
            // In such situation, the copy_transition function will handle it.
            copy_transition(composition_state, lhs_state, rhs_state, true);
        } else if (do_step_in_rhs) {
            // LHS is probably at the synchronization level or at the last (leave) zero-level state.
            // It's time for RHS to make a step.
            // We don't need to worry about the case when this is the last
            // transition in the RHS, the LHS is already at the synchronization level,
            // and we are projecting out the synchronization levels.
            // In such situation, the copy_transition function will handle it.
            copy_transition(composition_state, rhs_state, lhs_state, false);
        } else {
            // Both LHS and RHS are at the synchronization level.
            // Synchronization at this point is only possible if:
            // 1) There is at least one transition in either the LHS or
            //    RHS that follows the synchronization level
            //    (so we can connect to their successors), or
            // 2) We are not projecting out the synchronization levels.
            //    In this case, we use the synchronization transition to
            //    connect to the zero-level successors that follow.
            assert(lhs_level == lhs_sync_level && rhs_level == rhs_sync_level);
            assert(!project_out_sync_levels ||
                   lhs_sync_props.num_of_levels_after_sync > 0 ||
                   rhs_sync_props.num_of_levels_after_sync > 0);
            synchronize(composition_state, lhs_state, rhs_state);
        }
    }

    // Cannot do the trim to remove dead ends,
    // because some algorithms work with useless states.
    return result;
}

Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, bool project_out_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    // if (lhs_sync_levels.size() == 1 && rhs_sync_levels.size() == 1) {
    //     // If we have only one synchronization level we can do it faster.
    //     return compose(lhs, rhs, lhs_sync_levels.front(), lhs_sync_levels.front(), project_out_sync_levels, jump_mode);
    // }

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
