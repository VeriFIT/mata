/* afa.hh -- alternating finite automaton (over finite words)
 *
 * Copyright (c) TODO
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

#ifndef _MATA_AFA_HH_
#define _MATA_AFA_HH_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

// MATA headers
#include <mata/alphabet.hh>
#include <mata/nfa.hh>
#include <mata/parser.hh>
#include <mata/util.hh>
#include <mata/ord-vector.hh>
#include <mata/closed-set.hh>

namespace Mata
{
namespace Afa
{
extern const std::string TYPE_AFA;

// START OF THE DECLARATIONS

using State = Mata::Nfa::State;

template<typename T> using OrdVec = Mata::Util::OrdVector<T>;

using Node = OrdVec<State>;
using Nodes = OrdVec<Node>;

using SymbolToStringMap = Mata::Nfa::SymbolToStringMap;
using StateToStringMap = Mata::Nfa::StateToStringMap;
using StringToStateMap = Mata::Nfa::StringToStateMap;

using Path = Util::OrdVector<State>;
using Word = Util::OrdVector<Symbol>;

using StringDict = Mata::Nfa::StringMap;

using StateSet = OrdVec<State>;
using StateClosedSet = Mata::ClosedSet<Mata::Afa::State>;

/*
* A node is an ordered vector of states of the automaton.
* A transition consists of a source state, a symbol on the transition
* and a vector of nodes (which are the destination of the transition).
*
* In context of an AFA, the transition relation maps a state and a symbol
* to the positive Boolean formula over states, which is a Boolean formula
* using states in positive form, conjunctions and disjunctions. Since such
* a formula can be converted to the DNF, we can represent it as an ordered vector
* of nodes. The ordered vector represents a set of disjuncts. Each node corresponds
* to a single disjunct of a formula in DNF (states connected by conjunctions).
*
*/
struct Trans
{
	State src; // source state
	Symbol symb; // transition symbol
	Nodes dst; // a vector of vectors of states

	Trans() : src(), symb(), dst() { }
	Trans(State src, Symbol symb, Node dst) : src(src), symb(symb), dst(Nodes(dst)) { }
	Trans(State src, Symbol symb, Nodes dst) : src(src), symb(symb), dst(dst) { }

	bool operator==(const Trans& rhs) const
	{ // {{{
		return src == rhs.src && symb == rhs.symb && dst == rhs.dst ;
	} // operator== }}}
	bool operator!=(const Trans& rhs) const { return !this->operator==(rhs); }

}; // struct Trans

using TransList = std::vector<Trans>;
using TransRelation = std::vector<TransList>;

/* A tuple (result_node, precondition). The node result_node is a predecessor
* of a given node 'N' if the node 'precondition' is its subset. */
struct InverseResults{

	Node result_node{};
	Node precondition{};

	InverseResults() : result_node(), precondition() { }
	InverseResults(State state, Node precondition) : result_node(Node(state)),
	precondition(precondition) { }
	InverseResults(Node result_node, Node precondition) : result_node(result_node),
	precondition(precondition) { }

	bool operator==(InverseResults rhs) const
	{ // {{{
		return precondition == rhs.precondition && result_node == rhs.result_node;
	} // operator== }}}

	bool operator!=(InverseResults rhs) const
	{ // {{{
		return !this->operator==(rhs);
	} // operator!= }}}

	bool operator<(InverseResults rhs) const
	{ // {{{
		return precondition < rhs.precondition ||
		(precondition == rhs.precondition && result_node < rhs.result_node);
	} // operator< }}}

}; // struct InverseResults

/*
* A tuple (state, symb, inverseResults). The structure inverseResults contains tuples (inverseResult,
* precondition). If a node is a subset of 'precondition', the 'inverseResult' is a predecessor
* of the given node which is accessible through the symbol 'symb'.
* The state 'state' is always part of all 'preconditions' and it is a minimal element of them.
*/
struct InverseTrans{

	State state;
	Symbol symb;
	std::vector<InverseResults> inverseResults{};

