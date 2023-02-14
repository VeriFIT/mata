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
#include <mata/nfa.hh>
#include <mata/nfa-algorithms.hh>

using namespace Mata::Nfa;
using namespace Mata::Util;

/// naive language inclusion check (complementation + intersection + emptiness)
bool Mata::Nfa::Algorithms::is_included_naive(
        const Nfa &smaller,
        const Nfa &bigger,
        const Alphabet *const alphabet,//TODO: this should not be needed, likewise for equivalence
        Run *cex,
        const StringMap &  /* params*/) { // {{{
    Nfa bigger_cmpl;
    if (alphabet == nullptr) {
        bigger_cmpl = complement(bigger, create_alphabet(smaller, bigger));
    } else {
        bigger_cmpl = complement(bigger, *alphabet);
    }
    Nfa nfa_isect = intersection(smaller, bigger_cmpl, false, nullptr);

    return is_lang_empty(nfa_isect, cex);
} // is_included_naive }}}


/// language inclusion check using Antichains
// TODO, what about to construct the separator from this?
bool Mata::Nfa::Algorithms::is_included_antichains(
    const Nfa&             smaller,
    const Nfa&             bigger,
    const Alphabet* const  alphabet, //TODO: this parameter is not used
    Run*                   cex,
    const StringMap&      params) //TODO: why is this parameter there?
{ // {{{
    //TODO: what does this do?
    (void)params;
    (void)alphabet;

    using ProdStateType = std::pair<State, StateSet>;
    using WorklistType = std::deque<ProdStateType>;
    using ProcessedType = std::deque<ProdStateType>;

    //TODO: This is used in a container (a deque) of pairs, where every new pair means iterating through the entire list,
    // and testing subsumption with everybody.
    // Rewrite this as a deque (vector?) indexed by the first component (state) of vectors of the second components.
    // We will go to the vector of the first component and test subsumption of the second components there.
    // It may need some more fiddling if we still want to implement pure dfs/bfs however.
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
        return std::includes(rhs_bigger.begin(), rhs_bigger.end(),
            lhs_bigger.begin(), lhs_bigger.end());
    };

    // process parameters
    // TODO: set correctly!!!!
    bool is_dfs = true;

    // initialize
    WorklistType worklist = { };
    ProcessedType processed = { };

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
        processed.push_back(st);

        if (cex != nullptr)
            paths.insert({ st, {st, 0}});
    }

    //For synchronised iteration over the set of states
    using Iterator = Mata::Util::OrdVector<Move>::const_iterator;
    Mata::Util::SynchronizedExistentialIterator<Iterator> sync_iterator;

    while (!worklist.empty()) {
        // get a next product state
        ProdStateType prod_state;
        if (is_dfs) {
            prod_state = *worklist.rbegin();
            worklist.pop_back();
        } else { // BFS
            prod_state = *worklist.begin();
            worklist.pop_front();
        }

        const State& smaller_state = prod_state.first;
        const StateSet& bigger_set = prod_state.second;

        sync_iterator.reset();
        for (State q: bigger_set) {
            Mata::Util::push_back(sync_iterator, bigger.delta[q]);
        }

        // process transitions leaving smaller_state
        for (const auto& smaller_move : smaller[smaller_state]) {//TODO: this should become smaller.delta[smaller_state] after refactoring
            const Symbol& smaller_symbol = smaller_move.symbol;

            do {
                if (sync_iterator.is_synchronized()) {
                    auto current_min = sync_iterator.get_current_minimum();
                    if (*current_min >= smaller_move) {
                        break;
                    }
                }
            } while (sync_iterator.advance());

            // TODO: this is ugly, the interface of the sync iterator should be redesigned so that this looks ok
            StateSet bigger_succ = {};
            if(sync_iterator.is_synchronized() && *sync_iterator.get_current_minimum() == smaller_move) {
                std::vector<Iterator> bigger_moves = sync_iterator.get_current();
                for (auto m: bigger_moves) {
                    bigger_succ = bigger_succ.Union(m->targets);
                }
            }

            for (const State& smaller_succ : smaller_move.targets) {
                const ProdStateType succ = {smaller_succ, bigger_succ};

                if (smaller.final[smaller_succ] &&
                    are_disjoint(bigger_succ, bigger.final))
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
                for (const auto& anti_state : processed)
                { // trying to find a smaller state in processed
                    if (subsumes(anti_state, succ)) {
                        is_subsumed = true;
                        break;
                    }
                }

                if (is_subsumed) { continue; }

                for (std::deque<ProdStateType>* ds : {&processed, &worklist}) {
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

                // also set that succ was accessed from state
                paths[succ] = {prod_state, smaller_symbol};
            }
        }
    }

    return true;
} // }}}

namespace {
    using AlgoType = decltype(Algorithms::is_included_naive)*;

    bool compute_equivalence(const Nfa &lhs, const Nfa &rhs, const Mata::Alphabet *const alphabet, const StringMap &params,
                             const AlgoType &algo) {
        //alphabet should not be needed as input parameter
        if (algo(lhs, rhs, alphabet, nullptr, params)) {
            if (algo(rhs, lhs, alphabet, nullptr, params)) {
                return true;
            }
        }

        return false;
    }

    AlgoType set_algorithm(const std::string &function_name, const StringMap &params) {
        if (!haskey(params, "algorithm")) {
            throw std::runtime_error(function_name +
                                     " requires setting the \"algo\" key in the \"params\" argument; "
                                     "received: " + std::to_string(params));
        }

        decltype(Algorithms::is_included_naive) *algo;
        const std::string &str_algo = params.at("algorithm");
        if ("naive" == str_algo) {
            algo = Algorithms::is_included_naive;
        } else if ("antichains" == str_algo) {
            algo = Algorithms::is_included_antichains;
        } else {
            throw std::runtime_error(std::to_string(__func__) +
                                     " received an unknown value of the \"algo\" key: " + str_algo);
        }

        return algo;
    }

}

// The dispatching method that calls the correct one based on parameters
bool Mata::Nfa::is_included(
        const Nfa &smaller,
        const Nfa &bigger,
        Run *cex,
        const Alphabet *const alphabet,
        const StringMap &params) { // {{{
    AlgoType algo{set_algorithm(std::to_string(__func__), params)};
    return algo(smaller, bigger, alphabet, cex, params);
} // is_included }}}

bool Mata::Nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const Alphabet *alphabet, const StringMap& params)
{
    //TODO: add comment on what this is doing, what is __func__ ...
    AlgoType algo{ set_algorithm(std::to_string(__func__), params) };

    if (params.at("algorithm") == "naive") {
        if (alphabet == nullptr) {
            const auto computed_alphabet{create_alphabet(lhs, rhs) };
            return compute_equivalence(lhs, rhs, &computed_alphabet, params, algo);
        }
    }

    return compute_equivalence(lhs, rhs, alphabet, params, algo);
}

bool Mata::Nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const StringMap& params) {
    return are_equivalent(lhs, rhs, nullptr, params);
}
