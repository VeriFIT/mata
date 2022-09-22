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
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// MATA headers
#include <mata/parser.hh>
#include <mata/util.hh>
#include <mata/ord_vector.hh>
#include <mata/inter-aut.hh>
#include <simlib/util/binary_relation.hh>

namespace Mata
{
namespace Nfa
{
extern const std::string TYPE_NFA;

using State = unsigned long;
using StatePair = std::pair<State, State>;
using StateSet = Mata::Util::OrdVector<State>;
using Symbol = unsigned long;

using PostSymb = std::unordered_map<Symbol, StateSet>;      ///< Post over a symbol.

using ProductMap = std::unordered_map<StatePair, State>;
using SubsetMap = std::unordered_map<StateSet, State>;
using Path = std::vector<State>;        ///< A finite-length path through automaton.
using Word = std::vector<Symbol>;       ///< A finite-length word.
using WordSet = std::set<Word>;         ///< A set of words.

using StringToStateMap = std::unordered_map<std::string, State>;
using StringToSymbolMap = std::unordered_map<std::string, Symbol>;

template<typename Target>
using StateMap = std::unordered_map<State, Target>;

using StateToStringMap = StateMap<std::string>;
using StateToPostMap = StateMap<PostSymb>; ///< Transitions.
/// Mapping of states to states, used, for example, to map original states to reindexed states of new automaton, etc.
using StateToStateMap = StateMap<State>;

using SymbolToStringMap = std::unordered_map<Symbol, std::string>;

using StringDict = std::unordered_map<std::string, std::string>;

// ALPHABET {{{
class Alphabet
{
public:

    /// translates a string into a symbol
    virtual Symbol translate_symb(const std::string& symb) = 0;
    /// also translates strings to symbols
    Symbol operator[](const std::string& symb) { return this->translate_symb(symb); }
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

    explicit OnTheFlyAlphabet(StringToSymbolMap* str_sym_map, Symbol init_symbol = 0) :
            symbol_map(str_sym_map), cnt_symbol(init_symbol)
    {
        assert(nullptr != symbol_map);
    }

    std::list<Symbol> get_symbols() const override;
    Symbol translate_symb(const std::string& str) override;
    std::list<Symbol> get_complement(const std::set<Symbol>& syms) const override;
};

class DirectAlphabet : public Alphabet
{
public:
    Symbol translate_symb(const std::string& str) override
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

    Symbol translate_symb(const std::string& str) override
    {
        if (str.length() == 3 &&
            ((str[0] == '\'' && str[2] == '\'') ||
             (str[0] == '\"' && str[2] == '\"')
            ))
        { // direct occurrence of a character
            return str[1];
        }

        Symbol symb;
        std::istringstream stream(str);
        stream >> symb;
        return symb;
    }

