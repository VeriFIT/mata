/* nfa-incl.cc -- NFA language inclusion
 */

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata::nfa;
using namespace mata::utils;

/// naive language inclusion check (complementation + intersection + emptiness)
bool mata::nfa::algorithms::is_included_naive(
        const Nfa &smaller,
        const Nfa &bigger,
        const Alphabet *const alphabet,//TODO: this should not be needed, likewise for equivalence
        Run *cex) { // {{{
    Nfa bigger_cmpl;
    if (alphabet == nullptr) {
        bigger_cmpl = complement(bigger, create_alphabet(smaller, bigger));
    } else {
        bigger_cmpl = complement(bigger, *alphabet);
    }
    Nfa nfa_isect = intersection(smaller, bigger_cmpl);

    return nfa_isect.is_lang_empty(cex);
} // is_included_naive }}}


/// language inclusion check using Antichains
// TODO, what about to construct the separator from this?
bool mata::nfa::algorithms::is_included_antichains(
    const Nfa&             smaller,
    const Nfa&             bigger,
    const Alphabet* const  alphabet, //TODO: this parameter is not used
    Run*                   cex)
{ // {{{
    (void)alphabet;

    // TODO: Decide what is the best optimization for inclusion.

    using ProdStateType = std::tuple<State, StateSet, size_t>;
    using ProdStatesType = std::vector<ProdStateType>;
    // ProcessedType is indexed by states of the smaller nfa
    // tailored for pure antichain approach ... the simulation-based antichain will not work (without changes).
    using ProcessedType = std::vector<ProdStatesType>;

    auto subsumes = [](const ProdStateType& lhs, const ProdStateType& rhs) {
        if (std::get<0>(lhs) != std::get<0>(rhs)) {
            return false;
        }

        const StateSet& lhs_bigger = std::get<1>(lhs);
        const StateSet& rhs_bigger = std::get<1>(rhs);

        //TODO: Can this be done faster using more heuristics? E.g., compare the last elements first ...
        //TODO: Try BDDs! What about some abstractions?
        return lhs_bigger.IsSubsetOf(rhs_bigger);
    };


    // initialize
    ProdStatesType worklist{};//Pairs (q,S) to be processed. It sometimes gives a huge speed-up when they are kept sorted by the size of S,
    // worklist.reserve(32);
    // so those with smaller popped for processing first.
    ProcessedType processed(smaller.num_of_states()); // Allocate to the number of states of the smaller nfa.
    // The pairs of each state are also kept sorted. It allows slightly faster antichain pruning - no need to test inclusion in sets that have less elements.

    //Is |S| < |S'| for the inut pairs (q,S) and (q',S')?
    // auto smaller_set = [](const ProdStateType & a, const ProdStateType & b) { return std::get<1>(a).size() < std::get<1>(b).size(); };

    std::vector<State> distances_smaller = revert(smaller).distances_from_initial();
    std::vector<State> distances_bigger = revert(bigger).distances_from_initial();

    // auto closer_dist = [&](const ProdStateType & a, const ProdStateType & b) {
    //     return distances_smaller[a.first] < distances_smaller[b.first];
    // };

    // auto closer_smaller = [&](const ProdStateType & a, const ProdStateType & b) {
    //     if (distances_smaller[a.first] != distances_smaller[b.first])
    //         return distances_smaller[a.first] < distances_smaller[b.first];
    //     else
    //         return a.second.size() < b.second.size();
    // };

    // auto smaller_closer = [&](const ProdStateType & a, const ProdStateType & b) {
    //     if (a.second.size() != b.second.size())
    //         return a.second.size() < b.second.size();
    //     else
    //         return distances_smaller[a.first] < distances_smaller[b.first];
    // };

    auto min_dst = [&](const StateSet& set) {
        if (set.empty()) return Limits::max_state;
        return distances_bigger[*std::min_element(set.begin(), set.end(), [&](const State a,const State b){return distances_bigger[a] < distances_bigger[b];})];
    };

    auto lengths_incompatible = [&](const ProdStateType& pair) {
        return distances_smaller[std::get<0>(pair)] < std::get<2>(pair);
    };

    auto insert_to_pairs = [&](ProdStatesType & pairs,const ProdStateType & pair) {
        // auto it = std::lower_bound(pairs.begin(), pairs.end(), pair, smaller_set);
        // auto it = std::lower_bound(pairs.begin(), pairs.end(), pair, closer_dist);
        // auto it = std::lower_bound(pairs.begin(), pairs.end(), pair, smaller_closer);
        // auto it = std::lower_bound(pairs.begin(), pairs.end(), pair, closer_smaller);
        // pairs.insert(it,pair);
        pairs.push_back(pair);
        // std::sort(pairs.begin(), pairs.end(), smaller_closer);
    };

    // 'paths[s] == t' denotes that state 's' was accessed from state 't',
    // 'paths[s] == s' means that 's' is an initial state
    std::map<ProdStateType, std::pair<ProdStateType, Symbol>> paths;

    // check initial states first // TODO: this would be done in the main loop as the first thing anyway?
    for (const auto& state : smaller.initial) {
        if (smaller.final[state] &&
            are_disjoint(bigger.initial, bigger.final))
        {
            if (cex != nullptr) { cex->word.clear(); }
            return false;
        }

        StateSet bigger_state_set{ bigger.initial };
        const ProdStateType st = std::tuple(state, bigger_state_set, min_dst(bigger_state_set));
        insert_to_pairs(worklist, st);
        insert_to_pairs(processed[state],st);

        if (cex != nullptr)
            paths.insert({ st, {st, 0}});
    }

    //For synchronised iteration over the set of states
    SynchronizedExistentialSymbolPostIterator sync_iterator;

    // We use DFS strategy for the worklist processing
    while (!worklist.empty()) {
        // get a next product state
        ProdStateType prod_state = *worklist.rbegin();
        worklist.pop_back();

        const State& smaller_state = std::get<0>(prod_state);
        const StateSet& bigger_set = std::get<1>(prod_state);

        sync_iterator.reset();
        for (State q: bigger_set) {
            mata::utils::push_back(sync_iterator, bigger.delta[q]);
        }

        // process transitions leaving smaller_state
        for (const auto& smaller_move : smaller.delta[smaller_state]) {
            const Symbol& smaller_symbol = smaller_move.symbol;

            StateSet bigger_succ = {};
            if(sync_iterator.synchronize_with(smaller_move)) {
                bigger_succ = sync_iterator.unify_targets();
            }

            for (const State& smaller_succ : smaller_move.targets) {
                const ProdStateType succ = {smaller_succ, bigger_succ, min_dst(bigger_succ)};

                if (lengths_incompatible(succ) || (smaller.final[smaller_succ] &&
                    !bigger.final.intersects_with(bigger_succ)))
                {
                    if (cex != nullptr) {
                        cex->word.clear();
                        cex->word.push_back(smaller_symbol);
                        ProdStateType trav = prod_state;
                        while (paths[trav].first != trav)
                        { // go back until initial state
                            cex->word.push_back(paths[trav].second);
                            trav = paths[trav].first;
                        }

                        std::reverse(cex->word.begin(), cex->word.end());
                    }

                    return false;
                }

                bool is_subsumed = false;
                for (const auto& anti_state : processed[smaller_succ])
                { // trying to find in processed a smaller state than the newly created succ
                    // if (smaller_set(succ,anti_state)) {
                    //     break;
                    // }
                    if (subsumes(anti_state, succ)) {
                        is_subsumed = true;
                        break;
                    }
                }

                if (is_subsumed) {
                    continue;
                }

                for (ProdStatesType* ds: {&processed[smaller_succ], &worklist}) {
                    //Pruning of processed and the worklist.
                    //Since they are ordered by the size of the sets, we can iterate from back,
                    //and as soon as we get to sets larger than succ, we can stop (larger sets cannot be subsets).
                    std::erase_if(*ds, [&](const auto& d){ return subsumes(succ, d); });
                    // for (long it = static_cast<long>(ds->size()-1);it>=0;--it) {
                    //     // if (smaller_set((*ds)[static_cast<size_t>(it)],succ))
                    //         // break;
                    //     if (subsumes(succ, (*ds)[static_cast<size_t>(it)])) {
                    //         //Using index it instead of an iterator since erase could invalidate it (?)
                    //         ds->erase(ds->begin() + it);
                    //     }
                    // }
                    insert_to_pairs(*ds, succ);
                }

                if(cex != nullptr) {
                    // also set that succ was accessed from state
                    paths[succ] = {prod_state, smaller_symbol};
                }
            }
        }
    }
    return true;
} // }}}

