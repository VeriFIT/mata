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
	if (nullptr == cex)
	{
		result = is_lang_empty(nfa_isect);
	}
	else
	{
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

	assert(false);
	// using WorklistType = std::list<StateSet>;
	// using ProcessedType = std::list<StateSet>;
  //
	// auto subsumes = [](const StateSet& lhs, const StateSet& rhs) {
	// 	return std::includes(rhs.begin(), rhs.end(), lhs.begin(), lhs.end());
	// };
  //
	// // process parameters
	// // TODO: set correctly!!!!
	// bool is_dfs = true;
  //
	// // check the initial state
	// if (are_disjoint(aut.initialstates, aut.finalstates))
	// {
	// 	if (nullptr != cex) { cex->clear(); }
  //
	// 	return false;
	// }
  //
	// // initialize
	// WorklistType worklist = { aut.initialstates };
	// ProcessedType processed = { aut.initialstates };
	// std::list<Symbol> alph_symbols = alphabet.get_symbols();
  //
	// // 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// // 'paths[s] == s' means that 's' is an initial state
	// std::map<StateSet, std::pair<StateSet, Symbol>> paths =
	// 	{ {aut.initialstates, {aut.initialstates, 0}} };
  //
	// while (!worklist.empty())
	// {
	// 	// get a next state
	// 	StateSet state;
	// 	if (is_dfs)
	// 	{
	// 		state = *worklist.rbegin();
	// 		worklist.pop_back();
	// 	}
	// 	else
	// 	{ // BFS
	// 		state = *worklist.begin();
	// 		worklist.pop_front();
	// 	}
  //
	// 	// process it
	// 	for (Symbol symb : alph_symbols)
	// 	{
	// 		StateSet succ = aut.post(state, symb);
	// 		if (are_disjoint(succ, aut.finalstates))
	// 		{
	// 			if (nullptr != cex)
	// 			{
	// 				cex->clear();
	// 				cex->push_back(symb);
	// 				StateSet trav = state;
	// 				while (paths[trav].first != trav)
	// 				{ // go back until initial state
	// 					cex->push_back(paths[trav].second);
	// 					trav = paths[trav].first;
	// 				}
  //
	// 				std::reverse(cex->begin(), cex->end());
	// 			}
  //
	// 			return false;
	// 		}
  //
	// 		bool is_subsumed = false;
	// 		for (const auto& anti_state : processed)
	// 		{ // trying to find a smaller state in processed
	// 			if (anti_state.size() > succ.size())
	// 			{ // larger sets cannot be subsets
	// 				continue;
	// 			}
  //
	// 			if (subsumes(anti_state, succ))
	// 			{
	// 				is_subsumed = true;
	// 				break;
	// 			}
	// 		}
  //
	// 		if (is_subsumed) { continue; }
  //
	// 		// prune data structures and insert succ inside
	// 		for (std::list<StateSet>* ds : {&processed, &worklist})
	// 		{
	// 			auto it = ds->begin();
	// 			while (it != ds->end())
	// 			{
	// 				if (subsumes(succ, *it))
	// 				{
	// 					auto to_remove = it;
	// 					++it;
	// 					ds->erase(to_remove);
	// 				}
	// 				else
	// 				{
	// 					++it;
	// 				}
	// 			}
  //
	// 			// TODO: set pushing strategy
	// 			ds->push_back(succ);
	// 		}
  //
	// 		// also set that succ was accessed from state
	// 		paths[succ] = {state, symb};
	// 	}
	// }
  //
	// return true;
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
	if (!haskey(params, "algo"))
	{
		throw std::runtime_error(std::to_string(__func__) +
			" requires setting the \"algo\" key in the \"params\" argument; "
			"received: " + std::to_string(params));
	}

	const std::string& str_algo = params.at("algo");
	if ("naive" == str_algo)
	{ }
	else if ("antichains" == str_algo)
	{
		algo = is_incl_antichains;
	}
	else
	{
		throw std::runtime_error(std::to_string(__func__) +
			" received an unknown value of the \"algo\" key: " + str_algo);
	}

	return algo(smaller, bigger, alphabet, cex, params);
} // is_incl }}}