    std::list<Symbol> get_symbols() const override;
    std::list<Symbol> get_complement(
            const std::set<Symbol>& syms) const override;
};

const PostSymb EMPTY_POST{};

static constexpr struct Limits {
    State maxState = std::numeric_limits<State>::max();
    State minState = std::numeric_limits<State>::min();
    Symbol maxSymbol = std::numeric_limits<Symbol>::max();
    Symbol minSymbol = std::numeric_limits<Symbol>::min();
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

struct Nfa; ///< A non-deterministic finite automaton.

template<typename T> using Sequence = std::vector<T>; ///< A sequence of elements.
using AutSequence = Sequence<Nfa>; ///< A sequence of non-deterministic finite automata.

template<typename T> using RefSequence = Sequence<std::reference_wrapper<T>>; ///< A sequence of references to elements.
using AutRefSequence = RefSequence<Nfa>; ///< A sequence of references to non-deterministic finite automata.
using ConstAutRefSequence = RefSequence<const Nfa>; ///< A sequence of const references to non-deterministic finite automata.

template<typename T> using PtrSequence = Sequence<T*>; ///< A sequence of pointers to elements.
using AutPtrSequence = PtrSequence<Nfa>; ///< A sequence of pointers to non-deterministic finite automata.
using ConstAutPtrSequence = PtrSequence<const Nfa>; ///< A sequence of pointers to const non-deterministic finite automata.

template<typename T> using ConstPtrSequence = Sequence<T* const>; ///< A sequence of const pointers to elements.
using AutConstPtrSequence = ConstPtrSequence<Nfa>; ///< A sequence of const pointers to non-deterministic finite automata.
using ConstAutConstPtrSequence = ConstPtrSequence<const Nfa>; ///< A sequence of const pointers to const non-deterministic finite automata.

using SharedPtrAut = std::shared_ptr<Nfa>; ///< A shared pointer to NFA.

/// serializes Nfa into a ParsedSection
Mata::Parser::ParsedSection serialize(
	const Nfa&                aut,
	const SymbolToStringMap*  symbol_map = nullptr,
	const StateToStringMap*   state_map = nullptr);


struct TransSymbolStates {
    Symbol symbol{};
    StateSet states_to;

    TransSymbolStates() = default;
    explicit TransSymbolStates(Symbol symbolOnTransition) : symbol(symbolOnTransition), states_to() {}
    TransSymbolStates(Symbol symbolOnTransition, State states_to) :
            symbol(symbolOnTransition), states_to{states_to} {}
    TransSymbolStates(Symbol symbolOnTransition, const StateSet& states_to) :
            symbol(symbolOnTransition), states_to(states_to) {}

    inline bool operator<(const TransSymbolStates& rhs) const { return symbol < rhs.symbol; }
    inline bool operator<=(const TransSymbolStates& rhs) const { return symbol <= rhs.symbol; }
    inline bool operator>(const TransSymbolStates& rhs) const { return symbol > rhs.symbol; }
    inline bool operator>=(const TransSymbolStates& rhs) const { return symbol >= rhs.symbol; }
    inline bool operator==(const TransSymbolStates& rhs) const { return symbol == rhs.symbol; }
};

/// List of transitions from a certain state. Each element holds transitions with a certain symbol.
using TransitionList = Mata::Util::OrdVector<TransSymbolStates>;
/// Transition relation for an NFA. Each index 'i' to the vector represents a state 'i' in the automaton.
using TransitionRelation = std::vector<TransitionList>;

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols
const Symbol EPSILON = limits.maxSymbol;

/**
 * A struct representing an NFA.
 */
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

public:
    Nfa() : transitionrelation(), initialstates(), finalstates() {}

    /**
     * @brief Construct a new explicit NFA with num_of_states states and optionally set initial and final states.
     */
    explicit Nfa(const unsigned long num_of_states, const StateSet& initial_states = StateSet{},
                 const StateSet& final_states = StateSet{})
        : transitionrelation(num_of_states), initialstates(initial_states), finalstates(final_states) {}

    /**
     * @brief Construct a new explicit NFA with already filled transition relation and optionally set initial and final states.
     */
    explicit Nfa(const TransitionRelation& transition_relation, const StateSet& initial_states = StateSet{},
                 const StateSet& final_states = StateSet{})
        : transitionrelation(transition_relation), initialstates(initial_states), finalstates(final_states) {}

    /**
     * Clear transitions but keep the automata states.
     */
    void clear_transitions() {
        for (auto& state_transitions: transitionrelation) {
            state_transitions.clear();
        }
    }

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

    /**
     * Clear initial states set.
     */
    void clear_initial() { initialstates.clear(); }

    /**
     * Make @p state initial.
     * @param state State to be added to initial states.
     */
    void make_initial(State state)
    {
        if (this->get_num_of_states() <= state) {
            throw std::runtime_error("Cannot make state initial because it is not in automaton");
        }

        this->initialstates.insert(state);
    }

    /**
     * @brief Reset initial states set to contain only @p state.
     *
     * Overwrite the previous initial states set.
     *
     * @param state State to be set as the new initial state.
     */
    void reset_initial(State state)
    {
        clear_initial();
        make_initial(state);
    }

    /**
     * Make @p vec of states initial states.
     * @param vec Vector of states to be added to initial states.
     */
    void make_initial(const std::vector<State>& vec)
    {
        for (const State& st : vec) { this->make_initial(st); }
    }

    /**
     * @brief Reset initial states set to contain only @p state.
     *
     * Overwrite the previous initial states set.
     *
     * @param vec Vector of states to be set as new initial states.
     */
    void reset_initial(const std::vector<State>& vec)
    {
        clear_initial();
        for (const State& st: vec) { this->make_initial(st); }
    }

    bool has_initial(const State &state_to_check) const {return initialstates.count(state_to_check);}

    /**
     * Remove @p state from initial states.
     * @param state[in] State to be removed from initial states.
     */
    void remove_initial(State state)
    {
        assert(has_initial(state));
        this->initialstates.remove(state);
    }

    /**
     * Remove @p vec of states from initial states.
     * @param vec[in] Vector of states to be removed from initial states.
     */
    void remove_initial(const std::vector<State>& vec)
    {
        for (const State& st : vec) { this->remove_initial(st); }
    }

    /**
     * Clear final states set.
     */
    void clear_final() { finalstates.clear(); }

    /**
     * Make @p state final.
     * @param state[in] State to be added to final states.
     */
    void make_final(const State state)
    {
        if (this->get_num_of_states() <= state) {
            throw std::runtime_error("Cannot make state final because it is not in automaton");
        }

        this->finalstates.insert(state);
    }

    /**
     * @brief Reset final states set to contain only @p state.
     *
     * Overwrite the previous final states set.
     *
     * @param state[in] State to be set as the new final state.
     */
    void reset_final(const State state)
    {
        clear_final();
        make_final(state);
    }

    /**
     * Make @p vec of states final states.
     * @param vec[in] Vector of states to be added to final states.
     */
    void make_final(const std::vector<State>& vec)
    {
        for (const State& st : vec) { this->make_final(st); }
    }

    /**
     * @brief Reset final states set to contain only @p state.
     *
     * Overwrite the previous final states set.
     *
     * @param vec[in] Vector of states to be set as new final states.
     */
    void reset_final(const std::vector<State>& vec)
    {
        clear_final();
        for (const State& st: vec) { this->make_final(st); }
    }

    bool has_final(const State &state_to_check) const { return finalstates.count(state_to_check); }

    /**
     * Remove @p state from final states.
     * @param state[in] State to be removed from final states.
     */
    void remove_final(State state)
    {
        assert(has_final(state));
        this->finalstates.remove(state);
    }

    /**
     * Remove @p vec of states from final states.
     * @param vec[in] Vector of states to be removed from final states.
     */
    void remove_final(const std::vector<State>& vec)
    {
        for (const State& st : vec) { this->remove_final(st); }
    }

    /**
     * Add a new state to the automaton.
     * @return The newly created state.
     */
    State add_new_state();

    /**
     * Unify initial states into a single new initial state.
     */
    void unify_initial() {
        if (initialstates.empty() || initialstates.size() == 1) { return; }
        const State new_initial_state{ add_new_state() };
        for (const auto& orig_initial_state: initialstates) {
            for (const auto& transitions: get_transitions_from(orig_initial_state)) {
                for (const State state_to: transitions.states_to) {
                    add_trans(new_initial_state, transitions.symbol, state_to);
                }
            }
            if (has_final(orig_initial_state)) { make_final(new_initial_state); }
        }
        reset_initial(new_initial_state);
    }

    /**
     * Unify final states into a single new final state.
     */
    void unify_final() {
        if (finalstates.empty() || finalstates.size() == 1) { return; }
        const State new_final_state{ add_new_state() };
        for (const auto& orig_final_state: finalstates) {
            for (const auto& transitions: get_transitions_to(orig_final_state)) {
                add_trans(transitions.src, transitions.symb, new_final_state);
            }
            if (has_initial(orig_final_state)) { make_initial(new_final_state); }
        }
        reset_final(new_final_state);
    }

    bool is_state(const State &state_to_check) const { return state_to_check < transitionrelation.size(); }

    /**
     * @brief Clear the underlying NFA to a blank NFA.
     *
     * The whole NFA is cleared, each member is set to its zero value.
     */
    void clear_nfa() {
        transitionrelation.clear();
        clear_initial();
        clear_final();
    }

    /**
     * @brief Reset the underlying NFA to the specified values.
     *
     * Clear the underlying NFA and set its members to the passed argument values.
     *
     * @param[in] num_of_states A number of states to reset to..
     * @param[in] initial_states A set of initial states to reset to.
     * @param[in] final_states A set of final states to reset to.
     */
    void reset_nfa(const unsigned long num_of_states, const StateSet& initial_states = StateSet{},
                   const StateSet& final_states = StateSet{}) {
        clear_nfa();
        increase_size(num_of_states);
        initialstates = initial_states;
        finalstates = final_states;
    }

    /**
     * @brief Get set of reachable states.
     *
     * Reachable states are states accessible from any initial state.
     * @return Set of reachable states.
     */
    StateSet get_reachable_states() const;

    /**
     * @brief Get set of terminating states.
     *
     * Terminating states are states leading to any final state.
     * @return Set of terminating states.
     */
    StateSet get_terminating_states() const;

    /**
     * @brief Get a set of useful states.
     *
     * Useful states are reachable and terminating states.
     * @return Set of useful states.
     */
    StateSet get_useful_states() const;

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     */
    void trim();

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @return Trimmed automaton.
     */
    Nfa get_trimmed_automaton();

    // FIXME: Resolve this comment and delete it.
    /* Lukas: the above is nice. The good thing is that access to [q] is constant,
     * so one can iterate over all states for instance using this, and it is fast.
     * But I don't know how to do a similar thing inside TransitionList.
     * Returning a transition of q with the symbol a means to search for it in the list,
     * so iteration over the entire list would be very inefficient.
     * An efficient iteration would probably need an interface for an iterator, I don't know...
     * */

    /**
     * Add transition from @p state_from with @p symbol to @p state_to to automaton.
     * @param state_from Source state.
     * @param symbol Symbol on transition.
     * @param state_to Target states.
     */
    void add_trans(State state_from, Symbol symbol, State state_to);

    /**
     * Add transition @p trans to automaton.
     * @param trans Transition to add.
     */
    void add_trans(const Trans& trans) { add_trans(trans.src, trans.symb, trans.tgt); }

    /**
     * Add transitions from @p state_from with @p symbol to @p states_to to automaton.
     * @param state_from Source state.
     * @param symbol Symbol on transitions.
     * @param states_to Set of target states.
     */
    void add_trans(State state_from, Symbol symbol, const StateSet& states_to);

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

    /**
     * Remove epsilon transitions from the automaton.
     */
    void remove_epsilon(Symbol epsilon);

    bool has_trans(Trans trans) const
    {
        if (transitionrelation.empty()) {
            return false;
        }

        const TransitionList& tl = get_transitions_from(trans.src);
        if (tl.empty()) {
            return false;
        }
        auto symbol_transitions{ tl.find(TransSymbolStates{trans.symb} ) };
        if (symbol_transitions == tl.end()) {
            return false;
        }

        if (symbol_transitions->states_to.find(trans.tgt) == symbol_transitions->states_to.end()) {
            return false;
        }

        return true;
    }
    bool has_trans(State src, Symbol symb, State tgt) const
    { // {{{
        return this->has_trans({src, symb, tgt});
    } // }}}

    /**
     * Get transitions from @p state_from using transition @p symbol.
     * @param state_from State from which to get transitions.
     * @param symbol Transition symbol to look for in transitions.
     * @return Iterator for @c TransitionList; Allows us to compare to TransitionsList::end() to see whether the specified
     *  transitions exist. If they do, we can access the transitions with a dereference operator (*iter).
     */
    TransitionList::const_iterator get_transitions_from(State state_from, Symbol symbol) const;

    /**
     * Check whether automaton has no transitions.
     * @return True if there are no transitions in the automaton, false otherwise.
     */
    bool trans_empty() const;
    size_t get_num_of_trans() const; ///< Number of transitions; has linear time complexity.
    bool nothing_in_trans() const
    {
        return std::all_of(this->transitionrelation.begin(), this->transitionrelation.end(),
                    [](const auto& trans) {return trans.size() == 0;});
    }

    /**
     * Get transitions as a sequence of @c Trans.
     * @return Sequence of transitions as @c Trans.
     */
    TransSequence get_trans_as_sequence() const;

    /**
     * Get transitions from @p state_from as a sequence of @c Trans.
     * @param state_from[in] Source state_from of transitions to get.
     * @return Sequence of transitions as @c Trans from @p state_from.
     */
    TransSequence get_trans_from_as_sequence(State state_from) const;

    /**
     * Get transitions leading from @p state_from.
     * @param state_from[in] Source state for transitions to get.
     * @return List of transitions leading from @p state_from.
     */
    const TransitionList& get_transitions_from(const State state_from) const
    {
        assert(get_num_of_states() >= state_from + 1);
        return transitionrelation[state_from];
    }

    /**
     * Get transitions leading to @p state_to.
     * @param state_to[in] Target state for transitions to get.
     * @return Sequence of @c Trans transitions leading to @p state_to.
     */
    TransSequence get_transitions_to(State state_to) const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     * @param[in] abstract_symbol Abstract symbol to use for transitions in digraph.
     * @return An automaton representing a directed graph.
     */
    Nfa get_digraph(Symbol abstract_symbol = 'x') const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     *
     * @param[out] result An automaton representing a directed graph.
     */
    void get_digraph(Nfa& result) const;

    void print_to_DOT(std::ostream &outputStream) const;
    static Nfa read_from_our_format(std::istream &inputStream);

    StateSet post(const StateSet& states, const Symbol& symbol) const;

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

    /**
     * Return all epsilon transitions from epsilon symbol under a given state.
     * @param state State from which are epsilon transitions checked
     * @param epsilon User can define his favourite epsilon or used default
     * @return Returns reference element of transition list with epsilon transitions or end of transition list when
     * there are no epsilon transitions.
     */
    TransitionList::const_iterator get_epsilon_transitions(const State& state, const Symbol& epsilon = EPSILON) const;

private:
}; // Nfa

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

/**
 * Check whether is the language of the automaton empty.
 * @param[in] aut Automaton to check.
 * @param[out] cex Counter-example path for a case the language is not empty.
 * @return True if the language is empty, false otherwise.
 */
bool is_lang_empty(const Nfa& aut, Path* cex);

/**
 * Check whether is the language of the automaton empty.
 * @param[in] aut Automaton to check.
 * @return True if the language is empty, false otherwise.
 */
bool is_lang_empty(const Nfa& aut);

/**
 * Check whether is the language of the automaton empty.
 * @param[in] aut Automaton to check.
 * @param[out] cex Counter-example word for a case the language is not empty.
 * @return True if the language is empty, false otherwise.
 */
bool is_lang_empty_cex(const Nfa& aut, Word* cex);

Nfa uni(const Nfa &lhs, const Nfa &rhs);

inline void uni(Nfa *unionAutomaton, const Nfa &lhs, const Nfa &rhs)
{ // {{{
    *unionAutomaton = uni(lhs, rhs);
} // uni }}}

/**
 * @brief Compute intersection of two NFAs preserving epsilon transitions.
 *
 * Create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
 * of both automata. This means that for each ε-transition of the form `s -ε-> p` and each product state `(s, a)`,
 * an ε-transition `(s, a) -ε-> (p, a)` is created. Furthermore, for each ε-transition `s -ε-> p` and `a -ε-> b`,
 * a product state `(s, a) -ε-> (p, b)` is created.
 *
 * Automata must share alphabets.
 *
 * @param[in] lhs First NFA with possible epsilon symbols @p epsilon.
 * @param[in] rhs Second NFA with possible epsilon symbols @p epsilon.
 * @param[in] epsilon Symbol to handle as an epsilon symbol.
 * @param[out] prod_map Mapping of pairs of states (lhs_state, rhs_state) to new product states.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa intersection_over_epsilon(const Nfa &lhs, const Nfa &rhs, Symbol epsilon = EPSILON, ProductMap* prod_map = nullptr);

/**
 * @brief Compute intersection of two NFAs preserving epsilon transitions.
 *
 * Create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
 * of both automata. This means that for each ε-transition of the form `s -ε-> p` and each product state `(s, a)`,
 * an ε-transition `(s, a) -ε-> (p, a)` is created. Furthermore, for each ε-transition `s -ε-> p` and `a -ε-> b`,
 * a product state `(s, a) -ε-> (p, b)` is created.
 *
 * Automata must share alphabets.
 *
 * @param[out] res Result product NFA of the intersection of @p lhs and @p rhs with ε-transitions preserved.
 * @param[in] lhs First NFA with possible epsilon symbols @p epsilon.
 * @param[in] rhs Second NFA with possible epsilon symbols @p epsilon.
 * @param[in] epsilon Symbol to handle as an epsilon symbol.
 * @param[out] prod_map Mapping of pairs of states (lhs_state, rhs_state) to new product states.
 */
void intersection_over_epsilon(Nfa* res, const Nfa &lhs, const Nfa &rhs, Symbol epsilon = EPSILON,
                               ProductMap* prod_map = nullptr);

/**
 * @brief Compute intersection of two NFAs.
 *
 * @param[out] res Result product NFA of the intersection of @p lhs and @p rhs.
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[out] prod_map Mapping of pairs of states (lhs_state, rhs_state) to new product states.
 */
void intersection(Nfa* res, const Nfa& lhs, const Nfa& rhs, ProductMap* prod_map = nullptr);

/**
 * @brief Compute intersection of two NFAs.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa intersection(const Nfa &lhs, const Nfa &rhs, ProductMap* prod_map = nullptr);

/**
 * Concatenate two NFAs.
 * @param[out] res Concatenated automaton as a result of the concatenation of @p lhs and @p rhs.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 */
void concatenate(Nfa* res, const Nfa& lhs, const Nfa& rhs);

/**
 * Concatenate two NFAs.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @return Concatenated automaton.
 */
Nfa concatenate(const Nfa& lhs, const Nfa& rhs);

/**
 * Concatenate two NFAs over epsilon transitions.
 * @param[out] res Concatenated automaton as a result of the concatenation of @p lhs and @p rhs.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Epsilon symbol to concatenate @p lhs with @p rhs over.
 */
void concatenate_over_epsilon(Nfa* res, const Nfa& lhs, const Nfa& rhs, Symbol epsilon = EPSILON);

/**
 * Concatenate two NFAs over epsilon transitions.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Epsilon symbol to concatenate @p lhs with @p rhs over.
 * @return Concatenated automaton.
 */
Nfa concatenate_over_epsilon(const Nfa& lhs, const Nfa& rhs, Symbol epsilon = EPSILON);

/// makes the transition relation complete
void make_complete(
        Nfa&             aut,
        const Alphabet&  alphabet,
        State            sink_state);

// assumes deterministic automaton
void complement_in_place(Nfa &aut);

/// Co
Nfa complement(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params = {{"algo", "classical"}},
        SubsetMap*         subset_map = nullptr);

inline void complement(
        Nfa*               result,
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params = {{"algo", "classical"}},
        SubsetMap*         subset_map = nullptr)
{ // {{{
    *result = complement(aut, alphabet, params, subset_map);
} // complement }}}

Nfa minimize(const Nfa &aut);

inline void minimize(Nfa* res, const Nfa &aut)
{ // {{{
    *res = minimize(aut);
} // minimize }}}

/// Determinize an automaton
Nfa determinize(
        const Nfa&  aut,
        SubsetMap*  subset_map = nullptr);

inline void determinize(
        Nfa*        result,
        const Nfa&  aut,
        SubsetMap*  subset_map = nullptr)
{ // {{{
    *result = determinize(aut, subset_map);
} // determinize }}}

Simlib::Util::BinaryRelation compute_relation(
        const Nfa& aut,
        const StringDict&  params = {{"relation", "simulation"}, {"direction","forward"}});

// Reduce the size of the automaton
Nfa reduce(
        const Nfa &aut,
        StateToStateMap *state_map = nullptr,
        const StringDict&  params = {{"algorithm", "simulation"}});

inline void reduce(
        Nfa* result,
        const Nfa &aut,
        StateToStateMap *state_map = nullptr,
        const StringDict&  params = {{"algorithm", "simulation"}})
{ // {{{
    *result = reduce(aut, state_map, params);
} // reduce }}}

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

/**
 * @brief Checks inclusion of languages of two NFAs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param smaller[in] First automaton to concatenate.
 * @param bigger[in] Second automaton to concatenate.
 * @param cex[out] Counterexample for the inclusion.
 * @param alphabet[in] Alphabet of both NFAs to compute with.
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_incl(
        const Nfa&         smaller,
        const Nfa&         bigger,
        Word*              cex,
        const Alphabet*    alphabet = nullptr,
        const StringDict&  params = {{"algo", "antichains"}});

/**
 * @brief Checks inclusion of languages of two NFAs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param smaller[in] First automaton to concatenate.
 * @param bigger[in] Second automaton to concatenate.
 * @param alphabet[in] Alphabet of both NFAs to compute with.
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
inline bool is_incl(
        const Nfa&             smaller,
        const Nfa&             bigger,
        const Alphabet* const  alphabet = nullptr,
        const StringDict&      params = {{"algo", "antichains"}})
{ // {{{
    return is_incl(smaller, bigger, nullptr, alphabet, params);
} // }}}

/**
 * @brief Perform equivalence check of two NFAs: @p lhs and @p rhs.
 *
 * @param lhs[in] First automaton to concatenate.
 * @param rhs[in] Second automaton to concatenate.
 * @param alphabet[in] Alphabet of both NFAs to compute with.
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool equivalence_check(const Nfa& lhs, const Nfa& rhs, const Alphabet* alphabet,
                       const StringDict& params = {{"algo", "antichains"}});

/**
 * @brief Perform equivalence check of two NFAs: @p lhs and @p rhs.
 *
 * The current implementation of 'Mata::Nfa::Nfa' does not accept input alphabet. For this reason, an alphabet
 * has to be created from all transitions each time an operation on alphabet is called. When calling this function,
 * the alphabet has to be computed first.
 *
 * Hence, this function is less efficient than its alternative taking already defined alphabet as its parameter.
 * That way, alphabet has to be computed only once, as opposed to the current ad-hoc construction of the alphabet.
 * The use of the alternative with defined alphabet should be preferred.
 *
 * @param lhs[in] First automaton to concatenate.
 * @param rhs[in] Second automaton to concatenate.
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool equivalence_check(const Nfa& lhs, const Nfa& rhs, const StringDict& params = {{ "algo", "antichains"}});

/// Reverting the automaton
Nfa revert(const Nfa& aut);

inline void revert(Nfa* result, const Nfa& aut)
{ // {{{
    *result = revert(aut);
} // revert }}}

/// Removing epsilon transitions
Nfa remove_epsilon(const Nfa& aut, Symbol epsilon = EPSILON);

inline void remove_epsilon(Nfa* result, const Nfa& aut, Symbol epsilon = EPSILON)
{ // {{{
    *result = remove_epsilon(aut, epsilon);
} // remove_epsilon }}}

/// Test whether an automaton is deterministic, i.e., whether it has exactly
/// one initial state and every state has at most one outgoing transition over
/// every symbol.  Checks the whole automaton, not only the reachable part
bool is_deterministic(const Nfa& aut);

/// Test for automaton completeness wrt an alphabet.  An automaton is complete
/// if every reachable state has at least one outgoing transition over every
/// symbol.
bool is_complete(const Nfa& aut, const Alphabet& alphabet);

/** Loads an automaton from Parsed object */
Nfa construct(
        const Mata::Parser::ParsedSection&   parsec,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map = nullptr);

/** Loads an automaton from Parsed object */
Nfa construct(
        const Mata::IntermediateAut&         inter_aut,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map = nullptr);

template <class ParsedObject>
Nfa construct(
        const ParsedObject&                  parsed,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr)
{ // {{{
    Nfa aut;

    bool remove_symbol_map = false;
    if (nullptr == symbol_map)
    {
        symbol_map = new StringToSymbolMap();
        remove_symbol_map = true;
    }

    auto release_res = [&](){ if (remove_symbol_map) delete symbol_map; };

    Mata::Nfa::OnTheFlyAlphabet alphabet(symbol_map);

    try
    {
        aut = construct(parsed, &alphabet, state_map);
    }
    catch (std::exception&)
    {
        release_res();
        throw;
    }

    release_res();
    return aut;
}

/** Loads an automaton from Parsed object */
template <class ParsedObject>
void construct(
        Nfa*                                 result,
        const ParsedObject&                  parsed,
        StringToSymbolMap*                   symbol_map = nullptr,
        StringToStateMap*                    state_map = nullptr)
{ // {{{
    *result = construct(parsed, symbol_map, state_map);
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
	for (const auto& str : input) { result.push_back(symbol_map.at(str)); }
	return result;
} // encode_word }}}

/// operator<<
std::ostream& operator<<(std::ostream& strm, const Nfa& nfa);

/// global constructor to be called at program startup (from vm-dispatch)
void init();

/**
 * Operations on segment automata.
 */
namespace SegNfa
{
/// Segment automaton.
/// These are automata whose state space can be split into several segments connected by ε-transitions in a chain.
/// No other ε-transitions are allowed. As a consequence, no ε-transitions can appear in a cycle.
/// Segment automaton can have initial states only in the first segment and final states only in the last segment.
using SegNfa = Nfa;

/**
 * Class executing segmentation operations for a given segment automaton. Works only with segment automata.
 */
class Segmentation
{
public:
    using EpsilonDepth = unsigned; ///< Depth of ε-transitions.
    /// Dictionary of lists of ε-transitions grouped by their depth.
    /// For each depth 'i' we have 'depths[i]' which contains a list of ε-transitions of depth 'i'.
    using EpsilonDepthTransitions = std::unordered_map<EpsilonDepth, TransSequence>;

