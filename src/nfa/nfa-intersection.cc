/* nfa-intersection.cc -- Intersection of NFAs
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// MATA headers
#include <mata/nfa.hh>

using namespace Mata::Nfa;

namespace {
    void union_to_left(StateSet &receivingSet, const StateSet &addedSet) {
        receivingSet.insert(addedSet);
    }
}

namespace Mata
{
namespace Nfa
{

/**
 * Class handling intersection of automata.
 *
 * Implements a normal intersection and an intersection preserving epsilon transitions.
 */
class Intersection
{
public:
    /**
     * Compute classic intersection of NFAs @p lhs and @p rhs.
     * @param lhs First NFA to compute intersection for.
     * @param rhs Second NFA to compute intersection for.
     */
    Intersection(const Nfa& lhs, const Nfa& rhs) : lhs(lhs), rhs(rhs)
    {
        compute();
    }

    /**
     * Compute epsilon transitions preserving intersection of NFAs @p lhs and @p rhs.
     * @param lhs First NFA to compute intersection for.
     * @param rhs Second NFA to compute intersection for.
     * @param epsilon Symbol to handle as an epsilon symbol.
     */
    Intersection(const Nfa& lhs, const Nfa& rhs, const Symbol epsilon) : lhs(lhs), rhs(rhs), epsilon(epsilon)
    {
        compute_preserving_epsilon_transitions();
    }

    /**
     * Compute classic intersection of NFAs @p lhs and @p rhs.
     * @param lhs First NFA to compute intersection for.
     * @param rhs Second NFA to compute intersection for.
     */
    static Intersection compute(const Nfa& lhs, const Nfa& rhs)
    {
        return Intersection{ lhs, rhs };
    }

    /**
     * Compute epsilon transitions preserving intersection of NFAs @p lhs and @p rhs.
     * @param lhs First NFA to compute intersection for.
     * @param rhs Second NFA to compute intersection for.
     * @param epsilon Symbol to handle as an epsilon symbol.
     */
    static Intersection compute(const Nfa& lhs, const Nfa& rhs, const Symbol epsilon)
    {
        return Intersection{ lhs, rhs, epsilon };
    }

    /**
     * Get the final product NFA of the intersection.
     * @return Product NFA of the intersection.
     */
    const Nfa& get_product() { return product; }

    /**
     * Get product map for the generated product NFA.
     * @return Product map mapping original state pairs to new product states.
     */
    const ProductMap& get_product_map() { return product_map; }

private:
    Nfa product{}; ///< Product of the intersection.
    /// Product map for the generated intersection mapping original state pairs to new product states.
    ProductMap product_map{};
    const Nfa& lhs; ///< First NFA to compute intersection for.
    const Nfa& rhs; ///< Second NFA to compute intersection for.
    const Symbol epsilon{}; ///< Symbol to handle as an epsilon symbol.

    StatePair pair_to_process{}; ///< State pair of original states currently being processed.
    std::unordered_set<StatePair> pairs_to_process{}; ///< Set of state pairs of original states to process.

    /**
     * Compute classic intersection.
     */
    void compute()
    {
        // TODO probably remove this since we use prod_map as parameter
        //std::unordered_map<StatePair, State, decltype(hashStatePair)> thisAndOtherStateToIntersectState(10, hashStatePair); // TODO default buckets?

        initialize_pairs_to_process();

        while (!pairs_to_process.empty())
        {
            pair_to_process = *pairs_to_process.cbegin();
            pairs_to_process.erase(pair_to_process);

            // TODO rewrite this (TODO from previous Vata implementation -- rewrite the algorithm, or the format?).

            //compute_for_state_pair();
            compute_for_state_pair_using_sui();
        }
    }

    /**
     * Compute intersection preserving epsilon transitions.
     */
    void compute_preserving_epsilon_transitions() {
        initialize_pairs_to_process();

        while (!pairs_to_process.empty()) {
            pair_to_process = *pairs_to_process.cbegin();
            pairs_to_process.erase(pair_to_process);
            compute_transitions_for_state_pair_eps_pres();
        }
    }

    /**
     * Add transitions of the current state pair for an epsilon preserving product.
     */
    void compute_transitions_for_state_pair_eps_pres() {
        for (const auto& lhs_state_transitions: lhs.transitionrelation[pair_to_process.first]) {
            // Check for lhs epsilon transitions.
            if (lhs_state_transitions.symbol == epsilon) {
                compute_for_lhs_state_epsilon_transitions(lhs_state_transitions);
            }

            for (const auto& rhs_state_transitions: rhs.transitionrelation[pair_to_process.second]) {
                // Find all transitions that have the same symbol for first and the second state in the pair_to_process.
                if (lhs_state_transitions.symbol == rhs_state_transitions.symbol) {
                    compute_for_same_symbols(lhs_state_transitions, rhs_state_transitions);
                }
            }
        }

        // Check for rhs epsilon transitions.
        add_rhs_epsilon_transitions();
    }

