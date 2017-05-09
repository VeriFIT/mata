// TODO: add header

#ifndef _VATA2_NFA_HH_
#define _VATA2_NFA_HH_

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

	/**
	 * @brief  A hasher for sets
	 */
	template <class A>
	struct hash<std::set<A>>
	{
		size_t operator()(const std::set<A>& k) const
		{ // {{{
			// TODO: check whether it is OK
			size_t seed = 0;
			for (auto i : k)
			{
				seed ^= i + 0x9e3779b9 + (seed<<6) + (seed>>2);
			}
			return seed;
		} // operator() }}}
	};
} // namespace std }}}



namespace Vata2
{
namespace Nfa
{

// START OF THE DECLARATIONS

using State = uintptr_t;
using Symbol = uintptr_t;
using StateSet = std::set<State>;                           /// set of states
using PostSymb = std::unordered_map<Symbol, StateSet>;      /// post over a symbol
using StateToPostMap = std::unordered_map<State, PostSymb>; /// transitions

using ProductMap = std::unordered_map<std::pair<State, State>, State>;
using SubsetMap = std::unordered_map<StateSet, State>;
using Cex = std::vector<Symbol>;       /// counterexample

const PostSymb EMPTY_POST;

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

	bool has_initial(State state) const
	{ // {{{
		return this->initialstates.find(state) != this->initialstates.end();
	} // }}}
	bool has_final(State state) const
	{ // {{{
		return this->finalstates.find(state) != this->finalstates.end();
	} // }}}

	void add_trans(const Trans* trans);
	void add_trans(State src, Symbol symb, State tgt)
	{ // {{{
		Trans trans = {src, symb, tgt};
		this->add_trans(&trans);
	} // }}}
	bool has_trans(const Trans* trans) const;
	bool has_trans(State src, Symbol symb, State tgt) const
	{ // {{{
		Trans trans = {src, symb, tgt};
		return this->has_trans(&trans);
	} // }}}

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

	const PostSymb& operator[](State state) const
	{
		const PostSymb* post = get_post(state);
		if (nullptr == post)
		{
			return EMPTY_POST;
		}

		return *post;
	}

	const PostSymb* get_post(State state) const
	{ // {{{
		auto it = transitions.find(state);
		return (transitions.end() == it)? nullptr : &it->second;
	} // }}}
};

/** Do the automata have disjoint sets of states? */
bool are_state_disjoint(const Nfa* lhs, const Nfa* rhs);
/** Is the language of the automaton empty? */
bool is_lang_empty(const Nfa* aut, Cex* cex = nullptr);
/** Is the language of the automaton universal? */
bool is_lang_universal(const Nfa* aut, Cex* cex = nullptr);

/** Compute intersection of a pair of automata */
void intersection(
	Nfa* result,
	const Nfa* lhs,
	const Nfa* rhs,
	ProductMap* prod_map = nullptr);

/** Determinize an automaton */
void determinize(
	Nfa* result,
	const Nfa* aut,
	SubsetMap* subset_map = nullptr);

/** .vtf output serializer */
std::string serialize_vtf(const Nfa* aut);


// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* Vata2 */

#endif /* _VATA2_NFA_HH_ */