    /**
     * Prepare automaton @p aut for segmentation.
     * @param[in] aut Segment automaton to make segments for.
     * @param[in] epsilon Symbol to execute segmentation for.
     */
    Segmentation(const SegNfa& aut, const Symbol epsilon = EPSILON) : epsilon(epsilon), automaton(aut)
    {
        compute_epsilon_depths(); // Map depths to epsilon transitions.
    }

    /**
     * Get segmentation depths for ε-transitions.
     * @return Map of depths to lists of ε-transitions.
     */
    const EpsilonDepthTransitions& get_epsilon_depths() const { return epsilon_depth_transitions; }

    /**
     * Get segment automata.
     * @return A vector of segments for the segment automaton in the order from the left (initial state in segment automaton)
     * to the right (final states of segment automaton).
     */
    const AutSequence& get_segments();

    /**
     * Get raw segment automata.
     * @return A vector of segments for the segment automaton in the order from the left (initial state in segment automaton)
     * to the right (final states of segment automaton) without trimming (the states are same as in the original automaton).
     */
    const AutSequence& get_untrimmed_segments();

private:
    const Symbol epsilon; ///< Symbol for which to execute segmentation.
    /// Automaton to execute segmentation for. Must be a segment automaton (can be split into @p segments).
    const SegNfa& automaton;
    EpsilonDepthTransitions epsilon_depth_transitions{}; ///< Epsilon depths.
    AutSequence segments{}; ///< Segments for @p automaton.
    AutSequence segments_raw{}; ///< Raw segments for @p automaton.

