// TODO: add header

#include <vata2/nfa.hh>

using namespace Vata2::Nfa;
using namespace Vata2::util;

namespace {

/// naive language inclusion check (complementation + intersection + emptiness)
bool is_incl_naive(
	const Nfa&         smaller,
	const Nfa&         bigger,
	const Alphabet&    alphabet,
	Word*              cex,
	const StringDict&  /* params*/)
{ // {{{
	Nfa bigger_cmpl = complement(bigger, alphabet);
	Nfa nfa_isect = intersection(smaller, bigger_cmpl);

	bool result;
	if (nullptr == cex) {
		result = is_lang_empty(nfa_isect);
	} else {
		result = is_lang_empty_cex(nfa_isect, cex);
	}

	return result;
} // is_incl_naive }}}


/// language inclusion check using Antichains
bool is_incl_antichains(
	const Nfa&         smaller,
	const Nfa&         bigger,
	const Alphabet&    alphabet,
	Word*              cex,
	const StringDict&  params)
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
		if (rhs_bigger.size() > lhs_bigger.size()) { // bigger set cannot be subset
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
	for (const auto& state : smaller.initialstates) {
		if (smaller.has_final(state) &&
			are_disjoint(bigger.initialstates, bigger.finalstates))
		{
			if (nullptr != cex) { cex->clear(); }
			return false;
		}

		ProdStateType st = std::make_pair(state, bigger.initialstates);
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
			const Symbol& symb = post_symb.first;

			for (const State& smaller_succ : post_symb.second) {
				StateSet bigger_succ = bigger.post(bigger_set, symb);
				ProdStateType succ = {smaller_succ, bigger_succ};

				if (smaller.has_final(smaller_succ) &&
					are_disjoint(bigger_succ, bigger.finalstates))
				{
					if (nullptr != cex) {
						cex->clear();
						cex->push_back(symb);
						ProdStateType trav = prod_state;
						while (paths[trav].first != trav)
						{ // go back until initial state
							cex->push_back(paths[trav].second);
							trav = paths[trav].first;
						}

						std::reverse(cex->begin(), cex->end());
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

} // namespace


// The dispatching method that calls the correct one based on parameters
bool Vata2::Nfa::is_incl(
	const Nfa&         smaller,
	const Nfa&         bigger,
	const Alphabet&    alphabet,
	Word*              cex,
	const StringDict&  params)
{ // {{{

	// setting the default algorithm
	decltype(is_incl_naive)* algo = is_incl_naive;
	if (!haskey(params, "algo")) {
		throw std::runtime_error(std::to_string(__func__) +
			" requires setting the \"algo\" key in the \"params\" argument; "
			"received: " + std::to_string(params));
	}

	const std::string& str_algo = params.at("algo");
	if ("naive" == str_algo) { }
	else if ("antichains" == str_algo) {
		algo = is_incl_antichains;
	} else {
		throw std::runtime_error(std::to_string(__func__) +
			" received an unknown value of the \"algo\" key: " + str_algo);
	}

	return algo(smaller, bigger, alphabet, cex, params);
} // is_incl }}}