namespace {
    using AlgoType = decltype(algorithms::is_included_naive)*;

    bool compute_equivalence(const Nfa &lhs, const Nfa &rhs, const mata::Alphabet *const alphabet, const AlgoType &algo) {
        //alphabet should not be needed as input parameter
        if (algo(lhs, rhs, alphabet, nullptr)) {
            if (algo(rhs, lhs, alphabet, nullptr)) {
                return true;
            }
        }

        return false;
    }

    AlgoType set_algorithm(const std::string &function_name, const ParameterMap &params) {
        if (!haskey(params, "algorithm")) {
            throw std::runtime_error(function_name +
                                     " requires setting the \"algo\" key in the \"params\" argument; "
                                     "received: " + std::to_string(params));
        }

        decltype(algorithms::is_included_naive) *algo;
        const std::string &str_algo = params.at("algorithm");
        if ("naive" == str_algo) {
            algo = algorithms::is_included_naive;
        } else if ("antichains" == str_algo) {
            algo = algorithms::is_included_antichains;
        } else {
            throw std::runtime_error(std::to_string(__func__) +
                                     " received an unknown value of the \"algo\" key: " + str_algo);
        }

        return algo;
    }

}

// The dispatching method that calls the correct one based on parameters
bool mata::nfa::is_included(
        const Nfa &smaller,
        const Nfa &bigger,
        Run *cex,
        const Alphabet *const alphabet,
        const ParameterMap &params) { // {{{
    AlgoType algo{set_algorithm(std::to_string(__func__), params)};
    return algo(smaller, bigger, alphabet, cex);
} // is_included }}}

bool mata::nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const Alphabet *alphabet, const ParameterMap& params)
{
    //TODO: add comment on what this is doing, what is __func__ ...
    AlgoType algo{ set_algorithm(std::to_string(__func__), params) };

    if (params.at("algorithm") == "naive") {
        if (alphabet == nullptr) {
            const auto computed_alphabet{create_alphabet(lhs, rhs) };
            return compute_equivalence(lhs, rhs, &computed_alphabet, algo);
        }
    }

    return compute_equivalence(lhs, rhs, alphabet, algo);
}

bool mata::nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const ParameterMap& params) {
    return are_equivalent(lhs, rhs, nullptr, params);
}