    /**
     * Pair of state and its depth.
     */
    struct StateDepthPair
    {
        State state; ///< State with a depth.
        EpsilonDepth depth; ///< Depth of a state.
    };

    /**
     * Compute epsilon depths with their transitions.
     */
    void compute_epsilon_depths();

    /**
     * Split segment @c automaton into @c segments.
     */
    void split_aut_into_segments();

    /**
     * Propagate changes to the current segment automaton to the next segment automaton.
     * @param[in] current_depth Current depth.
     * @param[in] transition Current epsilon transition.
     */
    void update_next_segment(size_t current_depth, const Trans& transition);

    /**
     * Update current segment automaton.
     * @param[in] current_depth Current depth.
     * @param[in] transition Current epsilon transition.
     */
    void update_current_segment(size_t current_depth, const Trans& transition);

    /**
     * Initialize map of visited states.
     * @return Map of visited states.
     */
    StateMap<bool> initialize_visited_map() const;

    /**
     * Initialize worklist of states with depths to process.
     * @return Queue of state and its depth pairs.
     */
    std::deque<StateDepthPair> initialize_worklist() const;

    /**
     * Process pair of state and its depth.
     * @param[in] state_depth_pair Current state depth pair.
     * @param[out] worklist Worklist of state and depth pairs to process.
     */
    void process_state_depth_pair(const StateDepthPair& state_depth_pair, std::deque<StateDepthPair>& worklist);

