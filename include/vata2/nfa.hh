/* nfa.hh -- nondeterministic finite automaton (over finite words)
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VATA2_NFA_HH_
#define _VATA2_NFA_HH_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// VATA2 headers
#include <vata2/parser.hh>
#include <vata2/util.hh>
#include <vata2/ord_vector.hh>

#define EFFICIENT

namespace Vata2
{
namespace Nfa
{
extern const std::string TYPE_NFA;

// START OF THE DECLARATIONS
#ifdef EFFICIENT
using State = unsigned long;
using StateSet = Vata2::Util::OrdVector<State>;
using Symbol = unsigned long;
#else
using State = uintptr_t;
using Symbol = uintptr_t;
using StateSet = std::set<State>;                           /// set of states
#endif
using PostSymb = std::unordered_map<Symbol, StateSet>;      /// post over a symbol
using StateToPostMap = std::unordered_map<State, PostSymb>; /// transitions

using ProductMap = std::unordered_map<std::pair<State, State>, State>;
using SubsetMap = std::unordered_map<StateSet, State>;
using Path = std::vector<State>;        /// a finite-length path through automaton
using Word = std::vector<Symbol>;       /// a finite-length word

using StringToStateMap = std::unordered_map<std::string, State>;
using StringToSymbolMap = std::unordered_map<std::string, Symbol>;
using StateToStringMap = std::unordered_map<State, std::string>;
using SymbolToStringMap = std::unordered_map<Symbol, std::string>;

using StringDict = std::unordered_map<std::string, std::string>;

const PostSymb EMPTY_POST{};

static const struct Limits {
    State maxState = LONG_MAX;
    State minState = 0;
    Symbol maxSymbol = LONG_MAX;
    Symbol minSymbol = 0;
} limits;

/// A transition
struct Trans
{
	State src;
	Symbol symb;
	State tgt;

	Trans() : src(), symb(), tgt() { }
	Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) { }

	bool operator==(const Trans& rhs) const
	{ // {{{
		return src == rhs.src && symb == rhs.symb && tgt == rhs.tgt;
	} // operator== }}}
	bool operator!=(const Trans& rhs) const { return !this->operator==(rhs); }
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

	virtual std::list<Symbol> get_symbols() const override;
	virtual Symbol translate_symb(const std::string& str) override;
	virtual std::list<Symbol> get_complement(
		const std::set<Symbol>& syms) const override;
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
public:

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

	virtual std::list<Symbol> get_symbols() const override;
	virtual std::list<Symbol> get_complement(
		const std::set<Symbol>& syms) const override;
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

/// serializes Nfa into a ParsedSection
Vata2::Parser::ParsedSection serialize(
	const Nfa&                aut,
	const SymbolToStringMap*  symbol_map = nullptr,
	const StateToStringMap*   state_map = nullptr);


///  An NFA
#ifdef EFFICIENT
struct TransSymbolStates {
    Symbol symbol;
    StateSet states_to;

    TransSymbolStates() = delete;
    TransSymbolStates(Symbol symbolOnTransition) : symbol(symbolOnTransition) {}
    TransSymbolStates(Symbol symbolOnTransition, State states_to) :
            symbol(symbolOnTransition), states_to{states_to} {}
    TransSymbolStates(Symbol symbolOnTransition, StateSet states_to) :
            symbol(symbolOnTransition), states_to(std::move(states_to)) {}

    inline bool operator<(const TransSymbolStates& rhs) const { return symbol < rhs.symbol; }
    inline bool operator<=(const TransSymbolStates& rhs) const { return symbol <= rhs.symbol; }
    inline bool operator>(const TransSymbolStates& rhs) const { return symbol > rhs.symbol; }
    inline bool operator>=(const TransSymbolStates& rhs) const { return symbol >= rhs.symbol; }
};

using TransitionList = Vata2::Util::OrdVector<TransSymbolStates>;
using TransitionRelation = std::vector<TransitionList>;

struct Nfa
{
    /**
     * @brief For state q, transitionrelation[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to
     *
     * @todo maybe have this as its own class
     */
    TransitionRelation transitionrelation;
    StateSet initialstates = {};
    StateSet finalstates = {};

    //TODO we probably need the number of states, int, as a member, for when we want to remove states
    // alphabet?