	InverseTrans() : symb(), inverseResults() { }
	InverseTrans(Symbol symb) : symb(symb), inverseResults(std::vector<InverseResults>()) { }
	InverseTrans(Symbol symb, InverseResults inverseResults_) : symb(symb)
	{ inverseResults.push_back(inverseResults_); }
	InverseTrans(State state, Symbol symb, InverseResults inverseResults_) : state(state), symb(symb)
	{ inverseResults.push_back(inverseResults_); }

}; // struct InverseTrans

using InverseTransRelation = std::vector<std::vector<InverseTrans>>;

struct Afa;

/// serializes Afa into a ParsedSection
Mata::Parser::ParsedSection serialize(
	const Afa&                aut,
	const SymbolToStringMap*  symbol_map = nullptr,
	const StateToStringMap*   state_map = nullptr);


///  An AFA
struct Afa
{ // {{{
private:
    TransRelation transitionrelation{};
    InverseTransRelation inverseTransRelation{};

public:

	Afa() : transitionrelation(), inverseTransRelation() {}

	explicit Afa(const unsigned long num_of_states, const Nodes& initial_states = Nodes{},
		         const StateSet& final_states = StateSet{})
		: transitionrelation(num_of_states), inverseTransRelation(num_of_states),
		initialstates(initial_states), finalstates(final_states) {}

public:

	Nodes initialstates = {};
	StateSet finalstates = {};

	State add_new_state(void);

	auto get_num_of_states() const { return transitionrelation.size(); }

	void add_initial(State state) { this->initialstates.insert(Node({state})); }
	void add_initial(Node node) { this->initialstates.insert(node); }
	void add_initial(const std::vector<State> vec)
	{ // {{{
		Node node{};
		for (const State& st : vec) { node.insert(st); }
		this->initialstates.insert(node);
	} // }}}
	bool has_initial(State state) const
	{ // {{{
		return has_initial(Node({state}));
	} // }}}
	bool has_initial(Node node) const
	{ // {{{
		return StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1,
		initialstates).contains(node);
	} // }}}
	void add_final(State state) { this->finalstates.insert(state); }
	void add_final(const std::vector<State> vec)
	{ // {{{
		for (const State& st : vec) { this->add_final(st); }
	} // }}}
	bool has_final(State state) const
	{ // {{{
		return Mata::Util::haskey(this->finalstates, state);
	} // }}}

	void add_trans(const Trans& trans);
	void add_trans(State src, Symbol symb, State dst)
	{ // {{{
		this->add_trans({src, symb, Nodes(Node(dst))});
	} // }}}
	void add_trans(State src, Symbol symb, Node dst)
	{ // {{{
		this->add_trans({src, symb, Nodes(dst)});
	} // }}}
	void add_trans(State src, Symbol symb, Nodes dst)
	{ // {{{
		this->add_trans({src, symb, dst});
	} // }}}

	void add_inverse_trans(const Trans& trans);
	void add_inverse_trans(State src, Symbol symb, Node dst)
	{ // {{{
		this->add_inverse_trans({src, symb, Nodes(dst)});
	} // }}}
	void add_inverse_trans(State src, Symbol symb, Nodes dst)
	{ // {{{
		this->add_inverse_trans({src, symb, dst});
	} // }}}

	std::vector<InverseResults> perform_inverse_trans(State src, Symbol symb) const;
	std::vector<InverseResults> perform_inverse_trans(Node src, Symbol symb) const;

	bool has_trans(const Trans& trans) const;
	bool has_trans(State src, Symbol symb, Node dst) const
	{ // {{{
		return this->has_trans({src, symb, Nodes(dst)});
	} // }}}
	bool has_trans(State src, Symbol symb, Nodes dst) const
	{ // {{{
		return this->has_trans({src, symb, dst});
	} // }}}

	std::vector<Trans> get_trans_from_state(State state) const;
	Trans get_trans_from_state(State state, Symbol symbol) const;

	bool trans_empty() const {!transitionrelation.size();};// no transitions
	size_t trans_size() const;/// number of transitions; has linear time complexity


	StateClosedSet post(State state, Symbol symb) const;
	StateClosedSet post(Node node, Symbol symb) const;
	StateClosedSet post(Nodes nodes, Symbol symb) const;
	StateClosedSet post(StateClosedSet closed_set, Symbol symb) const;

	StateClosedSet post(Node node) const;
	StateClosedSet post(Nodes nodes) const;

	StateClosedSet post(StateClosedSet closed_set) const;

	StateClosedSet pre(Node node, Symbol symb) const;
	StateClosedSet pre(State state, Symbol symb) const {return pre(Node(state), symb);};
	StateClosedSet pre(Nodes nodes, Symbol symb) const;
	StateClosedSet pre(StateClosedSet closed_set, Symbol symb) const;

	StateClosedSet pre(Node node) const;
	StateClosedSet pre(Nodes nodes) const;

	StateClosedSet pre(StateClosedSet closed_set) const {return pre(closed_set.antichain());};

	StateClosedSet get_initial_nodes(void) const
	{
		StateClosedSet result = StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1);
		for(const auto& node : initialstates)
		{
			result.insert(node);
		}
		return result;
	}

	StateClosedSet get_non_initial_nodes(void) const {return StateClosedSet(upward_closed_set,
	0, transitionrelation.size()-1, initialstates).complement();};
	StateClosedSet get_final_nodes(void) const {return StateClosedSet(downward_closed_set,
	0, transitionrelation.size()-1, finalstates);};
	StateClosedSet get_non_final_nodes(void) const;

}; // Afa }}}