    /**
     * Add states with non-epsilon transitions to the @p worklist.
     * @param state_transitions[in] Transitions from current state.
     * @param depth[in] Current depth.
     * @param worklist[out] Worklist of state and depth pairs to process.
     */
    static void add_transitions_to_worklist(const TransSymbolStates& state_transitions, EpsilonDepth depth,
                                            std::deque<StateDepthPair>& worklist);

    /**
     * Process epsilon transitions for the current state.
     * @param[in] state_depth_pair Current state depth pair.
     * @param[in] state_transitions Transitions from current state.
     * @param[out] worklist Worklist of state and depth pairs to process.
     */
    void handle_epsilon_transitions(const StateDepthPair& state_depth_pair, const TransSymbolStates& state_transitions,
                                    std::deque<StateDepthPair>& worklist);

    /**
     * @brief Remove inner initial and final states.
     *
     * Remove all initial states for all segments but the first one and all final states for all segments but the last one.
     */
    void remove_inner_initial_and_final_states();
}; // Segmentation
} // SegNfa

/**
 * Class mapping states to the shortest words accepted by languages of the states.
 */
class ShortestWordsMap
{
public:
    /**
     * Maps states in the automaton @p aut to shortest words accepted by languages of the states.
     * @param aut Automaton to compute shortest words for.
     */
    explicit ShortestWordsMap(const Nfa& aut) : reversed_automaton(revert(aut)) {
        insert_initial_lengths();
        compute();
    }

