// TODO: add header

#ifndef _VATA2_NFA_HH_
#define _VATA2_NFA_HH_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

// VATA2 headers
#include <vata2/parser.hh>
#include <vata2/util.hh>

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
using Path = std::vector<State>;        /// a finite-length path through automaton
using Word = std::vector<Symbol>;       /// a finite-length word

using StringToStateMap = std::unordered_map<std::string, State>;
using StringToSymbolMap = std::unordered_map<std::string, Symbol>;

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
bool is_lang_empty(const Nfa* aut, Path* cex = nullptr);
/** Is the language of the automaton universal? */
bool is_lang_universal(const Nfa* aut, Path* cex = nullptr);

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

/** Loads an automaton from Parsed object */
void construct(
	Nfa* aut,
	const Vata2::Parser::Parsed* parsed,
	StringToSymbolMap* symbol_map = nullptr,
	StringToStateMap* state_map = nullptr);

/**
 * @brief  Obtains a word corresponding to a path in an automaton (or sets a flag)
 *
 * Returns a word that is consistent with @p path of states in automaton @p
 * aut, or sets a flag to @p false if such a word does not exist.
 *
 * @param[in]  aut   The automaton
 * @param[in]  path  The path of states
 *
 * @returns  A pair (word, bool) where if @p bool is @p true, then @p word is a
 *           word consistent with @p path, and if @p bool is @p false, this
 *           denotes that the path is invalid in @p aut
 */
std::pair<Word, bool> get_word_for_path(const Nfa& aut, const Path& path);

/** Encodes a vector of strings into a @c Word instance */
inline Word encode_word(
	const StringToSymbolMap&         symbol_map,
	const std::vector<std::string>&  input)
{
	Word result;
	for (auto str : input) { result.push_back(symbol_map.at(str)); }
	return result;
}

// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* Vata2 */

#endif /* _VATA2_NFA_HH_ */
