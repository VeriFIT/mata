/* nfa-intersection.cc -- Intersection of NFAs
 */

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include <cassert>
#include <functional>


using namespace mata::nfa;

namespace {

using ProductMap = std::unordered_map<std::pair<State,State>,State>;
using MatrixProductStorage = std::vector<std::vector<State>>;
using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
using InvertedProductStorage = std::vector<State>;
//Unordered map seems to be faster than ordered map here, but still very much slower than matrix.

} // Anonymous namespace.

namespace mata::nfa {

Nfa intersection(const Nfa& lhs, const Nfa& rhs, const Symbol first_epsilon, ProductMap *prod_map) {

    auto both_final = [&](const State lhs_state,const State rhs_state) {
        return lhs.final.contains(lhs_state) && rhs.final.contains(rhs_state);
    };

    if (lhs.final.empty() || lhs.initial.empty() || rhs.initial.empty() || rhs.final.empty())
        return Nfa{};

    return algorithms::product(lhs, rhs, both_final, first_epsilon, prod_map);
}

//TODO: move this method to nfa.hh? It is something one might want to use (e.g. for union, inclusion, equivalence of DFAs).
Nfa mata::nfa::algorithms::product(
        const Nfa& lhs, const Nfa& rhs, const std::function<bool(State,State)>&& final_condition,
        const Symbol first_epsilon, ProductMap *product_map) {

    Nfa product{}; // The product automaton.

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
 * Add symbol_post for the product state (lhs,rhs) to the product, used for epsilons only (it is simpler for normal symbols).
 * @param[in] pair_to_process Currently processed pair of original states.
 * @param[in] new_product_symbol_post State transitions to add to the product.
 */
    auto add_product_e_post = [&](const State lhs_source, const State rhs_source, SymbolPost& new_product_symbol_post)
    {
        if (new_product_symbol_post.empty()) { return; }

        State product_source = get_state_from_product_storage(lhs_source, rhs_source);

        StatePost &product_state_post{product.delta.mutable_state_post(product_source)};

        if (product_state_post.empty() || new_product_symbol_post.symbol > product_state_post.back().symbol) {
            product_state_post.push_back(std::move(new_product_symbol_post));
        }
        else {
            auto symbol_post_it = product_state_post.find(new_product_symbol_post.symbol);
            if (symbol_post_it == product_state_post.end()) {
                product_state_post.insert(std::move(new_product_symbol_post));
            }
            //Epsilons are not inserted in order, we insert all lhs epsilons and then all rhs epsilons.
            // It can happen that we insert an e-transition from lhs and then another with the same e from rhs.
            else {
                symbol_post_it->insert(new_product_symbol_post.targets);
            }
        }
    };

/**
 * Create product state if it does not exist in storage yet and fill in its symbol_post from lhs and rhs targets.
 * @param[in] lhs_target Target state in NFA @c lhs.
 * @param[in] rhs_target Target state in NFA @c rhs.
 * @param[out] product_symbol_post New SymbolPost of the product state.
 */
    auto create_product_state_and_symbol_post = [&](const State lhs_target, const State rhs_target, SymbolPost& product_symbol_post)
    {
        State product_target = get_state_from_product_storage(lhs_target, rhs_target );

        if ( product_target == Limits::max_state )
        {
            product_target = product.add_state();
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
            if (symbol < first_epsilon) {
                SymbolPost product_symbol_post{ symbol };
                for (const State lhs_target: same_symbol_posts[0]->targets) {
                    for (const State rhs_target: same_symbol_posts[1]->targets) {
                        create_product_state_and_symbol_post(lhs_target, rhs_target, product_symbol_post);
                    }
                }
                StatePost &product_state_post{product.delta.mutable_state_post(product_source)};
                //Here we are sure that we are working with the largest symbol so far, since we iterate through
                //the symbol posts of the lhs and rhs in order. So we can just push_back (not insert).
                product_state_post.push_back(std::move(product_symbol_post));
            }
            else
                break;
        }

        // Add epsilon transitions, from lhs e-transitions.
        const StatePost& lhs_state_post{lhs.delta[lhs_source] };

        //TODO: handling of epsilons might not be ideal, don't know, it would need some brain cycles to improve.
        // (handling of normal symbols is ok though)
        auto lhs_first_epsilon_it = lhs_state_post.first_epsilon_it(first_epsilon);
        if (lhs_first_epsilon_it != lhs_state_post.end()) {
            for (auto lhs_symbol_post = lhs_first_epsilon_it; lhs_symbol_post < lhs_state_post.end(); ++lhs_symbol_post) {
                SymbolPost prod_symbol_post{lhs_symbol_post->symbol };
                for (const State lhs_target: lhs_symbol_post->targets) {
                    create_product_state_and_symbol_post(lhs_target, rhs_source, prod_symbol_post);
                }
                add_product_e_post(lhs_source, rhs_source, prod_symbol_post);
            }
        }

        // Add epsilon transitions, from rhs e-transitions.
        const StatePost& rhs_state_post{rhs.delta[rhs_source] };
        auto rhs_first_epsilon_it = rhs_state_post.first_epsilon_it(first_epsilon);
        if (rhs_first_epsilon_it != rhs_state_post.end()) {
            for (auto rhs_symbol_post = rhs_first_epsilon_it; rhs_symbol_post < rhs_state_post.end(); ++rhs_symbol_post) {
                SymbolPost prod_symbol_post{rhs_symbol_post->symbol };
                for (const State rhs_target: rhs_symbol_post->targets) {
                    create_product_state_and_symbol_post(lhs_source, rhs_target, prod_symbol_post);
                }
                add_product_e_post(lhs_source, rhs_source, prod_symbol_post);
            }
        }
    }
    return product;
} // intersection().

} // namespace mata::nfa.