public:
    Nfa () : transitionrelation(), initialstates(), finalstates() {}
    /**
     * @brief Construct a new Explicit NFA with num_of_states states
     */
    Nfa(unsigned long num_of_states) : transitionrelation(num_of_states), initialstates(), finalstates() {}

    //void addState(State stateToAdd);

    auto get_num_of_states() const { return std::max(
            {transitionrelation.size(), initialstates.size(), finalstates.size()}); }

    void increase_size(size_t size)
    {
        assert(get_num_of_states() <= size);
        transitionrelation.resize(size);
    }

    // TODO: exceptions if states do not exist
    void add_initial(State state) { this->initialstates.insert(state); }
    void add_initial(const std::vector<State> vec)
    { // {{{
        for (const State& st : vec) { this->add_initial(st); }
    } // }}}
    bool has_initial(const State &state_to_check) const {return initialstates.count(state_to_check);}

    void add_final(State state) { this->finalstates.insert(state); }
    void add_final(const std::vector<State> vec)
    { // {{{
        for (const State& st : vec) { this->add_final(st); }
    } // }}}
    bool has_final(const State &state_to_check) const { return finalstates.count(state_to_check); }

    /**
     * @brief Returns a newly created state.
     */
    State add_new_state();
    bool is_state(const State &state_to_check) const { return state_to_check < transitionrelation.size(); }

    const TransitionList& get_transitions_from_state(State state_from) const
    {
        assert(!transitionrelation.empty());
        return transitionrelation[state_from];
    }

    /* Lukas: the above is nice. The good thing is that acces to [q] is constant,
     * so one can iterate over all states for instance using this, and it is fast.
     * But I don't know how to do a similar thing inside TransitionList.
     * Returning a transition of q with the symbol a means to search for it in the list,
     * so iteration over the entire list would be very inefficient.
     * An efficient iteration would probably need an interface for an iterator, I don't know...
     * */

    /**
     * @brief Adds a transition from stateFrom trough symbol to stateTo.
     *
     * TODO: If stateFrom or stateTo are not in the set of states of this automaton, there should probably be exception.
     */
    void add_trans(State src, Symbol symb, State tgt);
    void add_trans(const Trans& trans)
    {
        add_trans(trans.src, trans.symb, trans.tgt);
    }

    bool has_trans(Trans trans) const
    {
        if (transitionrelation.empty())
            return false;
        const TransitionList& tl = get_transitions_from_state(trans.src);

        if (tl.empty())
            return false;
        for (auto t : tl)
        {
            if (t.symbol > trans.symb)
                return false;
            if (trans.symb == t.symbol && t.states_to.count(trans.tgt))
                return true;
            assert(t.symbol <= trans.symb);
        }

        return false;
    }
    bool has_trans(State src, Symbol symb, State tgt) const
    { // {{{
        return this->has_trans({src, symb, tgt});
    } // }}}

    bool trans_empty() const { return this->transitionrelation.empty();} /// no transitions
    size_t trans_size() const {return transitionrelation.size();} /// number of transitions; has linear time complexity
    bool nothing_in_trans() const
    {
        for (const auto& trans : this->transitionrelation) {
            if (trans.size() > 0)
                return false;
        }

        return true;
    }

    void print_to_DOT(std::ostream &outputStream) const;
    static Nfa read_from_our_format(std::istream &inputStream);

    StateSet post(const StateSet& states, const Symbol& symbol) const
    {
        if (trans_empty())
            return StateSet();

        StateSet res;
        for (auto state : states)
            for (const auto& symStates : transitionrelation[state])
                if (symStates.symbol == symbol)
                    res.insert(symStates.states_to);

        return res;
    }

    //class for iterating successors of a set of states represented as a StateSet
    //the iteration will take the symbols in from the smallest
    struct state_set_post_iterator {
    private:
        std::vector<State> state_vector;
        const Nfa &automaton;//or just give it a transition relation, that would make it more universal
        std::size_t size; // usable size of stateVector
        Symbol min_symbol;//the smallest symbol, for the next post
        std::vector<TransitionList::const_iterator> transition_iterators; //vector of iterators into TransitionLists (that are assumed sorted by symbol), for every state
    public:
        state_set_post_iterator(std::vector<State> states, const Nfa &aut);

        bool has_next() const;

        std::pair<Symbol, const StateSet> next();
    };


    struct const_iterator
    { // {{{
        const Nfa* nfa;
        size_t trIt;
        TransitionList::const_iterator tlIt;
        StateSet::const_iterator ssIt;
        Trans trans;
        bool is_end = { false };

        const_iterator() : nfa(), trIt(0), tlIt(), ssIt(), trans() { };
        static const_iterator for_begin(const Nfa* nfa);
        static const_iterator for_end(const Nfa* nfa);

        void refresh_trans()
        { // {{{
            this->trans = {trIt, this->tlIt->symbol, *(this->ssIt)};
        } // }}}

        const Trans& operator*() const { return this->trans; }

        bool operator==(const const_iterator& rhs) const
        { // {{{
            if (this->is_end && rhs.is_end) { return true; }
            if ((this->is_end && !rhs.is_end) || (!this->is_end && rhs.is_end)) { return false; }
            return ssIt == rhs.ssIt && tlIt == rhs.tlIt && trIt == rhs.trIt;
        } // }}}
        bool operator!=(const const_iterator& rhs) const { return !(*this == rhs);}
        const_iterator& operator++();
    }; // }}}

    const_iterator begin() const { return const_iterator::for_begin(this); }
    const_iterator end() const { return const_iterator::for_end(this); }

    const TransitionList& operator[](State state) const
    { // {{{
        if (state >= transitionrelation.size())
        {
            return TransitionList();
        }

        return transitionrelation[state];
    } // operator[] }}}
};
#else
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

	void add_initial(State state) { this->initialstates.insert(state); }
	void add_initial(const std::vector<State> vec)
	{ // {{{
		for (const State& st : vec) { this->add_initial(st); }
	} // }}}
	bool has_initial(State state) const
	{ // {{{
		return Vata2::util::haskey(this->initialstates, state);
	} // }}}
	void add_final(State state) { this->finalstates.insert(state); }
	void add_final(const std::vector<State> vec)
	{ // {{{
		for (const State& st : vec) { this->add_final(st); }
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
		const nfa* nfa;
		statetopostmap::const_iterator stpmit;
		postsymb::const_iterator psit;
		stateset::const_iterator ssit;
		trans trans;
		bool is_end = { false };

		const_iterator() : nfa(), stpmit(), psit(), ssit(), trans() { };
		static const_iterator for_begin(const nfa* nfa);
		static const_iterator for_end(const nfa* nfa);

		void refresh_trans()
		{ // {{{
			this->trans = {this->stpmit->first, this->psit->first, *(this->ssit)};
		} // }}}

		const trans& operator*() const { return this->trans; }

		bool operator==(const const_iterator& rhs) const
		{ // {{{
			if (this->is_end && rhs.is_end) { return true; }
			if ((this->is_end && !rhs.is_end) || (!this->is_end && rhs.is_end)) { return false; }
			return ssit == rhs.ssit && psit == rhs.psit && stpmit == rhs.stpmit;
		} // }}}
		bool operator!=(const const_iterator& rhs) const { return !(*this == rhs);}
		const_iterator& operator++();
	}; // }}}

	const_iterator begin() const { return const_iterator::for_begin(this); }
	const_iterator end() const { return const_iterator::for_end(this); }

	const PostSymb& operator[](State state) const
	{ // {{{
		const PostSymb* post_s = this->post(state);
		if (nullptr == post_s)
		{
			return EMPTY_POST;
		}

		return *post_s;
	} // operator[] }}}

	const PostSymb* post(State state) const
	{ // {{{
		auto it = transitions.find(state);
		return (transitions.end() == it)? nullptr : &it->second;
	} // post }}}

	/// gets a post of a set of states over a symbol
	StateSet post(const StateSet& macrostate, Symbol sym) const;

	// /// ostream& << operator
	// friend std::ostream& operator<<(std::ostream& os, const Nfa& nfa)
	// {
	// 	os << "{NFA|initial: " << std::to_string(nfa.initialstates) <<
	// 		"|final: " << std::to_string(nfa.finalstates) << "|transitions: ";
	// 	bool first = true;
	// 	for (auto tr : nfa) {
	// 		if (!first) {
	// 			os << ", ";
	// 		}
	// 		first = false;
	// 		os << std::to_string(tr);
	// 	}
	// 	os << "}";
	// 	return os;
	// }
}; // Nfa }}}
#endif

