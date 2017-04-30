// TODO: add header

#ifndef _VATA_NG_NFA_HH_
#define _VATA_NG_NFA_HH_

#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

namespace std
{ // {{{
	/**
	 * @brief  A hasher for pairs
	 */
	template <class A, class B>
	struct hash<std::pair<A,B>>
	{
		size_t operator()(const std::pair<A,B>& k) const
		{ // {{{
			// TODO: check whether it is OK
			size_t seed = k.first;
      seed ^= k.second + 0x9e3779b9 + (seed<<6) + (seed>>2);
			return seed;
		} // operator() }}}
	};
} // namespace std }}}



namespace VataNG
{
namespace Nfa
{

// START OF THE DECLARATIONS

using State = uintptr_t;
using Symbol = uintptr_t;

using ProductMap = std::unordered_map<std::pair<State, State>, State>;

/**
 * @brief  A transition
 */
struct Trans
{
	State src;
	Symbol symb;
	State tgt;

	Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) { }
};


/**
 * @brief  An NFA
 */
struct Nfa
{
	std::set<State> initialstates = {};
	std::set<State> finalstates = {};

	/**
	 * @brief  The transitions of the NFA
	 *
	 * This data structure needs to be refined in future.
	 */
	std::vector<Trans> transitions = {};
};


void add_trans(Nfa* nfa, const Trans* trans);
void add_trans(Nfa* nfa, State src, Symbol symb, State tgt);

bool are_disjoint(const Nfa* lhs, const Nfa* rhs);
void intersection(Nfa* result, const Nfa* lhs, const Nfa* rhs, ProductMap* prod_map = nullptr);


// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* VataNG */

#endif /* _VATA_NG_NFA_HH_ */
