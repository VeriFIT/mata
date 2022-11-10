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
using namespace Mata::util;

/// naive language inclusion check (complementation + intersection + emptiness)
bool Mata::Nfa::Algorithms::is_incl_naive(
	const Nfa&             smaller,
	const Nfa&             bigger,
	const Alphabet* const  alphabet,
	Run*                   cex,
	const StringMap&  /* params*/)
{ // {{{
    Nfa bigger_cmpl;
    if (alphabet == nullptr) {
        bigger_cmpl = complement(bigger, OnTheFlyAlphabet::from_nfas(smaller, bigger));
    } else {
        bigger_cmpl = complement(bigger, *alphabet);
    }
	Nfa nfa_isect = intersection(smaller, bigger_cmpl, false, nullptr);

	return is_lang_empty(nfa_isect, cex);
} // is_incl_naive }}}


/// language inclusion check using Antichains
bool Mata::Nfa::Algorithms::is_incl_antichains(
	const Nfa&             smaller,
	const Nfa&             bigger,
	const Alphabet* const  alphabet,
	Run*                   cex,
	const StringMap&      params)
{ // {{{
	(void)params;
	(void)alphabet;

	using ProdStateType = std::pair<State, StateSet>;
	using WorklistType = std::list<ProdStateType>;
	using ProcessedType = std::list<ProdStateType>;

	auto subsumes = [](const ProdStateType& lhs, const ProdStateType& rhs) {
		if (lhs.first != rhs.first) {
			return false;
		}

		const StateSet& lhs_bigger = lhs.second;
		const StateSet& rhs_bigger = rhs.second;
		if (lhs_bigger.size() > rhs_bigger.size()) { // bigger set cannot be subset
			return false;
		}

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

	// check initial states first
	for (const auto& state : smaller.initial_states) {
		if (smaller.has_final(state) &&
			are_disjoint(bigger.initial_states, bigger.final_states))
		{
			if (nullptr != cex) { cex->word.clear(); }
			return false;
		}

		const ProdStateType st = std::make_pair(state, bigger.initial_states);
		worklist.push_back(st);
		processed.push_back(st);

		paths.insert({ st, {st, 0}});
	}

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

		// process transitions leaving smaller_state
		for (const auto& post_symb : smaller[smaller_state]) {
			const Symbol& symb = post_symb.symbol;

			for (const State& smaller_succ : post_symb.states_to) {
				const StateSet bigger_succ = bigger.post(bigger_set, symb);
				const ProdStateType succ = {smaller_succ, bigger_succ};

				if (smaller.has_final(smaller_succ) &&
					are_disjoint(bigger_succ, bigger.final_states))
				{
					if (nullptr != cex) {
						cex->word.clear();
						cex->word.push_back(symb);
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

				// prune data structures and insert succ inside
				for (std::list<ProdStateType>* ds : {&processed, &worklist}) {
					auto it = ds->begin();
					while (it != ds->end()) {
						if (subsumes(succ, *it)) {
							auto to_remove = it;
							++it;
							ds->erase(to_remove);
						} else {
							++it;
						}
					}

					// TODO: set pushing strategy
					ds->push_back(succ);
				}

				// also set that succ was accessed from state
				paths[succ] = {prod_state, symb};
			}
		}
	}

	return true;
} // }}}

namespace {
    using AlgoType = decltype(Algorithms::is_incl_naive)*;

    bool compute_equivalence(const Nfa &lhs, const Nfa &rhs, const Alphabet *const alphabet, const StringMap &params,
                             const AlgoType &algo) {
        if (algo(lhs, rhs, alphabet, nullptr, params)) {
            if (algo(rhs, lhs, alphabet, nullptr, params)) {
                return true;
            }
        }

        return false;
    }

    AlgoType set_algorithm(const std::string &function_name, const StringMap &params) {
        if (!haskey(params, "algo")) {
            throw std::runtime_error(function_name +
                                     " requires setting the \"algo\" key in the \"params\" argument; "
                                     "received: " + std::to_string(params));
        }

        decltype(Algorithms::is_incl_naive) *algo;
        const std::string &str_algo = params.at("algo");
        if ("naive" == str_algo) {
            algo = Algorithms::is_incl_naive;
        } else if ("antichains" == str_algo) {
            algo = Algorithms::is_incl_antichains;
        } else {
            throw std::runtime_error(std::to_string(__func__) +
                                     " received an unknown value of the \"algo\" key: " + str_algo);
        }

        return algo;
    }

}

// The dispatching method that calls the correct one based on parameters
bool Mata::Nfa::is_incl(
	const Nfa&             smaller,
	const Nfa&             bigger,
    Run*                   cex,
	const Alphabet* const  alphabet,
	const StringMap&      params)
{ // {{{
    AlgoType algo{ set_algorithm(std::to_string(__func__), params) };
	return algo(smaller, bigger, alphabet, cex, params);
} // is_incl }}}

bool Mata::Nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const Alphabet *alphabet, const StringMap& params)
{
    AlgoType algo{ set_algorithm(std::to_string(__func__), params) };

    if (params.at("algo") == "naive") {
        if (alphabet == nullptr) {
            const auto computed_alphabet{ OnTheFlyAlphabet::from_nfas(lhs, rhs) };
            return compute_equivalence(lhs, rhs, &computed_alphabet, params, algo);
        }
    }

    return compute_equivalence(lhs, rhs, alphabet, params, algo);
}

bool Mata::Nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const StringMap& params) {
    return are_equivalent(lhs, rhs, nullptr, params);
}
