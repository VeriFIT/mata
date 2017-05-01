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
using StateSet = std::set<State>;                           // set of states
using PostSymb = std::unordered_map<Symbol, StateSet>;      // post over a symbol
using StateToPostMap = std::unordered_map<State, PostSymb>; // transitions

using ProductMap = std::unordered_map<std::pair<State, State>, State>;

/**
 * @brief  A transition
 */
struct Trans
{
	State src;
	Symbol symb;
	State tgt;

	Trans() : src(), symb(), tgt() { }
	Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) { }
};


/**
 * @brief  An NFA
 */
struct Nfa
{
	std::set<State> initialstates = {};
	std::set<State> finalstates = {};
	StateToPostMap transitions = {};

	void add_trans(const Trans* trans);
	void add_trans(State src, Symbol symb, State tgt);

	struct const_iterator
	{ // {{{
		const Nfa* nfa;
		StateToPostMap::const_iterator stpmIt;
		PostSymb::const_iterator psIt;
		StateSet::const_iterator ssIt;
		Trans trans;
		bool is_end = { false };

		const_iterator() : nfa(), stpmIt(), psIt(), ssIt(), trans() { };
		static const_iterator for_begin(const Nfa* nfa);
		static const_iterator for_end(const Nfa* /* nfa */)
		{ // {{{
			const_iterator result;
			result.is_end = true;
			return result;
		} // }}}

		void refresh_trans()
		{ // {{{
			this->trans = {this->stpmIt->first, this->psIt->first, *(this->ssIt)};
		} // }}}

		const Trans& operator*() const {return this->trans; }

		bool operator==(const const_iterator& rhs) const
		{ // {{{
			if (this->is_end && rhs.is_end) { return true; }
			return ssIt == rhs.ssIt && psIt == rhs.psIt && stpmIt == rhs.stpmIt;
		} // }}}
		bool operator!=(const const_iterator& rhs) const { return !(*this == rhs);}
		const_iterator& operator++();
	}; // }}}

	const_iterator begin() const { return const_iterator::for_begin(this); }
	const_iterator end() const { return const_iterator::for_end(this); }
};


bool are_disjoint(const Nfa* lhs, const Nfa* rhs);
void intersection(Nfa* result, const Nfa* lhs, const Nfa* rhs, ProductMap* prod_map = nullptr);


// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* VataNG */

#endif /* _VATA_NG_NFA_HH_ */
