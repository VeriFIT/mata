// TODO: add header

#include <vata-ng/nfa.hh>

#include <list>
#include <unordered_set>

using std::tie;

void VataNG::Nfa::add_trans(Nfa* nfa, const Trans* trans)
{ // {{{
	assert(nullptr != nfa);
	assert(nullptr != trans);

	nfa->transitions.push_back(Trans(*trans));
} // add_trans }}}


void VataNG::Nfa::add_trans(Nfa* nfa, State src, Symbol symb, State tgt)
{ // {{{
	assert(nullptr != nfa);

	nfa->transitions.push_back(Trans(src, symb, tgt));
} // add_trans }}}


bool VataNG::Nfa::are_disjoint(const Nfa* lhs, const Nfa* rhs)
{ // {{{
	assert(nullptr != lhs);
	assert(nullptr != rhs);

	// fill lhs_states with all states of lhs
	std::unordered_set<State> lhs_states;
	lhs_states.insert(lhs->initialstates.begin(), lhs->initialstates.end());
	lhs_states.insert(lhs->finalstates.begin(), lhs->finalstates.end());

	for (auto trans : lhs->transitions)
	{
		lhs_states.insert({trans.src, trans.tgt});
	}

	// for every state found in rhs, check its presence in lhs_states
	for (auto rhs_st : rhs->initialstates)
	{
		if (lhs_states.find(rhs_st) != lhs_states.end())
		{
			return false;
		}
	}

	for (auto rhs_st : rhs->finalstates)
	{
		if (lhs_states.find(rhs_st) != lhs_states.end())
		{
			return false;
		}
	}

	for (auto trans : rhs->transitions)
	{
		if (lhs_states.find(trans.src) != lhs_states.end()
				|| lhs_states.find(trans.tgt) != lhs_states.end())
		{
			return false;
		}
	}

	// no common state found
	return true;
} // are_disjoint }}}

void VataNG::Nfa::intersection(Nfa* result, const Nfa* lhs, const Nfa* rhs, ProductMap* prod_map)
{ // {{{
	assert(nullptr != result);
	assert(nullptr != lhs);
	assert(nullptr != rhs);

	bool remove_prod_map = false;
	if (nullptr == prod_map)
	{
		remove_prod_map = true;
		prod_map = new ProductMap();
	}

	// counter for names of new states
	State cnt_state = 0;
	// list of elements the form <lhs_state, rhs_state, result_state>
	std::list<std::tuple<State, State, State>> worklist;

	// translate initial states and initialize worklist
	for (const auto lhs_st : lhs->initialstates)
	{
		for (const auto rhs_st : rhs->initialstates)
		{
			prod_map->insert(std::make_pair(std::make_pair(lhs_st, rhs_st), cnt_state));
			result->initialstates.insert(cnt_state);
			worklist.push_back(std::make_tuple(lhs_st, rhs_st, cnt_state));
			++cnt_state;
		}
	}

	while (!worklist.empty())
	{
		State lhs_st, rhs_st, res_st;
		tie(lhs_st, rhs_st, res_st) = worklist.front();
		worklist.pop_front();

		if (lhs->finalstates.find(lhs_st) != lhs->finalstates.end() &&
			rhs->finalstates.find(rhs_st) != rhs->finalstates.end())
		{
			result->finalstates.insert(res_st);
		}

		// TODO: a very inefficient implementation
		for (const auto lhs_tr : lhs->transitions)
		{
			if (lhs_tr.src == lhs_st)
			{
				for (const auto rhs_tr : rhs->transitions)
				{
					if (rhs_tr.src == rhs_st)
					{
						if (lhs_tr.symb == rhs_tr.symb)
						{
							// add a new transition
							State tgt_state;
							ProductMap::iterator it;
							bool ins;
							tie(it, ins) = prod_map->insert(std::make_pair(
								std::make_pair(lhs_tr.tgt, rhs_tr.tgt), cnt_state));
							if (ins)
							{
								tgt_state = cnt_state;
								++cnt_state;

								worklist.push_back(std::make_tuple(
									lhs_tr.tgt, rhs_tr.tgt, tgt_state));
							}
							else
							{
								tgt_state = it->second;
							}

							add_trans(result, res_st, lhs_tr.symb, tgt_state);
						}
					}
				}
			}
		}
	}

	if (remove_prod_map)
	{
		delete prod_map;
	}
} // intersection }}}
