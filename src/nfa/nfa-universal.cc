/* nfa-universal.cc -- NFA universality
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

//TODO: this could be merged with inclusion, or even removed, universality could be implemented using inclusion,
// it is not something needed in practice, so some little overhead is ok


/// naive universality check (complementation + emptiness)
bool Mata::Nfa::Algorithms::is_universal_naive(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	Run*               cex,
	const StringMap&  /* params*/)
{ // {{{
	Nfa cmpl = complement(aut, alphabet);

	return is_lang_empty(cmpl, cex);
} // is_universal_naive }}}


/// universality check using Antichains
bool Mata::Nfa::Algorithms::is_universal_antichains(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	Run*               cex,
	const StringMap&  params)
{ // {{{
	(void)params;

	using WorklistType = std::list<StateSet>;
	using ProcessedType = std::list<StateSet>;

	auto subsumes = [](const StateSet& lhs, const StateSet& rhs) {
		if (lhs.size() > rhs.size()) { // bigger set cannot be subset
			return false;
		}

		return std::includes(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
	};

	// process parameters
	// TODO: set correctly!!!!
	bool is_dfs = true;

	// check the initial state
	if (are_disjoint(aut.initial, aut.final)) {
		if (nullptr != cex) { cex->word.clear(); }
		return false;
	}

	// initialize
	WorklistType worklist = { StateSet(aut.initial) };
	ProcessedType processed = { StateSet(aut.initial) };
	Mata::Util::OrdVector<Symbol> alph_symbols = alphabet.get_alphabet_symbols();

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<StateSet, std::pair<StateSet, Symbol>> paths =
		{ {StateSet(aut.initial), {StateSet(aut.initial), 0}} };

	while (!worklist.empty()) {
		// get a next state
		StateSet state;
		if (is_dfs) {
			state = *worklist.rbegin();
			worklist.pop_back();
		} else { // BFS
			state = *worklist.begin();
			worklist.pop_front();
		}

		// process it
		for (Symbol symb : alph_symbols) {
			StateSet succ = aut.post(state, symb);
			if (are_disjoint(succ, aut.final)) {
				if (nullptr != cex) {
					cex->word.clear();
					cex->word.push_back(symb);
					StateSet trav = state;
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
			for (const auto& anti_state : processed) {
				// trying to find a smaller state in processed
				if (subsumes(anti_state, succ)) {
					is_subsumed = true;
					break;
				}
			}

			if (is_subsumed) { continue; }

			// prune data structures and insert succ inside
			for (std::list<StateSet>* ds : {&processed, &worklist}) {
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
			paths[succ] = {state, symb};
		}
	}

	return true;
} // }}}

// The dispatching method that calls the correct one based on parameters
bool Mata::Nfa::is_universal(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	Run*               cex,
	const StringMap&  params)
{ // {{{

	// setting the default algorithm
	decltype(Algorithms::is_universal_naive)* algo = Algorithms::is_universal_naive;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(std::to_string(__func__) +
			" requires setting the \"algo\" key in the \"params\" argument; "
			"received: " + std::to_string(params));
	}

	const std::string& str_algo = params.at("algorithm");
	if ("naive" == str_algo) { /* default */ }
	else if ("antichains" == str_algo) {
		algo = Algorithms::is_universal_antichains;
	} else {
		throw std::runtime_error(std::to_string(__func__) +
			" received an unknown value of the \"algo\" key: " + str_algo);
	}

	return algo(aut, alphabet, cex, params);
} // is_universal }}}
