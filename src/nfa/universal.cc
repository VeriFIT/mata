/* nfa-universal.cc -- NFA universality
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata::nfa;
using namespace mata::utils;

// TODO: this could be merged with inclusion, or even removed, universality could be implemented using inclusion,
//  it is not something needed in practice, so some little overhead is ok

bool mata::nfa::algorithms::is_universal_naive(const Nfa& aut, const Alphabet& alphabet, Run* cex) {
	return complement(aut, alphabet).is_lang_empty(cex);
}

bool mata::nfa::algorithms::is_universal_antichains(const Nfa& aut, const Alphabet& alphabet,
													Run* cex) { // {{{

	auto subsumes = [](const StateSet& lhs, const StateSet& rhs) {
		if (lhs.size() > rhs.size()) { // bigger set cannot be subset
			return false;
		}

		return std::ranges::includes(rhs, lhs);
	};

	// process parameters
	// TODO: set correctly!!!!
	constexpr bool is_dfs = true;

	// check the initial state
	if (are_disjoint(aut.initial, aut.final)) {
		if (nullptr != cex) { cex->word.clear(); }
		return false;
	}

	// initialize
	std::vector<StateSet> worklist = {StateSet(aut.initial)};
	std::vector<StateSet> processed = {StateSet(aut.initial)};
	const mata::utils::OrdVector<Symbol> alph_symbols = alphabet.get_alphabet_symbols();

	// 'paths[s] == t' denotes that state 's' was accessed from state 't' via some symbol,
	// 'paths[s] == {s, 0}' means that 's' is an initial state.
	// Only populated if a counterexample is requested (@p cex != nullptr).
	std::map<StateSet, std::pair<StateSet, Symbol>> paths;
	if (nullptr != cex) {
		paths[StateSet(aut.initial)] = {StateSet(aut.initial), 0};
	}

	using SyncIterator = mata::nfa::SynchronizedExistentialSymbolPostIterator;

	while (!worklist.empty()) {
		// get a next state, moving it out to avoid a copy
		StateSet state;
		if (is_dfs) {
			state = std::move(worklist.back());
			worklist.pop_back();
		} else { // BFS
			state = std::move(worklist.front());
			worklist.erase(worklist.begin());
		}

		// enumerate successors: collect posts of all states in the macrostate into one synchronized iterator,
		// walk it together with the sorted alphabet. foreign (out-of-alphabet) symbols are skipped, missing
		// alphabet symbols are detected as gaps and used as a counterexample immediately.
		SyncIterator sync_it{};
		for (const State orig_state : state) {
			mata::utils::push_back(sync_it, aut.delta[orig_state]);
		}

		auto alph_it = alph_symbols.begin();
		const auto alph_end = alph_symbols.end();
		bool sync_it_advanced = sync_it.advance();

		while (sync_it_advanced || alph_it != alph_end) {
			if (!sync_it_advanced) {
				// The iterator is exhausted but there are remaining alphabet symbols.
				// This means a symbol is missing: immediate counterexample.
				MATA_ASSERT(alph_it != alph_end);
				if (nullptr != cex) {
					cex->word.clear();
					cex->word.push_back(*alph_it);
					StateSet trav = state;
					while (paths[trav].first != trav) {
						cex->word.push_back(paths[trav].second);
						trav = paths[trav].first;
					}
					std::ranges::reverse(cex->word);
				}
				return false;
			}

			MATA_ASSERT(sync_it_advanced);
			const Symbol sync_symbol = sync_it.get_current_minimum()->symbol;

			// Skip foreign symbols (those not in the alphabet), or check for gaps
			if (alph_it == alph_end || sync_symbol < *alph_it) {
				// Foreign symbol: skip the iterator, stay at current alphabet position.
				sync_it_advanced = sync_it.advance();
				continue;
			}

			MATA_ASSERT(sync_symbol >= *alph_it);
			if (sync_symbol > *alph_it) {
				// Missing alphabet symbol: immediate counterexample.
				if (nullptr != cex) {
					cex->word.clear();
					cex->word.push_back(*alph_it);
					StateSet trav = state;
					while (paths[trav].first != trav) {
						cex->word.push_back(paths[trav].second);
						trav = paths[trav].first;
					}
					std::ranges::reverse(cex->word);
				}
				return false;
			}

			// sync_symbol == *alph_it: compute the successor macrostate
			StateSet succ = sync_it.unify_targets();
			if (!aut.final.intersects_with(succ)) {
				// The successor macrostate does not contain any final state: counterexample found.
				if (nullptr != cex) {
					cex->word.clear();
					cex->word.push_back(sync_symbol);
					StateSet trav = state;
					while (paths[trav].first != trav) {
						cex->word.push_back(paths[trav].second);
						trav = paths[trav].first;
					}
					std::ranges::reverse(cex->word);
				}
				return false;
			}

			// Check if the successor is subsumed by any processed macrostate
			bool is_subsumed = false;
			for (const auto& anti_state : processed) {
				if (subsumes(anti_state, succ)) {
					is_subsumed = true;
					break;
				}
			}

			if (!is_subsumed) {
				// prune data structures and insert succ inside
				for (std::vector<StateSet>* ds : {&processed, &worklist}) {
					auto it = ds->begin();
					while (it != ds->end()) {
						if (subsumes(succ, *it)) {
							it = ds->erase(it);
						} else {
							++it;
						}
					}
					ds->push_back(succ);
				}

				// Record the path to the successor if a counterexample is needed
				if (nullptr != cex) {
					paths[succ] = {state, sync_symbol};
				}
			}

			// Advance both iterators
			++alph_it;
			sync_it_advanced = sync_it.advance();
		}
	}

	return true;
} // }}}

// The dispatching method that calls the correct one based on parameters.
bool mata::nfa::Nfa::is_universal(const Alphabet& alphabet, Run* cex, const ParameterMap& params) const {
	// setting the default algorithm
	decltype(algorithms::is_universal_naive)* algo = algorithms::is_universal_naive;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	if (const std::string& str_algo = params.at("algorithm"); "naive" == str_algo) { /* default */
	} else if ("antichains" == str_algo) {
		algo = algorithms::is_universal_antichains;
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}
	return algo(*this, alphabet, cex);
} // is_universal()

bool mata::nfa::Nfa::is_universal(const Alphabet& alphabet, const ParameterMap& params) const {
	return this->is_universal(alphabet, nullptr, params);
}