/// a wrapper encapsulating @p Afa for higher-level use
struct AfaWrapper
{ // {{{
	/// the AFA
	Afa afa;

	/// the alphabet
	Alphabet* alphabet;

	/// mapping of state names (as strings) to their numerical values
	StringToStateMap state_dict;
}; // AfaWrapper }}}


/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Afa& lhs, const Afa& rhs);
/// Is the language of the automaton empty?
bool is_lang_empty(const Afa& aut, Path* cex = nullptr);
bool is_lang_empty_cex(const Afa& aut, Word* cex);

bool antichain_concrete_forward_emptiness_test_old(const Afa& aut);
bool antichain_concrete_backward_emptiness_test_old(const Afa& aut);

bool antichain_concrete_forward_emptiness_test_new(const Afa& aut);
bool antichain_concrete_backward_emptiness_test_new(const Afa& aut);


/// Retrieves the states reachable from initial states
std::unordered_set<State> get_fwd_reach_states(const Afa& aut);

/// Is the language of the automaton universal?
bool is_universal(
	const Afa&         aut,
	const Alphabet&    alphabet,
	Word*              cex = nullptr,
	const StringDict&  params = {{"algorithm", "antichains"}});

inline bool is_universal(
	const Afa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params)
{ // {{{
	return is_universal(aut, alphabet, nullptr, params);
} // }}}

/// Does the language of the automaton contain epsilon?
bool accepts_epsilon(const Afa& aut);

/// Checks inclusion of languages of two automata (smaller <= bigger)?
bool is_incl(
	const Afa&         smaller,
	const Afa&         bigger,
	const Alphabet&    alphabet,
	Word*              cex = nullptr,
	const StringDict&  params = {{"algorithm", "antichains"}});

inline bool is_incl(
	const Afa&         smaller,
	const Afa&         bigger,
	const Alphabet&    alphabet,
	const StringDict&  params)
{ // {{{
	return is_incl(smaller, bigger, alphabet, nullptr, params);
} // }}}

/// Compute union of a pair of automata
/// Assumes that sets of states of lhs, rhs, and result are disjoint
void union_norename(
	Afa*        result,
	const Afa&  lhs,
	const Afa&  rhs);

/// Compute union of a pair of automata
inline Afa union_norename(
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
	Afa result;
	union_norename(&result, lhs, rhs);
	return result;
} // union_norename }}}

/// Compute union of a pair of automata
/// The states of the automata do not need to be disjoint; renaming will be done
Afa union_rename(
	const Afa&  lhs,
	const Afa&  rhs);