/// a wrapper encapsulating @p Nfa for higher-level use
struct NfaWrapper
{ // {{{
	/// the NFA
	Nfa nfa;

	/// the alphabet
	Alphabet* alphabet;

	/// mapping of state names (as strings) to their numerical values
	StringToStateMap state_dict;
}; // NfaWrapper }}}

#ifdef EFFICIENT
/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Nfa& lhs, const Nfa& rhs);

/// Is the language of the automaton empty?
bool is_lang_empty(const Nfa& aut, Path* cex = nullptr);

bool is_lang_empty_cex(const Nfa& aut, Word* cex);

void uni(Nfa *unionAutomaton, const Nfa &lhs, const Nfa &rhs);

inline Nfa uni(const Nfa &lhs, const Nfa &rhs)
{ // {{
    Nfa uni_aut;
    uni(&uni_aut, lhs, rhs);
    return uni_aut;
} // uni }}}

void intersection(
        Nfa*         res,
        const Nfa&   lhs,
        const Nfa&   rhs,
        ProductMap*  prod_map = nullptr);

inline Nfa intersection(const Nfa &lhs, const Nfa &rhs)
{
    Nfa result;
    intersection(&result, lhs, rhs);
    return result;
}

/// Compute union of a pair of automata
/// Assumes that sets of states of lhs, rhs, and result are disjoint
void union_norename(
        Nfa*        result,
        const Nfa&  lhs,
        const Nfa&  rhs); // TODO