    /**
     * Check for epsilon transitions in case only rhs has any transitions and add them.
     */
    void add_rhs_epsilon_transitions() {
        for (const auto& rhs_state_transitions: rhs.transitionrelation[pair_to_process.second]) {
            if (rhs_state_transitions.symbol == epsilon) {
                compute_for_rhs_state_epsilon_transitions(rhs_state_transitions);
            }
        }
    }

    /**
     * Initialize pairs to process with initial state pairs.
     */
    void initialize_pairs_to_process() {
        for (const State this_initial_state : lhs.initialstates) {
            for (const State other_initial_state : rhs.initialstates) {
                handle_initial_state_pairs(this_initial_state, other_initial_state);
            }
        }
    }

    /**
     * Add transition to the product.
     * @param[in] intersection_transition State transitions to add to the product.
     */
    void add_product_transition(const Move& intersection_transition)
    {
        if (intersection_transition.states_to.empty()) { return; }

        auto& intersect_state_transitions{ product.transitionrelation[product_map[pair_to_process]] };
        auto symbol_transitions_iter{ intersect_state_transitions.find(intersection_transition) };
        if (symbol_transitions_iter == intersect_state_transitions.end()) {
            intersect_state_transitions.push_back(intersection_transition);
        }
        else {
            // Product already has some target states for the given symbol from the current product state.
            union_to_left(symbol_transitions_iter->states_to, intersection_transition.states_to);
        }
    }

    /**
     * Update product with initial state pairs.
     * @param[in] lhs_initial_state Initial state of NFA @c lhs.
     * @param[in] rhs_initial_state Initial state of NFA @c rhs.
     */
    void handle_initial_state_pairs(const State lhs_initial_state, const State rhs_initial_state)
    {
        const StatePair this_and_other_initial_state_pair(lhs_initial_state, rhs_initial_state);
        const State new_intersection_state = product.add_new_state();

        product_map[this_and_other_initial_state_pair] = new_intersection_state;
        pairs_to_process.insert(this_and_other_initial_state_pair);

        product.initialstates.push_back(new_intersection_state);
        if (lhs.has_final(lhs_initial_state) && rhs.has_final(rhs_initial_state))
        {
            product.finalstates.push_back(new_intersection_state);
        }
    }

    /**
     * Compute product for state transitions with same symbols.
     * @param[in] lhs_state_transitions State transitions of NFA @c lhs to compute product for.
     * @param[in] rhs_state_transitions State transitions of NFA @c rhs to compute product for.
     */
    void compute_for_same_symbols(const Move& lhs_state_transitions,
                                  const Move& rhs_state_transitions)
    {
        // Create transition from the pair_to_process to all pairs between states to which first transition goes and states
        // to which second one goes.
        Move intersection_transition{lhs_state_transitions.symbol };
        for (const State this_state_to: lhs_state_transitions.states_to)
        {
            for (const State other_state_to: rhs_state_transitions.states_to)
            {
                create_product_state_and_trans(this_state_to, other_state_to, intersection_transition);
            }
        }
        add_product_transition(intersection_transition);
    }

    /**
     * Compute product for state transitions with @c lhs state epsilon transition.
     * @param[in] lhs_state_transitions State transitions of NFA @c lhs to compute product for.
     */
    void compute_for_lhs_state_epsilon_transitions(const Move& lhs_state_transitions)
    {
        // Create transition from the pair_to_process to all pairs between states to which first transition goes and states to which second one goes.
        Move intersection_transition{lhs_state_transitions.symbol };
        for (const State this_state_to: lhs_state_transitions.states_to)
        {
            create_product_state_and_trans(this_state_to, pair_to_process.second, intersection_transition);
        }
        add_product_transition(intersection_transition);
    }

    /**
     * Compute product for state transitions with @c rhs state epsilon transition.
     * @param[in] rhs_state_transitions State transitions of NFA @c rhs to compute product for.
     */
    void compute_for_rhs_state_epsilon_transitions(const Move& rhs_state_transitions)
    {
        // create transition from the pair_to_process to all pairs between states to which first transition goes and states to which second one goes
        Move intersection_transition{rhs_state_transitions.symbol };
        for (const State other_state_to: rhs_state_transitions.states_to)
        {
            create_product_state_and_trans(pair_to_process.first, other_state_to, intersection_transition);
        }
        add_product_transition(intersection_transition);
    }

