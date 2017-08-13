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

using StringDict = std::unordered_map<std::string, std::string>;

const PostSymb EMPTY_POST{};

/// A transition
struct Trans
{
	State src;
	Symbol symb;
	State tgt;

	Trans() : src(), symb(), tgt() { }
	Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) { }

	friend std::ostream& operator<<(std::ostream& strm, const Trans& trans);
};

// ALPHABET {{{
class Alphabet
{
public:

	/// translates a string into a symbol
	virtual Symbol translate_symb(const std::string& symb) = 0;
	/// also translates strings to symbols
	Symbol operator[](const std::string& symb) {return this->translate_symb(symb);}
	/// gets a list of symbols in the alphabet
	virtual std::list<Symbol> get_symbols() const
	{ // {{{
		throw std::runtime_error("Unimplemented");
	} // }}}

	/// complement of a set of symbols wrt the alphabet
	virtual std::list<Symbol> get_complement(const std::set<Symbol>& syms) const
	{ // {{{
		(void)syms;
		throw std::runtime_error("Unimplemented");
	} // }}}

	virtual ~Alphabet() { }
};

class OnTheFlyAlphabet : public Alphabet
{
private:
	StringToSymbolMap* symbol_map;
	Symbol cnt_symbol;

private:
	OnTheFlyAlphabet(const OnTheFlyAlphabet& rhs);
	OnTheFlyAlphabet& operator=(const OnTheFlyAlphabet& rhs);

public:

	OnTheFlyAlphabet(StringToSymbolMap* str_sym_map, Symbol init_symbol = 0) :
		symbol_map(str_sym_map), cnt_symbol(init_symbol)
	{
		assert(nullptr != symbol_map);
	}

	virtual Symbol translate_symb(const std::string& str) override;
};

class DirectAlphabet : public Alphabet
{
	virtual Symbol translate_symb(const std::string& str) override
	{
		Symbol symb;
		std::istringstream stream(str);
		stream >> symb;
		return symb;
	}
};

class CharAlphabet : public Alphabet
{
	virtual Symbol translate_symb(const std::string& str) override
	{
		if (str.length() == 3 &&
			((str[0] == '\'' && str[2] == '\'') ||
			(str[0] == '\"' && str[2] == '\"')
			 ))
		{ // direct occurence of a character
			return str[1];
		}

		Symbol symb;
		std::istringstream stream(str);
		stream >> symb;
		return symb;
	}
};

class EnumAlphabet : public Alphabet
{
private:
	StringToSymbolMap symbol_map;

private:
	EnumAlphabet(const EnumAlphabet& rhs);
	EnumAlphabet& operator=(const EnumAlphabet& rhs);

public:

	EnumAlphabet() : symbol_map() { }

	template <class InputIt>
	EnumAlphabet(InputIt first, InputIt last) : EnumAlphabet()
	{ // {{{
		size_t cnt = 0;
		for (; first != last; ++first)
		{
			bool inserted;
			std::tie(std::ignore, inserted) = symbol_map.insert({*first, cnt++});
			if (!inserted)
			{
				throw std::runtime_error("multiple occurrence of the same symbol");
			}
		}
	} // }}}

	EnumAlphabet(std::initializer_list<std::string> l) :
		EnumAlphabet(l.begin(), l.end())
	{ }

	virtual Symbol translate_symb(const std::string& str) override
	{
		auto it = symbol_map.find(str);
		if (symbol_map.end() == it)
		{
			throw std::runtime_error("unknown symbol \'" + str + "\'");
		}

		return it->second;
	}

	virtual std::list<Symbol> get_symbols() const override;
	virtual std::list<Symbol> get_complement(
		const std::set<Symbol>& syms) const override;
};
// }}}


struct Nfa;

/// .vtf output serializer
std::string serialize_vtf(const Nfa& aut);


///  An NFA
struct Nfa
{ // {{{
private:

	// private transitions in order to avoid the use of transitions.size() which
	// returns something else than expected (basically returns the number of
	// states with outgoing edges in the NFA
	StateToPostMap transitions = {};

public:

	std::set<State> initialstates = {};
	std::set<State> finalstates = {};

	bool has_initial(State state) const
	{ // {{{
		return Vata2::util::haskey(this->initialstates, state);
	} // }}}
	bool has_final(State state) const
	{ // {{{
		return Vata2::util::haskey(this->finalstates, state);
	} // }}}

	void add_trans(const Trans& trans);
	void add_trans(State src, Symbol symb, State tgt)
	{ // {{{
		this->add_trans({src, symb, tgt});
	} // }}}

	bool has_trans(const Trans& trans) const;
	bool has_trans(State src, Symbol symb, State tgt) const
	{ // {{{
		return this->has_trans({src, symb, tgt});
	} // }}}

	bool trans_empty() const { return this->transitions.empty();};// no transitions
	size_t trans_size() const;/// number of transitions; has linear time complexity

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

		const Trans& operator*() const { return this->trans; }