/// Compute union of a pair of automata
inline Nfa union_norename(
        const Nfa&  lhs,
        const Nfa&  rhs)
{ // {{{
    assert(false); // TODO
}

/// makes the transition relation complete
void make_complete(
        Nfa*             aut,
        const Alphabet&  alphabet,
        State            sink_state);

// assumes deterministic automaton
void complement_in_place(Nfa &aut);

/// Co
void complement(
        Nfa*               result,
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params = {{"algo", "classical"}},
        SubsetMap*         subset_map = nullptr);

void complement_naive(
        Nfa*               result,
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params = {{"algo", "classical"}},
        SubsetMap*         subset_map = nullptr);

inline Nfa complement(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params = {{"algo", "classical"}},
        SubsetMap*         subset_map = nullptr)
{ // {{{
    Nfa result;
    complement(&result, aut, alphabet, params, subset_map);
    return result;
} // complement }}}

void minimize(Nfa* res, const Nfa &aut);

inline Nfa minimize(const Nfa &aut)
{
    Nfa minimized;
    minimize(&minimized, aut);
    return aut;
}

/// Determinize an automaton
void determinize(
        Nfa*        result,
        const Nfa&  aut,
        SubsetMap*  subset_map = nullptr,
        State*      last_state_num = nullptr);

