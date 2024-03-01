/* nft-intersection.cc -- Intersection of NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"

#include <fstream>
#include <cassert>
#include <functional>


using namespace mata::nft;

namespace {

using ProductMap = std::unordered_map<std::pair<State,State>,State>;
using MatrixProductStorage = std::vector<std::vector<State>>;
using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
using InvertedProductStorage = std::vector<State>;
//Unordered map seems to be faster than ordered map here, but still very much slower than matrix.

} // Anonymous namespace.

namespace mata::nft {

Nft intersection(const Nft& lhs, const Nft& rhs, ProductMap *prod_map, const State lhs_first_aux_state, const State rhs_first_aux_state) {

    auto both_final = [&](const State lhs_state,const State rhs_state) {
        return lhs.final.contains(lhs_state) && rhs.final.contains(rhs_state);
    };

    if (lhs.final.empty() || lhs.initial.empty() || rhs.initial.empty() || rhs.final.empty())
        return Nft{};

    return algorithms::product(lhs, rhs, both_final, prod_map, lhs_first_aux_state, rhs_first_aux_state);
}

//TODO: move this method to nft.hh? It is something one might want to use (e.g. for union, inclusion, equivalence of DFAs).
Nft mata::nft::algorithms::product(const Nft& lhs, const Nft& rhs, const std::function<bool(State,State)>&& final_condition, ProductMap *product_map, const State lhs_first_aux_state, const State rhs_first_aux_state) {

    Nft product{}; // The product automaton.
    product.levels_cnt = lhs.levels_cnt;

    // Set of product states to process.
    std::deque<State> worklist{};

    //The largest matrix (product_matrix) of pairs of states we are brave enough to allocate.
    // Let's say we are fine with allocating large_product * (about 8 Bytes) space.
    // So ten million cells is close to 100 MB.
    // If the number is larger, then we do not allocate a matrix, but use a vector of unordered maps (product_vec_map).
    // The unordered_map seems to be about twice slower.
    constexpr size_t MAX_PRODUCT_MATRIX_SIZE = 50'000'000;
    //constexpr size_t MAX_PRODUCT_MATRIX_SIZE = 0;
    const bool large_product = lhs.num_of_states() * rhs.num_of_states() > MAX_PRODUCT_MATRIX_SIZE;
    assert(lhs.num_of_states() < Limits::max_state);
    assert(rhs.num_of_states() < Limits::max_state);
    assert(lhs.levels_cnt == rhs.levels_cnt);

    //Two variants of storage for the mapping from pairs of lhs and rhs states to product state, for large and non-large products.
    MatrixProductStorage matrix_product_storage;
    VecMapProductStorage vec_map_product_storage;
    InvertedProductStorage product_to_lhs(lhs.num_of_states()+rhs.num_of_states());
    InvertedProductStorage product_to_rhs(lhs.num_of_states()+rhs.num_of_states());


    //Initialize the storage, according to the number of possible state pairs.
    if (!large_product)
        matrix_product_storage = MatrixProductStorage(lhs.num_of_states(), std::vector<State>(rhs.num_of_states(), Limits::max_state));
    else
        vec_map_product_storage = VecMapProductStorage(lhs.num_of_states());

    /// Give me the product state for the pair of lhs and rhs states.
    /// Returns Limits::max_state if not found.
    auto get_state_from_product_storage = [&](State lhs_state, State rhs_state) {
        if (!large_product)
            return matrix_product_storage[lhs_state][rhs_state];
        else {
            auto it = vec_map_product_storage[lhs_state].find(rhs_state);
            if (it == vec_map_product_storage[lhs_state].end())
                return Limits::max_state;
            else
                return it->second;
        }
    };

    /// Insert new mapping lhs rhs state pair to product state.
    auto insert_to_product_storage = [&](State lhs_state, State rhs_state, State product_state) {
        if (!large_product)
            matrix_product_storage[lhs_state][rhs_state] = product_state;
        else
            vec_map_product_storage[lhs_state][rhs_state] = product_state;

        product_to_lhs.resize(product_state+1);
        product_to_rhs.resize(product_state+1);
        product_to_lhs[product_state] = lhs_state;
        product_to_rhs[product_state] = rhs_state;

        //this thing is not used internally. It is only used if we want to return the mapping. But it is expensive.
        if (product_map != nullptr)
            (*product_map)[std::pair<State,State>(lhs_state,rhs_state)] = product_state;
    };

/**
 * Create product state if it does not exist in storage yet and fill in its symbol_post from lhs and rhs targets.
 * @param[in] lhs_target Target state in NFT @c lhs.
 * @param[in] rhs_target Target state in NFT @c rhs.
 * @param[out] product_symbol_post New SymbolPost of the product state.
 */
    auto create_product_state_and_symbol_post = [&](const State lhs_target, const State rhs_target, SymbolPost& product_symbol_post)
    {
        // Two auxiliary states can not create a product state.
        if (lhs_first_aux_state <= lhs_target && rhs_first_aux_state <= rhs_target) {
            return;
        }
        State product_target = get_state_from_product_storage(lhs_target, rhs_target );

        if ( product_target == Limits::max_state )
        {
            product_target = product.add_state();

            // The level of the newly created product state will be the minimum level from
            // the pair of original states. However, if only one of them is zero (in this case meaning
            // that this state is on the last/highest level), the nonzero (theoretically smaller)
            // level will be chosen for the product state.
            if (lhs.levels[lhs_target] == 0) {
                product.levels[product_target] = rhs.levels[rhs_target];
            } else if (rhs.levels[rhs_target] == 0) {
                product.levels[product_target] = lhs.levels[lhs_target];
            } else {
                product.levels[product_target] = std::min(lhs.levels[lhs_target], rhs.levels[rhs_target]);
            }
            assert(product_target < Limits::max_state);

            insert_to_product_storage(lhs_target,rhs_target, product_target);

            worklist.push_back(product_target);

            if (final_condition(lhs_target,rhs_target)) {
                product.final.insert(product_target);
            }
        }
        //TODO: Push_back all of them and sort at the could be faster.
        product_symbol_post.insert(product_target);
    };

    // If DONT_CARE is not present in the given dcare_state_post, no action is taken.
    // For each transition in specific_state_post and each target found in dcare_state_post using find(DONT_CARE),
    // a corresponding transition and product state are created.
    auto process_dont_care = [&](const StatePost& dcare_state_post,
                                 const StatePost& specific_state_post,
                                 const bool dcare_on_lhs,
                                 const State product_source)
    {
        auto dcare_symbol_post_it = dcare_state_post.find(DONT_CARE);
        if (dcare_symbol_post_it == dcare_state_post.end()) {
            return;
        }
        for (const auto &specific_symbol_post : specific_state_post) {
            SymbolPost product_symbol_post{ specific_symbol_post.symbol };
            for (const State dcare_target : dcare_symbol_post_it->targets) {
                for (const State specific_target : specific_symbol_post.targets) {
                    if (dcare_on_lhs) {
                        create_product_state_and_symbol_post(dcare_target, specific_target, product_symbol_post);
                    } else {
                        create_product_state_and_symbol_post(specific_target, dcare_target, product_symbol_post);
                    }
                }
            }
            if (product_symbol_post.empty()) {
                continue;
            }
            StatePost &product_state_post{product.delta.mutable_state_post(product_source)};
            const auto product_state_post_find_it = product_state_post.find(product_symbol_post.symbol);
            if (product_state_post_find_it == product_state_post.end()) {
                product_state_post.insert(std::move(product_symbol_post));
            } else {
                product_state_post_find_it->targets.insert(product_symbol_post.targets);
            }
        }
    };

    // Initialize pairs to process with initial state pairs.
    for (const State lhs_initial_state : lhs.initial) {
        for (const State rhs_initial_state : rhs.initial) {
            // Update product with initial state pairs.
            const State product_initial_state = product.add_state();
            insert_to_product_storage(lhs_initial_state,rhs_initial_state,product_initial_state);
            worklist.push_back(product_initial_state);
            product.initial.insert(product_initial_state);
            if (final_condition(lhs_initial_state,rhs_initial_state)) {
                product.final.insert(product_initial_state);
            }
        }
    }

    while (!worklist.empty()) {
        State product_source = worklist.back();;
        worklist.pop_back();
        State lhs_source =  product_to_lhs[product_source];
        State rhs_source =  product_to_rhs[product_source];

        if (lhs.levels[lhs_source] == rhs.levels[rhs_source]) {
            // Compute classic product for current state pair.
            mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
            mata::utils::push_back(sync_iterator, lhs.delta[lhs_source]);
            mata::utils::push_back(sync_iterator, rhs.delta[rhs_source]);
            while (sync_iterator.advance()) {
                const std::vector<StatePost::const_iterator>& same_symbol_posts{ sync_iterator.get_current() };
                assert(same_symbol_posts.size() == 2); // One move per state in the pair.

                // Compute product for state transitions with same symbols.
                // Find all transitions that have the same symbol for first and the second state in the pair_to_process.
                // Create transition from the pair_to_process to all pairs between states to which first transition goes
                //  and states to which second one goes.
                Symbol symbol = same_symbol_posts[0]->symbol;
                SymbolPost product_symbol_post{ symbol };
                for (const State lhs_target: same_symbol_posts[0]->targets) {
                    for (const State rhs_target: same_symbol_posts[1]->targets) {
                        create_product_state_and_symbol_post(lhs_target, rhs_target, product_symbol_post);
                    }
                }
                if (product_symbol_post.empty()) {
                    continue;
                }
                StatePost &product_state_post{product.delta.mutable_state_post(product_source)};
                //Here we are sure that we are working with the largest symbol so far, since we iterate through
                //the symbol posts of the lhs and rhs in order. So we can just push_back (not insert).
                product_state_post.push_back(std::move(product_symbol_post));
            }

            process_dont_care(lhs.delta[lhs_source], rhs.delta[rhs_source], true, product_source);
            process_dont_care(rhs.delta[rhs_source], lhs.delta[lhs_source], false, product_source);

        } else if ((lhs.levels[lhs_source] < rhs.levels[rhs_source] && lhs.levels[lhs_source] != 0) || rhs.levels[rhs_source] == 0) {
            // The second state (from rhs) is deeper, so it must wait.
            for (const auto &symbol_post : lhs.delta[lhs_source]) {
                SymbolPost product_symbol_post{ symbol_post.symbol };
                for (const State target : symbol_post.targets) {
                    create_product_state_and_symbol_post(target, rhs_source, product_symbol_post);
                }
                if (product_symbol_post.empty()) {
                    continue;
                }
                StatePost &product_state_post{product.delta.mutable_state_post(product_source)};
                product_state_post.push_back(std::move(product_symbol_post));
            }
        } else {
            // The first state (from lhs) is deeper, so it must wait.
            for (const auto &symbol_post : rhs.delta[rhs_source]) {
                SymbolPost product_symbol_post{ symbol_post.symbol };
                for (const State target : symbol_post.targets) {
                    create_product_state_and_symbol_post(lhs_source, target, product_symbol_post);
                }
                if (product_symbol_post.empty()) {
                    continue;
                }
                StatePost &product_state_post{product.delta.mutable_state_post(product_source)};
                product_state_post.push_back(std::move(product_symbol_post));
            }
        }

    }
    return product;
} // intersection().

} // namespace mata::nft.
