/* cntnfa-intersection.cc -- Intersection of NFAs
 */

// MATA headers
#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/algorithms.hh"
#include <cassert>
#include <functional>


using namespace mata::cntnfa;

namespace {

using ProductMap = std::unordered_map<std::pair<State,State>,State>;
using MatrixProductStorage = std::vector<std::vector<State>>;
using VecMapProductStorage = std::vector<std::unordered_map<State,State>>;
using InvertedProductStorage = std::vector<State>;
//Unordered map seems to be faster than ordered map here, but still very much slower than matrix.

} // Anonymous namespace.

namespace mata::cntnfa {

Cntnfa mata::cntnfa::algorithms::product_counter_nfas(const Cntnfa& lhs, const Cntnfa& rhs) {
    using StatePair = std::pair<State, State>;
    Cntnfa result;

    // Maps state pairs (lhs × rhs) to result states
    std::map<StatePair, State> state_map;
    // Queue to process pairs (BFS-like)
    std::deque<StatePair> worklist;

    // Helper to get or create a state in the result automaton for a pair of lhs and rhs states
    auto get_or_create_state = [&](State lhs_s, State rhs_s) -> State {
        StatePair p = {lhs_s, rhs_s};
        auto it = state_map.find(p);
        if (it != state_map.end()) {
            return it->second;
        }

        // Create new state in the result
        State new_state = result.add_state();
        state_map[p] = new_state;
        worklist.push_back(p);

        // Initial state if both components are initial
        if (lhs.initial.contains(lhs_s) && rhs.initial.contains(rhs_s)) {
            result.initial.insert(new_state);
        }

        // Final state if both components are final
        if (lhs.final.contains(lhs_s) && rhs.final.contains(rhs_s)) {
            result.final.insert(new_state);
        }

        return new_state;
    };

    // Merge shared counters from both automata (by name)
    std::unordered_map<std::string, size_t> counter_id_map;
    for (size_t i = 0; i < lhs.counter_set.size(); ++i) {
        const auto& c = lhs.counter_set.get(i);
        size_t new_id = result.counter_set.insert(c.name, c.value);
        counter_id_map[c.name] = new_id;
    }
    for (size_t i = 0; i < rhs.counter_set.size(); ++i) {
        const auto& c = rhs.counter_set.get(i);
        if (counter_id_map.count(c.name) == 0) {
            size_t new_id = result.counter_set.insert(c.name, c.value);
            counter_id_map[c.name] = new_id;
        }
    }

    // Helper to remap annotation from lhs or rhs to result using shared counter names
    auto remap_annotation = [&](const std::shared_ptr<TransitionAnnotation>& ann, const CounterSet& from_set) -> std::shared_ptr<TransitionAnnotation> {
        std::string name = from_set.get_name(ann->get_register_id());
        size_t new_id = counter_id_map.at(name);
        CounterValue val = ann->get_value();

        // Create a new annotation of the same type with remapped counter ID
        if (ann->get_type() == "CounterAssign") return std::make_shared<CounterAssign>(new_id, val); // c++ enum class udelat TODO
        if (ann->get_type() == "CounterIncrement") return std::make_shared<CounterIncrement>(new_id, val);
        if (ann->get_type() == "CounterEqual") return std::make_shared<CounterEqual>(new_id, val);
        if (ann->get_type() == "CounterNotEqual") return std::make_shared<CounterNotEqual>(new_id, val);
        if (ann->get_type() == "CounterGreater") return std::make_shared<CounterGreater>(new_id, val);
        if (ann->get_type() == "CounterLess") return std::make_shared<CounterLess>(new_id, val);
        if (ann->get_type() == "CounterGreaterEqual") return std::make_shared<CounterGreaterEqual>(new_id, val);
        if (ann->get_type() == "CounterLessEqual") return std::make_shared<CounterLessEqual>(new_id, val);

        throw std::runtime_error("Unknown annotation type: " + ann->get_type());
    };

    // Initialize product state space from all pairs of initial states
    for (auto s1 : lhs.initial) {
        for (auto s2 : rhs.initial) {
            // Fills state_map and worklist
            get_or_create_state(s1, s2);
        }
    }

    // Process all reachable state pairs
    while (!worklist.empty()) {
        auto [s1, s2] = worklist.front();
        worklist.pop_front();
        State current = state_map[{s1, s2}];

        auto it1 = lhs.delta[s1].begin();
        auto it2 = rhs.delta[s2].begin();

        // Merge transitions by synchronizing on the same symbol
        while (it1 != lhs.delta[s1].end() && it2 != rhs.delta[s2].end()) { // predelat toto na synchronized iterator ? TODO
            if (it1->symbol < it2->symbol) {
                ++it1;
            } else if (it1->symbol > it2->symbol) {
                ++it2;
            } else {
                Symbol symbol = it1->symbol;
                // Combined transitions under symbol
                SymbolPost product_post{symbol};

                // For each pair of targets, create a new transition
                for (auto tgt1 : it1->targets) {
                    for (auto tgt2 : it2->targets) {
                        State tgt = get_or_create_state(tgt1.state, tgt2.state);

                        // Merge annotations from both automata
                        std::vector<std::shared_ptr<TransitionAnnotation>> all_anns;
                        if (tgt1.annotations_id != UNDEFINED_ANNOTATIONS) { // std::optional ? TODO
                            for (const auto& ann : lhs.annotation_collection[tgt1.annotations_id]) {
                                all_anns.push_back(remap_annotation(ann, lhs.counter_set));
                            }
                        }
                        if (tgt2.annotations_id != UNDEFINED_ANNOTATIONS) {
                            for (const auto& ann : rhs.annotation_collection[tgt2.annotations_id]) {
                                all_anns.push_back(remap_annotation(ann, rhs.counter_set));
                            }
                        }

                        // Insert the combined annotations into result
                        size_t ann_set = UNDEFINED_ANNOTATIONS;
                        if (!all_anns.empty()) {
                            ann_set = result.annotation_collection.size();
                            result.annotation_collection.allocate(ann_set + 1);
                            for (auto& ann : all_anns) {
                                result.annotation_collection.insert(ann, ann_set);
                            }
                        }

                        // Add the final target state with annotations
                        product_post.insert(AnnotationState(tgt, ann_set));
                    }
                }

                // Add the transition to the product automaton
                result.delta.mutable_state_post(current).push_back(std::move(product_post));

                // Move to the next transition
                ++it1;
                ++it2;
            }
        }
    }

    return result;
}

//TODO: move this method to cntnfa.hh? It is something one might want to use (e.g. for union, inclusion, equivalence of DFAs).
Cntnfa mata::cntnfa::algorithms::product(
        const Cntnfa& lhs, const Cntnfa& rhs, const std::function<bool(State,State)>&& final_condition,
        const Symbol first_epsilon, ProductMap *product_map) {

    Cntnfa product{}; // The product automaton.

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

} // namespace mata::cntnfa.