    /**
     * Gets shortest words for the given @p states.
     * @param[in] states States to map shortest words for.
     * @return Set of shortest words.
     */
    WordSet get_shortest_words_for(const StateSet& states) const;

    /**
     * Gets shortest words for the given @p state.
     * @param[in] state State to map shortest words for.
     * @return Set of shortest words.
     */
    WordSet get_shortest_words_for(State state) const;

private:
    using WordLength = int; ///< A length of a word.
    /// Pair binding the length of all words in the word set and word set with words of the given length.
    using LengthWordsPair = std::pair<WordLength, WordSet>;
    /// Map mapping states to the shortest words accepted by the automaton from the mapped state.
    StateMap<LengthWordsPair> shortest_words_map{};
    std::set<State> processed{}; ///< Set of already processed states.
    std::deque<State> fifo_queue{}; ///< FIFO queue for states to process.
    const Nfa reversed_automaton; ///< Reversed input automaton.

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
}; // class ShortestWordsMap.

class EnumAlphabet : public Alphabet
{
public:
    using InsertionResult = std::pair<StringToSymbolMap::const_iterator, bool>; ///< Result of the insertion of a new symbol.

    EnumAlphabet() = default;
    EnumAlphabet(const EnumAlphabet& rhs) : symbol_map(rhs.symbol_map), next_symbol_value(rhs.next_symbol_value) {}

private:
    // Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
    template<bool...> struct bool_pack{};
    /// Checks for all types in the pack.
    template<typename... Ts>
    using conjunction = std::is_same<bool_pack<true,Ts::value...>, bool_pack<Ts::value..., true>>;
    /// Checks whether all types are 'Nfa'.
    template<typename... Ts>
    using AreAllNfas = typename conjunction<std::is_same<Ts, const Nfa&>...>::type;

public:
    /**
     * Create alphabet from variable number of NFAs.
     * @tparam[in] Nfas Type Nfa.
     * @param[in] nfas NFAs to create alphabet from.
     * @return Created alphabet.
     */
    template<typename... Nfas, typename = AreAllNfas<Nfas...>>
    static EnumAlphabet from_nfas(const Nfas&... nfas) {
        EnumAlphabet alphabet{};
        // TODO: When we are on C++17, we can use fold expression here instead of the manual for_each_argument reimplementation.
        for_each_argument([&alphabet](const Nfa& aut) {
            fill_alphabet(aut, alphabet);
        }, nfas...);
        return alphabet;
    }

