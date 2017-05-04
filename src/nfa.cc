// TODO: add header

#include <vata-ng/nfa.hh>
#include <vata-ng/util.hh>

#include <list>
#include <unordered_set>

using std::tie;
using namespace VataNG::util;

void VataNG::Nfa::Nfa::add_trans(const Trans* trans)
{ // {{{
	assert(nullptr != trans);

	auto it = this->transitions.find(trans->src);
	if (it != this->transitions.end())
	{
		PostSymb& post = it->second;
		auto jt = post.find(trans->symb);
		if (jt != post.end())
		{
			jt->second.insert(trans->tgt);
		}
		else
		{
			post.insert({trans->symb, StateSet({trans->tgt})});
		}
	}
	else
	{
		this->transitions.insert(
			{trans->src, PostSymb({{trans->symb, StateSet({trans->tgt})}})});
	}
} // add_trans }}}

bool VataNG::Nfa::Nfa::has_trans(const Trans* trans) const
{ // {{{
	assert(nullptr != trans);

	auto it = this->transitions.find(trans->src);
	if (it == this->transitions.end())
	{
		return false;
	}

	const PostSymb& post = it->second;
	auto jt = post.find(trans->symb);
	if (jt == post.end())
	{
		return false;
	}

	return jt->second.find(trans->tgt) != jt->second.end();
} // has_trans }}}

VataNG::Nfa::Nfa::const_iterator VataNG::Nfa::Nfa::const_iterator::for_begin(const Nfa* nfa)
{ // {{{
	assert(nullptr != nfa);

	const_iterator result;
	if (nfa->transitions.empty())
	{
		result.is_end = true;
		return result;
	}

	result.nfa = nfa;
	result.stpmIt = nfa->transitions.begin();
	const PostSymb& post = result.stpmIt->second;
	assert(!post.empty());
	result.psIt = post.begin();
	const StateSet& state_set = result.psIt->second;
	assert(!state_set.empty());
	result.ssIt = state_set.begin();

	result.refresh_trans();

	return result;
} // for_begin }}}

VataNG::Nfa::Nfa::const_iterator& VataNG::Nfa::Nfa::const_iterator::operator++()
{ // {{{
	assert(nullptr != nfa);

	++(this->ssIt);
	const StateSet& state_set = this->psIt->second;
	assert(!state_set.empty());
	if (this->ssIt != state_set.end())
	{
		this->refresh_trans();
		return *this;
	}

	// out of state set
	++(this->psIt);
	const PostSymb& post_map = this->stpmIt->second;
	assert(!post_map.empty());
	if (this->psIt != post_map.end())
	{
		const StateSet& new_state_set = this->psIt->second;
		assert(!new_state_set.empty());
		this->ssIt = new_state_set.begin();

		this->refresh_trans();
		return *this;
	}

	// out of post map
	++(this->stpmIt);
	assert(!this->nfa->transitions.empty());
	if (this->stpmIt != this->nfa->transitions.end())
	{
		const PostSymb& new_post_map = this->stpmIt->second;
		assert(!new_post_map.empty());
		this->psIt = new_post_map.begin();
		const StateSet& new_state_set = this->psIt->second;
		assert(!new_state_set.empty());
		this->ssIt = new_state_set.begin();

		this->refresh_trans();
		return *this;
	}

	// out of transitions
	this->is_end = true;

	return *this;
} // operator++ }}}

bool VataNG::Nfa::are_state_disjoint(const Nfa* lhs, const Nfa* rhs)
{ // {{{
	assert(nullptr != lhs);
	assert(nullptr != rhs);

	// fill lhs_states with all states of lhs
	std::unordered_set<State> lhs_states;
	lhs_states.insert(lhs->initialstates.begin(), lhs->initialstates.end());
	lhs_states.insert(lhs->finalstates.begin(), lhs->finalstates.end());

	for (auto trans : *lhs)
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

	for (auto trans : *rhs)
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

void VataNG::Nfa::intersection(
	Nfa* result,
	const Nfa* lhs,
	const Nfa* rhs,
	ProductMap* prod_map)
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
			prod_map->insert({{lhs_st, rhs_st}, cnt_state});
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
		for (const auto lhs_tr : *lhs)
		{
			if (lhs_tr.src == lhs_st)
			{
				for (const auto rhs_tr : *rhs)
				{
					if (rhs_tr.src == rhs_st)
					{
						if (lhs_tr.symb == rhs_tr.symb)
						{
							// add a new transition
							State tgt_state;
							ProductMap::iterator it;
							bool ins;
							tie(it, ins) = prod_map->insert(
								{{lhs_tr.tgt, rhs_tr.tgt}, cnt_state});
							if (ins)
							{
								tgt_state = cnt_state;
								++cnt_state;

								worklist.push_back({lhs_tr.tgt, rhs_tr.tgt, tgt_state});
							}
							else
							{
								tgt_state = it->second;
							}

							result->add_trans(res_st, lhs_tr.symb, tgt_state);
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

bool VataNG::Nfa::is_lang_empty(const Nfa* aut, Cex* cex)
{ // {{{
	assert(nullptr != aut);

	std::list<State> worklist(
		aut->initialstates.begin(), aut->initialstates.end());
	std::unordered_set<State> processed(
		aut->initialstates.begin(), aut->initialstates.end());

	while (!worklist.empty())
	{
		State state = worklist.front();
		worklist.pop_front();

		if (aut->finalstates.find(state) != aut->finalstates.end())
		{
			// TODO process the CEX
			if (nullptr != cex)
			{
				*cex = {1,2};
			}

			return false;
		}

		const PostSymb* post_map = aut->get_post(state);
		if (nullptr == post_map) { continue; }
		for (const auto& symb_stateset : *post_map)
		{
			const StateSet& stateset = symb_stateset.second;
			for (auto tgt_state : stateset)
			{
				bool inserted;
				std::tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted) { worklist.push_back(tgt_state); }
			}
		}
	}

	return true;
} // is_lang_empty }}}

bool VataNG::Nfa::is_lang_universal(const Nfa* aut, Cex* cex)
{ // {{{
	assert(nullptr != aut);

	assert(false);

	*cex = {1,2};

	return true;

} // is_lang_universal }}}

void VataNG::Nfa::determinize(
	Nfa* result,
	const Nfa* aut,
	SubsetMap* subset_map)
{ // {{{
	assert(nullptr != result);
	assert(nullptr != aut);

	bool delete_map = false;
	if (nullptr == subset_map)
	{
		subset_map = new SubsetMap();
		delete_map = true;
	}

	State cnt_state = 0;
	std::list<std::pair<const StateSet*, State>> worklist;

	auto it_bool_pair = subset_map->insert({aut->initialstates, cnt_state});
	result->initialstates = {cnt_state};
	worklist.push_back({&it_bool_pair.first->first, cnt_state});
	++cnt_state;

	while (!worklist.empty())
	{
		const StateSet* state_set;
		State new_state;
		tie(state_set, new_state) = worklist.front();
		worklist.pop_front();

		// set the state final
		if (!are_disjoint(state_set, &aut->finalstates))
		{
			result->finalstates.insert(new_state);
		}
	}

	if (delete_map)
	{
		delete subset_map;
	}
} // determinize }}}