/// makes the transition relation complete
void make_complete(
	Afa*             aut,
	const Alphabet&  alphabet,
	State            sink_state);

/// Reverting the automaton
void revert(Afa* result, const Afa& aut);

inline Afa revert(const Afa& aut)
{ // {{{
	Afa result;
	revert(&result, aut);
	return result;
} // revert }}}

/// Removing epsilon transitions
void remove_epsilon(Afa* result, const Afa& aut, Symbol epsilon);

inline Afa remove_epsilon(const Afa& aut, Symbol epsilon)
{ // {{{
	Afa result;
	remove_epsilon(&result, aut, epsilon);
	return result;
} // }}}


/// Minimizes an AFA.  The method can be set using @p params
void minimize(
	Afa*               result,
	const Afa&         aut,
	const StringDict&  params = {});


inline Afa minimize(
	const Afa&         aut,
	const StringDict&  params = {})
{ // {{{
	Afa result;
	minimize(&result, aut, params);
	return result;
} // minimize }}}


/// Test whether an automaton is deterministic, i.e., whether it has exactly
/// one initial state and every state has at most one outgoing transition over
/// every symbol.  Checks the whole automaton, not only the reachable part
bool is_deterministic(const Afa& aut);

/// Test for automaton completeness wrt an alphabet.  An automaton is complete
/// if every reachable state has at least one outgoing transition over every
/// symbol.
bool is_complete(const Afa& aut, const Alphabet& alphabet);

/** Loads an automaton from Parsed object */
Afa construct(
	    const Mata::Parser::ParsedSection&   parsec,
	    Alphabet*                            alphabet,
	    StringToStateMap*                    state_map = nullptr);
/**
 * Loads automaton from intermediate automaton
 */
Afa construct(
        const Mata::IntermediateAut&         inter_aut,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map = nullptr);

/**
 * Loads automaton from parsed object (either ParsedSection or Intermediate automaton.
 * If user does not provide symbol map or state map, it allocates its own ones.
 */
template <class ParsedObject>
Afa construct(
        const ParsedObject&                  parsed,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr)
{ // {{{
    StringToSymbolMap tmp_symbol_map;
    if (symbol_map) {
        tmp_symbol_map = *symbol_map;
    }
    Mata::OnTheFlyAlphabet alphabet(tmp_symbol_map);

    Afa aut = construct(parsed, &alphabet, state_map);

    if (symbol_map) {
        *symbol_map = alphabet.get_symbol_map();
    }
    return aut;
}

/** Loads an automaton from Parsed object */
template <class ParsedObject>
void construct(
        Afa*                                 result,
        const ParsedObject&                  parsed,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr)
{ // {{{
    *result = construct(parsed, symbol_map, state_map);
} // construct }}}

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
std::pair<Word, bool> get_word_for_path(const Afa& aut, const Path& path);


/// Checks whether a string is in the language of an automaton
bool is_in_lang(const Afa& aut, const Word& word);

/// Checks whether the prefix of a string is in the language of an automaton
bool is_prfx_in_lang(const Afa& aut, const Word& word);

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
inline Word encode_word(
	const StringToSymbolMap&         symbol_map,
	const std::vector<std::string>&  input)
{ // {{{
	Word result;
	for (auto str : input) { result.insert(symbol_map.at(str)); }
	return result;
} // encode_word }}}

/// operator<<
std::ostream& operator<<(std::ostream& os, const Afa& afa);

/// global constructor to be called at program startup (from vm-dispatch)
void init();

// CLOSING NAMESPACES AND GUARDS
} /* Afa */
} /* Mata */

namespace std
{ // {{{
template <>
struct hash<Mata::Afa::Trans>
{
	inline size_t operator()(const Mata::Afa::Trans& trans) const
	{
		size_t accum = std::hash<Mata::Afa::State>{}(trans.src);
		accum = Mata::Util::hash_combine(accum, trans.symb);
		accum = Mata::Util::hash_combine(accum, trans.dst);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Mata::Afa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Mata::Afa::AfaWrapper& afa_wrap);
} // std }}}


#endif /* _MATA_AFA_HH_ */