    /**
     * Create alphabet from vector of of NFAs.
     * @param[in] nfas Vector of NFAs to create alphabet from.
     * @return Created alphabet.
     */
    static EnumAlphabet from_nfas(const ConstAutRefSequence& nfas) {
        EnumAlphabet alphabet{};
        for (const auto& nfa: nfas) {
            fill_alphabet(nfa, alphabet);
        }
        return alphabet;
    }

    /**
     * Create alphabet from vector of of NFAs.
     * @param[in] nfas Vector of NFAs to create alphabet from.
     * @return Created alphabet.
     */
    static EnumAlphabet from_nfas(const AutRefSequence& nfas) {
        EnumAlphabet alphabet{};
        for (const auto& nfa: nfas) {
            fill_alphabet(nfa, alphabet);
        }
        return alphabet;
    }

    /**
     * Create alphabet from vector of of NFAs.
     * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
     * @return Created alphabet.
     */
    static EnumAlphabet from_nfas(const ConstAutPtrSequence& nfas) {
        EnumAlphabet alphabet{};
        for (const Nfa* const nfa: nfas) {
            fill_alphabet(*nfa, alphabet);
        }
        return alphabet;
    }

    /**
     * Create alphabet from vector of of NFAs.
     * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
     * @return Created alphabet.
     */
    static EnumAlphabet from_nfas(const AutPtrSequence& nfas) {
        EnumAlphabet alphabet{};
        for (const Nfa* const nfa: nfas) {
            fill_alphabet(*nfa, alphabet);
        }
        return alphabet;
    }