inline Nfa determinize(
        const Nfa&  aut,
        SubsetMap*  subset_map,
        State*      last_state_num = nullptr)
{ // {{{
    Nfa result;
    determinize(&result, aut, subset_map, last_state_num);
    return result;
} // determinize }}}

void invert(Nfa* result, const Nfa &aut);

inline Nfa invert(const Nfa &aut)
{
    Nfa inverted;
    invert(&inverted, aut);
    return inverted;
}
// TODO: VATA::Util::BinaryRelation computeSimulation() const;

/// Is the language of the automaton universal?
bool is_universal(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        Word*              cex = nullptr,
        const StringDict&  params = {{"algo", "antichains"}});

inline bool is_universal(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params)
{ // {{{
    return is_universal(aut, alphabet, nullptr, params);
} // }}}

/// Checks inclusion of languages of two automata (smaller <= bigger)?
bool is_incl(
        const Nfa&         smaller,
        const Nfa&         bigger,
        const Alphabet&    alphabet,
        Word*              cex = nullptr,
        const StringDict&  params = {{"algo", "antichains"}});

inline bool is_incl(
        const Nfa&         smaller,
        const Nfa&         bigger,
        const Alphabet&    alphabet,
        const StringDict&  params)
{ // {{{
    return is_incl(smaller, bigger, alphabet, nullptr, params);
} // }}}

/// Reverting the automaton
void revert(Nfa* result, const Nfa& aut);

inline Nfa revert(const Nfa& aut)
{ // {{{
    Nfa result;
    revert(&result, aut);
    return result;
} // revert }}}

/// Removing epsilon transitions
void remove_epsilon(Nfa* result, const Nfa& aut, Symbol epsilon);

inline Nfa remove_epsilon(const Nfa& aut, Symbol epsilon)
{ // {{{
    Nfa result;
    remove_epsilon(&result, aut, epsilon);
    return result;
} // }}}

/// Test whether an automaton is deterministic, i.e., whether it has exactly
/// one initial state and every state has at most one outgoing transition over
/// every symbol.  Checks the whole automaton, not only the reachable part
bool is_deterministic(const Nfa& aut);

/// Test for automaton completeness wrt an alphabet.  An automaton is complete
/// if every reachable state has at least one outgoing transition over every
/// symbol.
bool is_complete(const Nfa& aut, const Alphabet& alphabet);

/** Loads an automaton from Parsed object */
void construct(
        Nfa*                                 aut,
        const Vata2::Parser::ParsedSection&  parsec,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
void construct(
        Nfa*                                 aut,
        const Vata2::Parser::ParsedSection&  parsec,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
        const Vata2::Parser::ParsedSection&  parsec,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr)
{ // {{{
    Nfa result;
    construct(&result, parsec, symbol_map, state_map);
    return result;
} // construct }}}

std::pair<Word, bool> get_word_for_path(const Nfa& aut, const Path& path);

/// Checks whether a string is in the language of an automaton
bool is_in_lang(const Nfa& aut, const Word& word);

/// Checks whether the prefix of a string is in the language of an automaton
bool is_prfx_in_lang(const Nfa& aut, const Word& word);

#else
/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Nfa& lhs, const Nfa& rhs);
/// Is the language of the automaton empty?
bool is_lang_empty(const Nfa& aut, Path* cex = nullptr);
bool is_lang_empty_cex(const Nfa& aut, Word* cex);

/// Retrieves the states reachable from initial states
std::unordered_set<State> get_fwd_reach_states(const Nfa& aut);

/// Is the language of the automaton universal?
bool is_universal(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	Word*              cex = nullptr,
	const StringDict&  params = {{"algo", "antichains"}});

inline bool is_universal(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params)
{ // {{{
	return is_universal(aut, alphabet, nullptr, params);
} // }}}

/// Does the language of the automaton contain epsilon?
bool accepts_epsilon(const Nfa& aut);

/// Checks inclusion of languages of two automata (smaller <= bigger)?
bool is_incl(
	const Nfa&         smaller,
	const Nfa&         bigger,
	const Alphabet&    alphabet,
	Word*              cex = nullptr,
	const StringDict&  params = {{"algo", "antichains"}});

