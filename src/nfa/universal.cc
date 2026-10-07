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

	/// A macrostate of the subset construction together with its signature, which is computed once, when the
	///  macrostate is built, and then filters most of the subsumption tests in O(1).
	struct Macrostate {
		StateSet set{};
		uint64_t signature{0};
	};

	using WorklistType = std::list<Macrostate>;
	using ProcessedType = std::list<Macrostate>;

	auto subsumes = [](const Macrostate& lhs, const Macrostate& rhs) {
		if (lhs.set.size() > rhs.set.size()) { // bigger set cannot be subset
			return false;
		}
		// A state of `lhs` outside of `rhs` shows up as a signature bit of `lhs` that `rhs` does not have.
		if ((lhs.signature & ~rhs.signature) != 0) { return false; }

		return std::ranges::includes(rhs.set, lhs.set);
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
	const StateSet initial_set{aut.initial};
	const Macrostate initial{initial_set, macrostate_signature(initial_set)};
	WorklistType worklist = {initial};
	ProcessedType processed = {initial};
	const mata::utils::OrdVector<Symbol> alph_symbols = alphabet.get_alphabet_symbols();

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<StateSet, std::pair<StateSet, Symbol>> paths = {{initial_set, {initial_set, 0}}};

	while (!worklist.empty()) {
		// get a next state
		Macrostate state;
		if (is_dfs) {
			state = *worklist.rbegin();
			worklist.pop_back();
		} else { // BFS
			state = *worklist.begin();
			worklist.pop_front();
		}

		// process it
		for (Symbol symb : alph_symbols) {
			StateSet succ_set = aut.post(state.set, symb);
			if (!aut.final.intersects_with(succ_set)) {
				if (nullptr != cex) {
					cex->word.clear();
					cex->word.push_back(symb);
					StateSet trav = state.set;
					while (paths[trav].first != trav) { // go back until initial state
						cex->word.push_back(paths[trav].second);
						trav = paths[trav].first;
					}

					std::ranges::reverse(cex->word);
				}

				return false;
			}

			const uint64_t succ_signature{macrostate_signature(succ_set)};
			const Macrostate succ{std::move(succ_set), succ_signature};
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
			for (std::list<Macrostate>* ds : {&processed, &worklist}) {
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
			paths[succ.set] = {state.set, symb};
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
