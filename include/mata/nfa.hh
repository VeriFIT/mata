/* nfa.hh -- nondeterministic finite automaton (over finite words)
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
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

#ifndef _MATA_NFA_HH_
#define _MATA_NFA_HH_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <climits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// MATA headers
#include <mata/parser.hh>
#include <mata/util.hh>
#include <mata/ord_vector.hh>
#include <simlib/util/binary_relation.hh>

namespace Mata
{
namespace Nfa
{
extern const std::string TYPE_NFA;

// START OF THE DECLARATIONS
using State = unsigned long;
using StateSet = Mata::Util::OrdVector<State>;
using Symbol = unsigned long;

using PostSymb = std::unordered_map<Symbol, StateSet>;      ///< Post over a symbol.
using StateToPostMap = std::unordered_map<State, PostSymb>; ///< Transitions.

using ProductMap = std::unordered_map<std::pair<State, State>, State>;
using SubsetMap = std::unordered_map<StateSet, State>;
using Path = std::vector<State>;        ///< A finite-length path through automaton.
using Word = std::vector<Symbol>;       ///< A finite-length word.
using WordSet = std::set<Word>;         ///< A set of words.

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

/// A transition.
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

using TransSequence = std::vector<Trans>; ///< Set of transitions.

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
public:
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
Mata::Parser::ParsedSection serialize(
	const Nfa&                aut,
	const SymbolToStringMap*  symbol_map = nullptr,
	const StateToStringMap*   state_map = nullptr);


///  An NFA
struct TransSymbolStates {
    Symbol symbol;
    StateSet states_to;

    TransSymbolStates() = delete;
    explicit TransSymbolStates(Symbol symbolOnTransition) : symbol(symbolOnTransition), states_to() {}
    TransSymbolStates(Symbol symbolOnTransition, State states_to) :
            symbol(symbolOnTransition), states_to{states_to} {}
    TransSymbolStates(Symbol symbolOnTransition, const StateSet& states_to) :
            symbol(symbolOnTransition), states_to(states_to) {}

    inline bool operator<(const TransSymbolStates& rhs) const { return symbol < rhs.symbol; }
    inline bool operator<=(const TransSymbolStates& rhs) const { return symbol <= rhs.symbol; }
    inline bool operator>(const TransSymbolStates& rhs) const { return symbol > rhs.symbol; }
    inline bool operator>=(const TransSymbolStates& rhs) const { return symbol >= rhs.symbol; }
};

using TransitionList = Mata::Util::OrdVector<TransSymbolStates>;
using TransitionRelation = std::vector<TransitionList>;

struct Nfa
{
    /**
     * @brief For state q, transitionrelation[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to the number of states minus one.
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
     * @brief Construct a new explicit NFA with num_of_states states.
     */
    explicit Nfa(const unsigned long num_of_states) : transitionrelation(num_of_states), initialstates(), finalstates() {}

    auto get_num_of_states() const { return transitionrelation.size(); }

    void increase_size(size_t size)
    {
        assert(get_num_of_states() <= size);
        transitionrelation.resize(size);
    }

    /**
     * Increase size to include @p state.
     * @param state[in] The new state to be included.
     */
    void increase_size_for_state(const State state)
    {
        increase_size(state + 1);
    }

    void make_initial(State state) {
        if (this->get_num_of_states() <= state) {
            throw std::runtime_error("Cannot make state initial because it is not in automaton");
        }

        this->initialstates.insert(state);
    }
    void make_initial(const std::vector<State>& vec)
    { // {{{
        for (const State& st : vec) { this->make_initial(st); }
    } // }}}
    bool has_initial(const State &state_to_check) const {return initialstates.count(state_to_check);}
    void remove_initial(State state)
    {
        assert(has_initial(state));
        this->initialstates.remove(state);
    }

    void make_final(State state) {
        if (this->get_num_of_states() <= state) {
            throw std::runtime_error("Cannot make state final because it is not in automaton");
        }

        this->finalstates.insert(state);
    }
    void make_final(const std::vector<State>& vec)
    { // {{{
        for (const State& st : vec) { this->make_final(st); }
    } // }}}
    bool has_final(const State &state_to_check) const { return finalstates.count(state_to_check); }
    void remove_final(State state)
    {
        assert(has_final(state));
        this->finalstates.remove(state);
    }

    /**
     * Add a new state to the automaton.
     * @return The newly created state.
     */
    State add_new_state();
    bool is_state(const State &state_to_check) const { return state_to_check < transitionrelation.size(); }

    const TransitionList& get_transitions_from_state(State state_from) const
    {
        assert(!transitionrelation.empty());
        return transitionrelation[state_from];
    }

    /* Lukas: the above is nice. The good thing is that access to [q] is constant,
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

    /**
     * Remove transition.
     * @param src Source state of the transition to be removed.
     * @param symb Transition symbol of the transition to be removed.
     * @param tgt Target state of the transition to be removed.
     */
    void remove_trans(State src, Symbol symb, State tgt);

    /**
     * Remove transition.
     * @param trans Transition to be removed.
     */
    void remove_trans(const Trans& trans)
    {
        remove_trans(trans.src, trans.symb, trans.tgt);
    }

    bool has_trans(Trans trans) const
    {
        if (transitionrelation.empty())
            return false;
        const TransitionList& tl = get_transitions_from_state(trans.src);

        if (tl.empty())
            return false;
        for (auto& t : tl)
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
        return std::all_of(this->transitionrelation.begin(), this->transitionrelation.end(),
                    [](const auto& trans) {return trans.size() == 0;});
    }

    /**
     * Get transitions as a sequence of @c Trans.
     * @return Sequence of transitions as @c Trans.
     */
    TransSequence get_trans_as_sequence();

    void print_to_DOT(std::ostream &outputStream) const;
    static Nfa read_from_our_format(std::istream &inputStream);

    StateSet post(const StateSet& states, const Symbol& symbol) const
    {
        if (trans_empty())
            return StateSet{};

        StateSet res;
        for (auto state : states)
            for (const auto& symStates : transitionrelation[state])
                if (symStates.symbol == symbol)
                    res.insert(symStates.states_to);

        return res;
    }

    /**
     * Get shortest words (regarding their length) of the automaton using BFS.
     * @return Set of shortest words.
     */
    WordSet get_shortest_words() const;

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
        assert(state < transitionrelation.size());

        return transitionrelation[state];
    } // operator[] }}}
};

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
        SubsetMap*  subset_map = nullptr);