    /**
     * Compute classic product for current state pair.
     */
    void compute_for_state_pair()
    {
        auto this_state_transitions_iter = lhs.transitionrelation[pair_to_process.first].begin();
        auto other_state_transitions_iter = rhs.transitionrelation[pair_to_process.second].begin();
        const auto this_state_transitions_iter_end = lhs.transitionrelation[pair_to_process.first].end();
        const auto other_state_transitions_iter_end = rhs.transitionrelation[pair_to_process.second].end();
        // find all transitions that have same symbol for first and the second state in the pair_to_process
        while (this_state_transitions_iter != this_state_transitions_iter_end
               && other_state_transitions_iter != other_state_transitions_iter_end)
        {
            // first iterator points to transition with smaller symbol, move it until it is either same or further than second iterator
            if (this_state_transitions_iter->symbol < other_state_transitions_iter->symbol)
            {
                while (this_state_transitions_iter != this_state_transitions_iter_end
                       && this_state_transitions_iter->symbol < other_state_transitions_iter->symbol)
                {
                    ++this_state_transitions_iter;
                }
                if (this_state_transitions_iter == this_state_transitions_iter_end)
                {
                    break;
                }
            }
            else
            {
                // second iterator points to transition with smaller symbol, move it until it is either same or further than first iterator
                while (other_state_transitions_iter != other_state_transitions_iter_end
                       && this_state_transitions_iter->symbol > other_state_transitions_iter->symbol)
                {
                    ++other_state_transitions_iter;
                }
                if (other_state_transitions_iter == other_state_transitions_iter_end) { break; }
            }

            // check both iterators point to the transitions with same symbol
            if (this_state_transitions_iter->symbol == other_state_transitions_iter->symbol)
            {
                compute_for_same_symbols(*this_state_transitions_iter,
                                         *other_state_transitions_iter);

                ++this_state_transitions_iter;
                ++other_state_transitions_iter;
            }
        }
    }

    // Alternative for the above that uses synchronized_univerzal_iterator
    void compute_for_state_pair_using_sui()
    {
        Mata::Util::synchronized_univerzal_iterator<Move> sui(2);
        sui.push_back(lhs.transitionrelation[pair_to_process.first]);
        sui.push_back(rhs.transitionrelation[pair_to_process.second]);

        while (sui.advance()){
            std::vector<Mata::Util::OrdVector<Move>::const_iterator> moves = sui.get_current();
            assert(moves.size() == 2);
            compute_for_same_symbols(*moves[0],*moves[1]);
        }
    }

    /**
     * Create product state and its transitions.
     * @param[in] lhs_state_to Target state in NFA @c lhs.
     * @param[in] rhs_state_to Target state in NFA @c rhs.
     * @param[out] intersect_transitions Transitions of the product state.
     */
    void create_product_state_and_trans(const State lhs_state_to, const State rhs_state_to, Move& intersect_transitions)
    {
        const StatePair intersect_state_pair_to(lhs_state_to, rhs_state_to);
        State intersect_state_to;
        if (product_map.find(intersect_state_pair_to) == product_map.end())
        {
            intersect_state_to = product.add_new_state();
            product_map[intersect_state_pair_to] = intersect_state_to;
            pairs_to_process.insert(intersect_state_pair_to);

            if (lhs.has_final(lhs_state_to) && rhs.has_final(rhs_state_to))
            {
                product.make_final(intersect_state_to);
            }
        }
        else
        {
            intersect_state_to = product_map[intersect_state_pair_to];
        }
        intersect_transitions.states_to.insert(intersect_state_to);
    }
}; // Intersection

void intersection(Nfa *res, const Nfa &lhs, const Nfa &rhs, ProductMap*  prod_map)
{
    Intersection intersection { Intersection::compute(lhs, rhs) };
    if (prod_map != nullptr)
    {
        *prod_map = intersection.get_product_map();
    }
    *res = intersection.get_product();
}

void intersection_preserving_epsilon_transitions(Nfa* res, const Nfa &lhs, const Nfa &rhs, Symbol epsilon, ProductMap* prod_map)
{
    Intersection intersection { Intersection::compute(lhs, rhs, epsilon) };
    if (prod_map != nullptr)
    {
        *prod_map = intersection.get_product_map();
    }
    *res = intersection.get_product();
}

Nfa intersection_preserving_epsilon_transitions(const Nfa& lhs, const Nfa& rhs, Symbol epsilon, ProductMap*  prod_map)
{
    Intersection intersection { Intersection::compute(lhs, rhs, epsilon) };
    if (prod_map != nullptr)
    {
        *prod_map = intersection.get_product_map();
    }
    return intersection.get_product();
}

Nfa intersection(const Nfa& lhs, const Nfa& rhs, ProductMap* prod_map)
{
    Intersection intersection { Intersection::compute(lhs, rhs) };
    if (prod_map != nullptr)
    {
        *prod_map = intersection.get_product_map();
    }

    return intersection.get_product();
}

} // Nfa
} // Mata
