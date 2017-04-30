// TODO: add header

#include <vata-ng/nfa.hh>

#include <unordered_set>


void VataNG::Nfa::add_trans(Nfa* nfa, const Trans* trans)
{
	assert(nullptr != nfa);
	assert(nullptr != trans);

	nfa->transitions.push_back(Trans(*trans));
}


void VataNG::Nfa::add_trans(Nfa* nfa, State src, Symbol symb, State tgt)
{
	assert(nullptr != nfa);

	nfa->transitions.push_back(Trans(src, symb, tgt));
}


bool VataNG::Nfa::are_disjoint(const Nfa* lhs, const Nfa* rhs)
{
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
}
