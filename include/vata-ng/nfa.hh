// TODO: add header

#ifndef _VATA_NG_NFA_HH_
#define _VATA_NG_NFA_HH_

#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

namespace VataNG
{
namespace Nfa
{

// START OF THE DECLARATIONS

using State = uintptr_t;
using Symbol = uintptr_t;

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

// NAMESPACES AND GUARDS

} /* Nfa */
} /* VataNG */

#endif /* _VATA_NG_NFA_HH_ */