		bool operator==(const const_iterator& rhs) const
		{ // {{{
			if (this->is_end && rhs.is_end) { return true; }
			if ((this->is_end && !rhs.is_end) || (!this->is_end && rhs.is_end)) { return false; }
			return ssIt == rhs.ssIt && psIt == rhs.psIt && stpmIt == rhs.stpmIt;
		} // }}}
		bool operator!=(const const_iterator& rhs) const { return !(*this == rhs);}
		const_iterator& operator++();
	}; // }}}

	const_iterator begin() const { return const_iterator::for_begin(this); }
	const_iterator end() const { return const_iterator::for_end(this); }

	const PostSymb& operator[](State state) const
	{ // {{{
		const PostSymb* post = get_post(state);
		if (nullptr == post)
		{
			return EMPTY_POST;
		}

		return *post;
	} // operator[] }}}

	const PostSymb* get_post(State state) const
	{ // {{{
		auto it = transitions.find(state);
		return (transitions.end() == it)? nullptr : &it->second;
	} // get_post }}}

	/// gets a post of a set of states over a symbol
	StateSet get_post_of_set(const StateSet& macrostate, Symbol sym) const;
}; // Nfa }}}

/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Nfa& lhs, const Nfa& rhs);
/// Is the language of the automaton empty?
bool is_lang_empty(const Nfa& aut, Path* cex = nullptr);
bool is_lang_empty_cex(const Nfa& aut, Word* cex);


/// Is the language of the automaton universal?
bool is_universal(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	Word*              cex = nullptr,
	const StringDict&  params = {});

inline bool is_universal(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params)
{ // {{{
	return is_universal(aut, alphabet, nullptr, params);
} // }}}

/// Compute intersection of a pair of automata
void intersection(
	Nfa*         result,
	const Nfa&   lhs,
	const Nfa&   rhs,
	ProductMap*  prod_map = nullptr);

inline Nfa intersection(
	const Nfa&   lhs,
	const Nfa&   rhs,
	ProductMap*  prod_map = nullptr)
{ // {{{
	Nfa result;
	intersection(&result, lhs, rhs, prod_map);
	return result;
} // intersection }}}

/// Determinize an automaton
void determinize(
	Nfa*        result,
	const Nfa&  aut,
	SubsetMap*  subset_map = nullptr,
	State*      last_state_num = nullptr);

inline Nfa determinize(
	const Nfa&  aut,
	SubsetMap*  subset_map = nullptr,
	State*      last_state_num = nullptr)
{ // {{{
	Nfa result;
	determinize(&result, aut, subset_map, last_state_num);
	return result;
} // determinize }}}

/// makes the transition relation complete
void make_complete(
	Nfa*             aut,
	const Alphabet&  alphabet,
	State            sink_state);

/// Complement
void complement(
	Nfa*             result,
	const Nfa&       aut,
	const Alphabet&  alphabet,
	SubsetMap*       subset_map = nullptr);

inline Nfa complement(
	const Nfa&       aut,
	const Alphabet&  alphabet,
	SubsetMap*       subset_map = nullptr)
{ // {{{
	Nfa result;
	complement(&result, aut, alphabet, subset_map);
	return result;
} // complement }}}

/** Loads an automaton from Parsed object */
void construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsed,
	StringToSymbolMap*                   symbol_map = nullptr,
	StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
	const Vata2::Parser::ParsedSection&  parsed,
	StringToSymbolMap*                   symbol_map = nullptr,
	StringToStateMap*                    state_map = nullptr)
{ // {{{
	Nfa result;
	construct(&result, parsed, symbol_map, state_map);
	return result;
} // construct }}}

/** Loads an automaton from Parsed object */
void construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsed,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
	const Vata2::Parser::ParsedSection&  parsed,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map = nullptr)
{ // {{{
	Nfa result;
	construct(&result, parsed, alphabet, state_map);
	return result;
} // construct(Alphabet) }}}

/**
 * @brief  Obtains a word corresponding to a path in an automaton (or sets a flag)
 *
 * Returns a word that is consistent with @p path of states in automaton @p
 * aut, or sets a flag to @p false if such a word does not exist.  Note that
 * there may be several words with the same path (in case some pair of states
 * is connected by transitions over more than one symbol).
 *
 * @param[in]  aut   The automaton
 * @param[in]  path  The path of states
 *
 * @returns  A pair (word, bool) where if @p bool is @p true, then @p word is a
 *           word consistent with @p path, and if @p bool is @p false, this
 *           denotes that the path is invalid in @p aut
 */
std::pair<Word, bool> get_word_for_path(const Nfa& aut, const Path& path);


/// Checks whether a string is in the language of an automaton
bool is_in_lang(const Nfa& aut, const Word& word);

/// Checks whether the prefix of a string is in the language of an automaton
bool is_prfx_in_lang(const Nfa& aut, const Word& word);

/** Encodes a vector of strings (each corresponding to one symbol) into a @c Word instance */
inline Word encode_word(
	const StringToSymbolMap&         symbol_map,
	const std::vector<std::string>&  input)
{ // {{{
	Word result;
	for (auto str : input) { result.push_back(symbol_map.at(str)); }
	return result;
} // encode_word }}}

/// operator<<
std::ostream& operator<<(std::ostream& strm, const Nfa& nfa);

// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* Vata2 */

#endif /* _VATA2_NFA_HH_ */