inline Nfa determinize(
        const Nfa&  aut,
        SubsetMap*  subset_map)
{ // {{{
    Nfa result;
    determinize(&result, aut, subset_map);
    return result;
} // determinize }}}

void invert(Nfa* result, const Nfa &aut);

inline Nfa invert(const Nfa &aut)
{
    Nfa inverted;
    invert(&inverted, aut);
    return inverted;
}

Simlib::Util::BinaryRelation compute_relation(
        const Nfa& aut,
        const StringDict&  params = {{"relation", "simulation"}, {"direction","forward"}});

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
        const Mata::Parser::ParsedSection&  parsec,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
void construct(
        Nfa*                                 aut,
        const Mata::Parser::ParsedSection&  parsec,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
inline Nfa construct(
        const Mata::Parser::ParsedSection&  parsec,
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

/**
 * Class mapping states to the shortest words accepted by languages of the states.
 */
class ShortestWordsMap
{
public:
    /**
     * Maps states in the automaton @p aut to shortest words accepted by languages of the states.
     */
    explicit ShortestWordsMap(const Nfa& aut)
        : reversed_automaton(revert(aut))
    {
        insert_initial_lengths();

        compute();
    }

    /**
     * Gets shortest words for the given @p states.
     * @param[in] states States to map shortest words for.
     * @return Set of shortest words.
     */
    WordSet get_shortest_words_for_states(const StateSet& states) const;

private:
    using WordLength = int; ///< A length of a word.
    /// Pair binding the length of all words in the word set and word set with words of the given length.
    using LengthWordsPair = std::pair<WordLength, WordSet>;
    /// Map mapping states to the shortest words accepted by the automaton from the mapped state.
    std::unordered_map<State, LengthWordsPair> shortest_words_map{};
    std::set<State> processed{}; ///< Set of already processed states.
    std::deque<State> lifo_queue{}; ///< LIFO queue for states to process.
    const Nfa reversed_automaton{}; ///< Reversed input automaton.

    /**
     * @brief Inserts initial lengths into the shortest words map.
     *
     * Inserts initial length of length 0 for final state in the automaton (initial states in the reversed automaton).
     */
    void insert_initial_lengths();

    /**
     * Computes shortest words for all states in the automaton.
     */
    void compute();

    /**
     * Computes shortest words for the given @p state.
     * @param[in] state State to compute shortest words for.
     */
    void compute_for_state(State state);

    /**
     * Creates default shortest words mapping for yet unprocessed @p state.
     * @param[in] state State to map default shortest words.
     * @return Created default shortest words map element for the given @p state.
     */
    LengthWordsPair map_default_shortest_words(const State state)
    {
        return shortest_words_map.emplace(state, std::make_pair(-1, WordSet{})).first->second;
    }

    /**
     * Update words for the current state.
     * @param[out] act Current state shortest words and length.
     * @param[in] dst Transition target state shortest words and length.
     * @param[in] symbol Symbol to update with.
     */
    static void update_current_words(LengthWordsPair& act, const LengthWordsPair& dst, Symbol symbol);
}; // ShortestWordsMap

// CLOSING NAMESPACES AND GUARDS
} /* Nfa */
} /* Mata */

namespace std
{ // {{{
template <>
struct hash<Mata::Nfa::Trans>
{
	inline size_t operator()(const Mata::Nfa::Trans& trans) const
	{
		size_t accum = std::hash<Mata::Nfa::State>{}(trans.src);
		accum = Mata::util::hash_combine(accum, trans.symb);
		accum = Mata::util::hash_combine(accum, trans.tgt);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Mata::Nfa::NfaWrapper& nfa_wrap);
} // std }}}


#endif /* _MATA_NFA_HH_ */