inline bool is_incl(
	const Nfa&         smaller,
	const Nfa&         bigger,
	const Alphabet&    alphabet,
	const StringDict&  params)
{ // {{{
	return is_incl(smaller, bigger, alphabet, nullptr, params);
} // }}}

/// Compute union of a pair of automata
/// Assumes that sets of states of lhs, rhs, and result are disjoint
void union_norename(
	Nfa*        result,
	const Nfa&  lhs,
	const Nfa&  rhs);

/// Compute union of a pair of automata
inline Nfa union_norename(
	const Nfa&  lhs,
	const Nfa&  rhs)
{ // {{{
	Nfa result;
	union_norename(&result, lhs, rhs);
	return result;
} // union_norename }}}

/// Compute union of a pair of automata
/// The states of the automata do not need to be disjoint; renaming will be done
Nfa union_rename(
	const Nfa&  lhs,
	const Nfa&  rhs);

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
	Nfa*               result,
	const Nfa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params = {{"algo", "classical"}},
	SubsetMap*         subset_map = nullptr);

inline Nfa complement(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params = {{"algo", "classical"}},
	SubsetMap*         subset_map = nullptr)
{ // {{{
	Nfa result;
	complement(&result, aut, alphabet, params, subset_map);
	return result;
} // complement }}}

/// Reverting the automaton
void revert(Nfa* result, const Nfa& aut);

inline Nfa revert(const Nfa& aut)
{ // {{{
	Nfa result;
	revert(&result, aut);
	return result;
} // revert }}}

/// Removing epsilon transitions
void remove_epsilon(Nfa* result, const Nfa& aut, Symbol epsilon);

inline Nfa remove_epsilon(const Nfa& aut, Symbol epsilon)
{ // {{{
	Nfa result;
	remove_epsilon(&result, aut, epsilon);
	return result;
} // }}}


/// Minimizes an NFA.  The method can be set using @p params
void minimize(
	Nfa*               result,
	const Nfa&         aut,
	const StringDict&  params = {});


inline Nfa minimize(
	const Nfa&         aut,
	const StringDict&  params = {})
{ // {{{
	Nfa result;
	minimize(&result, aut, params);
	return result;
} // minimize }}}


/// Test whether an automaton is deterministic, i.e., whether it has exactly
/// one initial state and every state has at most one outgoing transition over
/// every symbol.  Checks the whole automaton, not only the reachable part
bool is_deterministic(const Nfa& aut);

/// Test for automaton completeness wrt an alphabet.  An automaton is complete
/// if every reachable state has at least one outgoing transition over every
/// symbol.
bool is_complete(const Nfa& aut, const Alphabet& alphabet);

/** Loads an automaton from Parsed object */
void construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map = nullptr,
	StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
	const Vata2::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map = nullptr,
	StringToStateMap*                    state_map = nullptr)
{ // {{{
	Nfa result;
	construct(&result, parsec, symbol_map, state_map);
	return result;
} // construct }}}

/** Loads an automaton from Parsed object */
void construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
	const Vata2::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map = nullptr)
{ // {{{
	Nfa result;
	construct(&result, parsec, alphabet, state_map);
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

#endif

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
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

/// global constructor to be called at program startup (from vm-dispatch)
void init();

// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* Vata2 */

namespace std
{ // {{{
template <>
struct hash<Vata2::Nfa::Trans>
{
	inline size_t operator()(const Vata2::Nfa::Trans& trans) const
	{
		size_t accum = std::hash<Vata2::Nfa::State>{}(trans.src);
		accum = Vata2::util::hash_combine(accum, trans.symb);
		accum = Vata2::util::hash_combine(accum, trans.tgt);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Vata2::Nfa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Vata2::Nfa::NfaWrapper& nfa_wrap);
} // std }}}


#endif /* _VATA2_NFA_HH_ */