    /**
     * Expand alphabet by symbols from the passed @p nfa.
     * @param[in] nfa Automaton with whose transition symbols to expand the current alphabet.
     */
    void add_symbols_from(const Nfa& nfa);

    template <class InputIt>
    EnumAlphabet(InputIt first, InputIt last) : EnumAlphabet() {
        for (; first != last; ++first) {
            add_new_symbol(*first, next_symbol_value);
        }
    }

    EnumAlphabet(std::initializer_list<std::string> l) : EnumAlphabet(l.begin(), l.end()) {}

    Symbol translate_symb(const std::string& str) override
    {
        auto it = symbol_map.find(str);
        if (symbol_map.end() == it)
        {
            throw std::runtime_error("unknown symbol \'" + str + "\'");
        }

        return it->second;
    }

    std::list<Symbol> get_symbols() const override;
    std::list<Symbol> get_complement(const std::set<Symbol>& syms) const override;

    /**
     * @brief Add new symbol to the alphabet with the value of @c next_symbol_value.
     *
     * Throws an exception when the adding fails.
     *
     * @param[in] key User-space representation of the symbol.
     * @return Result of the insertion as @c InsertionResult.
     */
    InsertionResult add_new_symbol(const std::string& key) {
        InsertionResult insertion_result{ try_add_new_symbol(key, next_symbol_value) };
        if (!insertion_result.second) { // If the insertion of key-value pair failed.
            throw std::runtime_error("multiple occurrences of the same symbol");
        }
        ++next_symbol_value;
        return insertion_result;
    }

    /**
     * @brief Add new symbol to the alphabet.
     *
     * Throws an exception when the adding fails.
     *
     * @param[in] key User-space representation of the symbol.
     * @param[in] value Number of the symbol to be used on transitions.
     * @return Result of the insertion as @c InsertionResult.
     */
     InsertionResult add_new_symbol(const std::string& key, Symbol value) {
        InsertionResult insertion_result{ try_add_new_symbol(key, value) };
        if (!insertion_result.second) { // If the insertion of key-value pair failed.
            throw std::runtime_error("multiple occurrences of the same symbol");
        }
        update_next_symbol_value(value);
        return insertion_result;
    }

    /**
     * @brief Try to add symbol to the alphabet map.
     *
     * Does not throw an exception when the adding fails.
     *
     * @param[in] key User-space representation of the symbol.
     * @param[in] value Number of the symbol to be used on transitions.
     * @return Result of the insertion as @c InsertionResult.
     */
    InsertionResult try_add_new_symbol(const std::string& key, Symbol value) {
        return symbol_map.insert({ key, value});
    }

    /**
     * Get next value for a potential new symbol.
     * @return Next Symbol value.
     */
    Symbol get_next_value() const { return next_symbol_value; }

private:
    StringToSymbolMap symbol_map{}; ///< Map of string transition symbols to symbol values.
    Symbol next_symbol_value{}; ///< Next value to be used for a newly added symbol.

    EnumAlphabet& operator=(const EnumAlphabet& rhs);

    // Adapted from: https://stackoverflow.com/a/41623721.
    template <typename TF, typename... Ts>
    static void for_each_argument(TF&& f, Ts&&... xs)
    {
        std::initializer_list<const Nfa>{(f(std::forward<Ts>(xs)), Nfa{})... };
    }

    /**
     * @brief Update next symbol value when appropriate.
     *
     * When the newly inserted value is larger or equal to the current next symbol value, update he next symbol value to
     * a value one larger than the new value.
     * @param value The value of the newly added symbol.
     */
    void update_next_symbol_value(Symbol value) {
        if (next_symbol_value <= value) {
            next_symbol_value = value + 1;
        }
    }

    /**
     * Fill @p alphabet with symbols from @p nfa.
     * @param[in] nfa NFA with symbols to fill @p alphabet with.
     * @param[out] alphabet Alphabet to be filled with symbols from @p nfa.
     */
    static void fill_alphabet(const Nfa& nfa, EnumAlphabet& alphabet) {
        size_t nfa_num_of_states{ nfa.get_num_of_states() };
        for (State state{ 0 }; state < nfa_num_of_states; ++state) {
            for (const auto& state_transitions: nfa.transitionrelation[state]) {
                alphabet.update_next_symbol_value(state_transitions.symbol);
                alphabet.try_add_new_symbol(std::to_string(state_transitions.symbol), state_transitions.symbol);
            }
        }
    }
}; // class EnumAlphabet.

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
