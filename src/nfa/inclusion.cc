/* nfa-incl.cc -- NFA language inclusion
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
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
    Nfa nfa_isect = intersection(smaller, bigger_cmpl, false, nullptr);

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
    //TODO: what does this do?
    (void)alphabet;

    using ProdStateType = std::pair<State, StateSet>;
    using WorklistType = std::deque<ProdStateType>;
    // ProcessedType is indexed by states of the smaller nfa
    // tailored for pure antichain approach ... the simulation-based antichain will not work (without changes).
    using ProcessedType = std::vector<std::deque<ProdStateType>>;

    auto subsumes = [](const ProdStateType& lhs, const ProdStateType& rhs) {
        if (lhs.first != rhs.first) {
            return false;
        }

        const StateSet& lhs_bigger = lhs.second;
        const StateSet& rhs_bigger = rhs.second;
        if (lhs_bigger.size() > rhs_bigger.size()) { // bigger set cannot be subset
            return false;
        }

        //TODO: Can this be done faster using more heuristics? E.g., compare the last elements first ...
        //TODO: Try BDDs! What about some abstractions?
        return lhs_bigger.IsSubsetOf(rhs_bigger);
    };

    // initialize
    WorklistType worklist = { };
    ProcessedType processed(smaller.num_of_states()); // allocate to the number of states of the smaller nfa

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

        const ProdStateType st = std::make_pair(state, StateSet(bigger.initial));
        worklist.push_back(st);
        processed[state].push_back(st);

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

        const State& smaller_state = prod_state.first;
        const StateSet& bigger_set = prod_state.second;

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
                const ProdStateType succ = {smaller_succ, bigger_succ};

                if (smaller.final[smaller_succ] &&
                    !bigger.final.intersects_with(bigger_succ))
                {
                    if (cex  != nullptr) {
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
                { // trying to find a smaller state in processed
                    if (subsumes(anti_state, succ)) {
                        is_subsumed = true;
                        break;
                    }
                }

                if (is_subsumed) { continue; }

                for (std::deque<ProdStateType>* ds : {&processed[smaller_succ], &worklist}) {
                    for (size_t it = 0; it < ds->size(); ++it) {
                        if (subsumes(succ, ds->at(it))) {
                            //Removal though replacement by the last element and removal pob_back.
                            //Because calling erase would invalidate iterator it (in deque).
                            ds->at(it) = ds->back(); //does it coppy stuff?
                            ds->pop_back();
                        } else {
                            ++it;
                        }
                    }

                    // TODO: set pushing strategy
                    ds->push_back(succ);
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
