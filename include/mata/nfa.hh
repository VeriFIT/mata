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
#include <memory>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// MATA headers
#include <mata/alphabet.hh>
#include <mata/parser.hh>
#include <mata/util.hh>
#include <mata/ord-vector.hh>
#include <mata/inter-aut.hh>
#include <mata/synchronized-iterator.hh>

namespace Mata::Nfa {
extern const std::string TYPE_NFA;

using State = unsigned long;

using StateSet = Mata::Util::OrdVector<State>;

template<typename T> using Set = Mata::Util::OrdVector<T>;

using WordSet = std::set<std::vector<Symbol>>;
struct Run {
    std::vector<Symbol> word; ///< A finite-length word.
    std::vector<State> path; ///< A finite-length path through automaton.
};

using StringToStateMap = std::unordered_map<std::string, State>;

using StateToStringMap = std::unordered_map<State, std::string>;
// using StateToPostMap = StateMap<PostSymb>; ///< Transitions.
/// Mapping of states to states, used, for example, to map original states to reindexed states of new automaton, etc.
using StateToStateMap = std::unordered_map<State, State>;

using SymbolToStringMap = std::unordered_map<Symbol, std::string>;
/*TODO: this should become a part of the automaton somehow.
 * It should be a vector indexed by states.
 * */

using StringMap = std::unordered_map<std::string, std::string>;

/*TODO: What about to
 * have names Set, UMap/OMap, State, Symbol, Sequence... and name by Set<State>, State<UMap>, ...
 * maybe something else is needed for the more complex maps*/

static constexpr struct Limits {
    State maxState = std::numeric_limits<State>::max();
    State minState = std::numeric_limits<State>::min();
    Symbol maxSymbol = std::numeric_limits<Symbol>::max();
    Symbol minSymbol = std::numeric_limits<Symbol>::min();
} limits;

/*TODO: Ideally remove functions using this struct as a parameter.
 * unpack the trans. relation to transitions is inefficient, goes against the hairs of the library.
 * Do we want to support it?
 */
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

//TODO: Kill these types names? Some of them?
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

// TODO: why introduce this type name?
using SharedPtrAut = std::shared_ptr<Nfa>; ///< A shared pointer to NFA.

/// serializes Nfa into a ParsedSection
Mata::Parser::ParsedSection serialize(
	const Nfa&                aut,
	const SymbolToStringMap*  symbol_map = nullptr,
	const StateToStringMap*   state_map = nullptr);

/**
 * Structure represents a move which is symbol and set of right-handed states of transitions.
 */
struct Move {
    Symbol symbol{};
    StateSet targets;

    Move() = default;
    explicit Move(Symbol symbolOnTransition) : symbol(symbolOnTransition), targets() {}
    Move(Symbol symbolOnTransition, State states_to) :
            symbol(symbolOnTransition), targets{states_to} {}
    Move(Symbol symbolOnTransition, const StateSet& states_to) :
            symbol(symbolOnTransition), targets(states_to) {}

    inline bool operator<(const Move& rhs) const { return symbol < rhs.symbol; }
    inline bool operator<=(const Move& rhs) const { return symbol <= rhs.symbol; }
    inline bool operator==(const Move& rhs) const { return symbol == rhs.symbol; }
    inline bool operator!=(const Move& rhs) const { return symbol != rhs.symbol; }
    inline bool operator>(const Move& rhs) const { return symbol > rhs.symbol; }
    inline bool operator>=(const Move& rhs) const { return symbol >= rhs.symbol; }

    StateSet::iterator begin() { return targets.begin(); }
    StateSet::iterator end() { return targets.end(); }

    StateSet::const_iterator cbegin() const  { return targets.cbegin(); }
    StateSet::const_iterator cend() const { return targets.cend(); }

    size_t count(State s) const { return targets.count(s); }
    bool empty() const { return targets.empty(); }
    size_t size() const { return targets.size(); }

    void insert(State s)
    {
        if (targets.find(s) == targets.end()) {
            targets.insert(s);
        }
    }

    void insert(StateSet states)
    {
        for (State s : states) {
            insert(s);
        }
    }

    void remove(State s) { targets.remove(s); }
};

/**
 * Post is a data structure representing possible transitions over different symbols.
 * It is an ordered vector containing possible Moves (i.e., pair of symbol and target states.
 * Vector is ordered by symbols which are numbers.
 */
struct Post : private Util::OrdVector<Move> {
    using iterator = Util::OrdVector<Move>::iterator;
    using const_iterator = Util::OrdVector<Move>::const_iterator;

    iterator begin() override { return Util::OrdVector<Move>::begin(); }
    const_iterator begin() const override { return Util::OrdVector<Move>::begin(); }
    iterator end() override { return Util::OrdVector<Move>::end(); }
    const_iterator end() const override { return Util::OrdVector<Move>::end(); }

    const_iterator cbegin() const override { return Util::OrdVector<Move>::cbegin(); }
    const_iterator cend() const override { return Util::OrdVector<Move>::cend(); }

    Post() = default;

    virtual ~Post() = default;

    const_iterator find(const Move& m) const override { return Util::OrdVector<Move>::find(m);}
    iterator find(const Move& m) override { return Util::OrdVector<Move>::find(m);}

    void insert(const Move& m) override { Util::OrdVector<Move>::insert(m); }

    const Move& back() const override { return Util::OrdVector<Move>::back(); }

    void remove(const Move& m)  { Util::OrdVector<Move>::remove(m); }

    bool empty() const override{ return Util::OrdVector<Move>::empty(); }
    size_t size() const override { return Util::OrdVector<Move>::size(); }

	const std::vector<Move>& ToVector() const
	{
		return Util::OrdVector<Move>::ToVector();
	}
};

/**
 * Delta is a data structure for representing transition relation.
 * Its underlying data structure is vector of Post structures.
 * Each index of vector corresponds to one state, that is a number of
 * state is an index to the vector of Posts.
 */
struct Delta {
private:
    std::vector<Post> post;

    /// Number of actual states occurring in the transition relation.
    ///
    /// These states are used in the transition relation, either on the left side or on the right side.
    /// The value is always consistent with the actual number of states in the transition relation.
    size_t m_num_of_states;

public:
    inline static const Post empty_post; //when post[q] is not allocated, then delta[q] returns this.

    Delta() : post(), m_num_of_states(0) {}
    explicit Delta(size_t n) : post(), m_num_of_states(n) {}

    void reserve(size_t n) {
        post.reserve(n);
        if (n > m_num_of_states) {
            m_num_of_states = n;
        }
    };

    /**
     * Size of delta is number of all transitions, i.e. triples of form (state, symbol, state)
     */
    size_t size() const;

    // Get a non const reference to post of a state, which allows modifying the post.
    //
    // BEWARE, IT HAS A SIDE EFFECT.
    //
    // Namely, it allocates the post of the state if it was not allocated yet. This in turn may cause that
    // the entire post data structure is re-allocated, iterators to it get invalidated ...
    // Use the constant [] operator below if possible.
    // Or, to prevent the side effect form happening, one might want to make sure that posts of all states in the automaton
    // are allocated, e.g., write an NFA method that allocate delta for all states of the NFA.
    // But it feels fragile, before doing something like that, better think and talk to people.
    Post & get_mutable_post(State q)
    {
        if (q >= post.size()) {
            const size_t new_size{ q + 1 };
            post.resize(new_size);
            if (new_size > m_num_of_states) {
                m_num_of_states = new_size;
            }
        }

        return post[q];
    };

    // Get a constant reference to the post of a state. No side effects.
    const Post & operator[] (State q) const
    {
        if (q >= post.size())
            return empty_post;
        else
            return post[q];
    };

    void emplace_back() {
        post.emplace_back();
        if (post.size() > m_num_of_states) { ++m_num_of_states; }
    }

    void clear()
    {
        post.clear();
        m_num_of_states = 0;
    }

    void increase_size(size_t n)
    {
        assert(n >= post.size());
        post.resize(n);
        if (post.size() > m_num_of_states)
            m_num_of_states = post.size();
    }

    size_t post_size() const { return post.size(); }

    void add(State state_from, Symbol symbol, State state_to);
    void add(const Trans& trans) { add(trans.src, trans.symb, trans.tgt); }
    void remove(State src, Symbol symb, State tgt);
    void remove(const Trans& trans) { remove(trans.src, trans.symb, trans.tgt); }

    bool contains(State src, Symbol symb, State tgt) const;

    /**
     * Check whether automaton contains no transitions.
     * @return True if there are no transitions in the automaton, false otherwise.
     */
    bool empty() const;

    size_t num_of_states() const { return m_num_of_states; }

    /**
     * Function removes empty indices in transition vector and renames states accordingly
     * @return Renaming of states where on index defined by old state number is a new number of the same state
     */
    std::vector<State> defragment();

    /**
     * Iterator over transitions. It iterates over triples (lhs, symbol, rhs) where lhs and rhs are states.
     */
    struct const_iterator {
    private:
        const std::vector<Post>& post;
        size_t current_state;
        Post::const_iterator post_iterator;
        StateSet::const_iterator targets_position;
        bool is_end;

    public:
        explicit const_iterator(const std::vector<Post>& post_p, bool ise = false);

        const_iterator(const std::vector<Post>& post_p, size_t as,
                       Post::const_iterator pi, StateSet::const_iterator ti, bool ise = false) :
                post(post_p), current_state(as), post_iterator(pi), targets_position(ti), is_end(ise) {};

        const_iterator(const const_iterator& other) = default;

        Trans operator*() const
        {
            return Trans{current_state, (*post_iterator).symbol, *targets_position};
        }

        // Prefix increment
        const_iterator& operator++();

        // Postfix increment
        const const_iterator operator++(int)
        {
            const const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator& operator=(const const_iterator& x)
        {
            this->post_iterator = x.post_iterator;
            this->targets_position = x.targets_position;
            this->current_state = x.current_state;
            this->is_end = x.is_end;

            return *this;
        }

        friend bool operator== (const const_iterator& a, const const_iterator& b)
        {
            if (a.is_end && b.is_end)
                return true;
            else if ((a.is_end && !b.is_end) || (!a.is_end && b.is_end))
                return false;
            else
                return a.current_state == b.current_state && a.post_iterator == b.post_iterator
                       && a.targets_position == b.targets_position;
        }

        friend bool operator!= (const const_iterator& a, const const_iterator& b) { return !(a == b); };
    };

    struct const_iterator cbegin() const
    {
        return const_iterator(post);
    }

    struct const_iterator cend() const
    {
        return const_iterator(post, true);
    }

    struct const_iterator begin() const
    {
        return cbegin();
    }

    struct const_iterator end() const
    {
        return cend();
    }

private:
    State find_max_state();
};

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = limits.maxSymbol;

/**
 * A struct representing an NFA.
 */
struct Nfa {
    /**
     * @brief For state q, delta[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to the number of states minus one.
     */
    Delta delta;
    Util::NumberPredicate<State> initial{};
    Util::NumberPredicate<State> final{};
    Alphabet* alphabet = nullptr; ///< The alphabet which can be shared between multiple automata.
    /// Key value store for additional attributes for the NFA. Keys are attribute names as strings and the value types
    ///  are up to the user.
    /// For example, we can set up attributes such as "state_dict" for state dictionary attribute mapping states to their
    ///  respective names, or "transition_dict" for transition dictionary adding a human-readable meaning to each
    ///  transition.
    // TODO: When there is a need for state dictionary, consider creating default library implementation of state
    //  dictionary in the attributes.
    std::unordered_map<std::string, void*> attributes{};

    /// Number of pre-requested states in the automaton.
    ///
    /// These states may be unallocated and they might not be used anywhere in the automaton.
    /// The value can be always less than the actual number of states in the whole automaton.
    ///
    /// This variable exists solely for the purpose of pre-requesting a certain number of states with
    ///  'Nfa::Nfa::add_state(n)' where 'n' is the number of requested states. However, it does not make sense to
    ///  allocate for these states space in Delta, nor in the sets of initial/final states. The variable should be
    ///  therefore always zero (or less than the actual number of states in the whole automaton), unless
    ///  'Nfa::Nfa::add_state(n)' was called. In that case, this variable will be set to n to store the information
    ///  that the user manually added (requested) new states.
    size_t m_num_of_requested_states{ 0 };

public:
    Nfa() : delta(), initial(), final(), m_num_of_requested_states(0) {}

    /**
     * @brief Construct a new explicit NFA with num_of_states states and optionally set initial and final states.
     *
     * @param[in] num_of_states Number of states for which to preallocate Delta.
     */
    explicit Nfa(const unsigned long num_of_states, const StateSet& initial_states = StateSet{},
                 const StateSet& final_states = StateSet{}, Alphabet* alphabet = new IntAlphabet())
        : delta(num_of_states), initial(initial_states), final(final_states), alphabet(alphabet), m_num_of_requested_states(0) {}

    /**
     * @brief Construct a new explicit NFA from other NFA.
     */
    Nfa(const Mata::Nfa::Nfa& other) = default;
    Nfa& operator=(const Mata::Nfa::Nfa& other) = default;

    /**
     * Clear transitions but keep the automata states.
     */
    void clear_transitions() {
        const size_t delta_size = delta.post_size();
        for (size_t i = 0; i < delta_size; ++i) {
            delta.get_mutable_post(i) = Post();
        }
    }

    /**
     * Add a new state to the automaton.
     * @return The newly created state.
     */
    State add_state();

    /**
     * Add a state provided by a user to the automaton. It increases size of NFA, if needed for adding the state.
     * @param state State be added to automaton
     * @return State added to automaton
     */
    State add_state(State state)
    {
        if (state >= size())
            m_num_of_requested_states = state + 1;

        return state;
    }

    /**
     * @brief Get the current number of states in the whole automaton.
     *
     * This includes the initial and final states as well as states in the transition relation.
     * @return The number of states.
     */
     size_t size() const {
        return std::max({m_num_of_requested_states, delta.num_of_states(), initial.domain_size(), final.domain_size() });
    }

    /**
     * Unify initial states into a single new initial state.
     */
    void unify_initial();

    /**
     * Unify final states into a single new final state.
     */
    void unify_final();

    bool is_state(const State &state_to_check) const { return state_to_check < size(); }

    /**
     * @brief Clear the underlying NFA to a blank NFA.
     *
     * The whole NFA is cleared, each member is set to its zero value.
     */
    void clear() {
        delta.clear();
        initial.clear();
        final.clear();
        m_num_of_requested_states = 0;
    }

    /**
     * @brief Get set of symbols used on the transitions in the automaton.
     *
     * Does not necessarily have to equal the set of symbols in the alphabet used by the automaton.
     * @return Set of symbols used on the transitions.
     */
    Util::OrdVector<Symbol> get_used_symbols() const;

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
     *
     * @param[out] state_map Mapping of trimmed states to new states.
     */
    void trim(StateToStateMap* state_map = nullptr);

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @param[out] state_map Mapping of trimmed states to new states.
     * @return Trimmed automaton.
     */
    Nfa get_trimmed_automaton(StateToStateMap* state_map = nullptr);

    // FIXME: Resolve this comment and delete it.
    /* Lukas: the above is nice. The good thing is that access to [q] is constant,
     * so one can iterate over all states for instance using this, and it is fast.
     * But I don't know how to do a similar thing inside Moves.
     * Returning a transition of q with the symbol a means to search for it in the list,
     * so iteration over the entire list would be very inefficient.
     * An efficient iteration would probably need an interface for an iterator, I don't know...
     * */

    /**
     * Remove epsilon transitions from the automaton.
     */
    void remove_epsilon(Symbol epsilon = EPSILON);

    size_t get_num_of_trans() const; ///< Number of transitions; contains linear time complexity.

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
     *
     * If we try to access a state which is present in the delta as a target state, yet does not have allocated space
     *  for itself in @c post, @c post is resized to include @p state_from.
     * @param state_from[in] Source state for transitions to get.
     * @return List of transitions leading from @p state_from.
     */
    const Post& get_moves_from(const State state_from) const
    {
        assert(state_from < size());
        return delta[state_from];
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
    Nfa get_one_letter_aut(Symbol abstract_symbol = 'x') const;

    /**
     * Check whether @p symbol is epsilon symbol or not.
     * @param symbol Symbol to check.
     * @return True if the passed @p symbol is epsilon, false otherwise.
     */
    bool is_epsilon(Symbol symbol) const {
        // TODO: When multiple epsilon symbols specification inside the alphabets is implemented, update this check to
        //  reflect the new changes:
        //  Check for alphabet in the NFA, check for specified epsilon symbol and compare. Otherwise, compare with the
        //  default epsilon symbol EPSILON.
        return symbol == EPSILON;
    }

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     *
     * @param[out] result An automaton representing a directed graph.
     */
    void get_one_letter_aut(Nfa& result) const;

    void print_to_DOT(std::ostream &outputStream) const;

    // TODO: Relict from VATA. What to do with inclusion/ universality/ this post function? Revise all of them.
    StateSet post(const StateSet& states, const Symbol& symbol) const;

    struct const_iterator
    { // {{{
        const Nfa* nfa;
        size_t trIt;
        Post::const_iterator tlIt;
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

    const Post& operator[](State state) const
    { // {{{
        assert(state < size());
        return delta[state];
    } // operator[] }}}


    /**
     * Return all epsilon transitions from epsilon symbol under a given state.
     * @param[in] state State from which are epsilon transitions checked
     * @param[in] epsilon User can define his favourite epsilon or used default
     * @return Returns reference element of transition list with epsilon transitions or end of transition list when
     * there are no epsilon transitions.
     */
    Post::const_iterator get_epsilon_transitions(State state, Symbol epsilon = EPSILON) const;

    /**
     * Return all epsilon transitions from epsilon symbol under given state transitions.
     * @param[in] state_transitions State transitions from which are epsilon transitions checked.
     * @param[in] epsilon User can define his favourite epsilon or used default
     * @return Returns reference element of transition list with epsilon transitions or end of transition list when
     * there are no epsilon transitions.
     */
    static Post::const_iterator get_epsilon_transitions(const Post& state_transitions, Symbol epsilon = EPSILON);

    /**
     * Method defragments transition relation. It eventually clears empty space in vector
     * containing transitions and decreases size.
     * TODO: once merged with new initial and final state predicate, do renaming of these sets of states.
     * TODO: Modify Nfa::m_num_of_requested_states as well. Or not?
     */
    void defragment() {
        std::vector<State> renaming = delta.defragment();
        initial.rename(renaming, delta.num_of_states());
        initial.truncate_domain();
        final.rename(renaming, delta.num_of_states());
        final.truncate_domain();
        m_num_of_requested_states = 0;
    }

    /**
     * @brief Expand alphabet by symbols from this automaton to given alphabet
     *
     * The value of the already existing symbols will NOT be overwritten.
     */
    void add_symbols_to(OnTheFlyAlphabet& alphabet);
}; // struct Nfa.

/**
 * Create automaton accepting only epsilon string.
 */
Nfa create_empty_string_nfa();

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 */
Nfa create_sigma_star_nfa(Alphabet* alphabet = new OnTheFlyAlphabet{});

/**
  * Fill @p alphabet with symbols from @p nfa.
  * @param[in] nfa NFA with symbols to fill @p alphabet with.
  * @param[out] alphabet Alphabet to be filled with symbols from @p nfa.
  */
void fill_alphabet(const Mata::Nfa::Nfa& nfa, Mata::OnTheFlyAlphabet& alphabet);

// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
template<bool...> struct bool_pack{};
/// Checks for all types in the pack.
template<typename... Ts> using conjunction = std::is_same<bool_pack<true,Ts::value...>, bool_pack<Ts::value..., true>>;
/// Checks whether all types are 'Nfa'.
template<typename... Ts> using AreAllNfas = typename conjunction<std::is_same<Ts, const Mata::Nfa::Nfa&>...>::type;

/**
 * Create alphabet from variable number of NFAs.
 * @tparam[in] Nfas Type Nfa.
 * @param[in] nfas NFAs to create alphabet from.
 * @return Created alphabet.
 */
template<typename... Nfas, typename = AreAllNfas<Nfas...>>
inline OnTheFlyAlphabet create_alphabet(const Nfas&... nfas) {
    Mata::OnTheFlyAlphabet alphabet{};
    auto f = [&alphabet](const Mata::Nfa::Nfa& aut) {
        fill_alphabet(aut, alphabet);
    };
    (f(nfas), ...);
    return alphabet;
}

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const ConstAutRefSequence& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const AutRefSequence& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const ConstAutPtrSequence& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const AutPtrSequence& nfas);

/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Nfa& lhs, const Nfa& rhs);

/**
 * Check whether is the language of the automaton empty.
 * @param[in] aut Automaton to check.
 * @param[out] cex Counter-example path for a case the language is not empty.
 * @return True if the language is empty, false otherwise.
 */
bool is_lang_empty(const Nfa& aut, Run* cex = nullptr);

Nfa uni(const Nfa &lhs, const Nfa &rhs);

/**
 * @brief Compute intersection of two NFAs.
 *
 * Supports epsilon symbols when @p preserve_epsilon is set to true.
 * When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can
 *  contain ε-transitions. The product preserves the ε-transitions
 *  of both automata. This means that for each ε-transition of the form `s -ε-> p` and each product state `(s, a)`,
 *  an ε-transition `(s, a) -ε-> (p, a)` is created. Furthermore, for each ε-transition `s -ε-> p` and `a -ε-> b`,
 *  a product state `(s, a) -ε-> (p, b)` is created.
 *
 * Automata must share alphabets.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[in] preserve_epsilon Whether to compute intersection preserving epsilon transitions.
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states.
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa intersection(const Nfa& lhs, const Nfa& rhs,
                 bool preserve_epsilon = false, std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two NFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_result_states_map Map mapping lhs states to result states.
 * @param[out] rhs_result_states_map Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
// TODO: check how fast is using just concatenate over epsilon and then call remove_epsilon().
Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon = false,
                StateToStateMap* lhs_result_states_map = nullptr, StateToStateMap* rhs_result_states_map = nullptr);


/// makes the transition relation complete
void make_complete(
        Nfa&             aut,
        const Alphabet&  alphabet,
        State            sink_state);

/// Co
Nfa complement(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringMap&  params = {{"algorithm", "classical"}},
        std::unordered_map<StateSet, State> *subset_map = nullptr);

Nfa minimize(const Nfa &aut);

/// Determinize an automaton
Nfa determinize(
        const Nfa&  aut,
        std::unordered_map<StateSet, State> *subset_map = nullptr);

/**
 * Reduce the size of the automaton.
 *
 * @param[in] aut Automaton to reduce.
 * @param[in] trim_input Whether to trim the input automaton first or not.
 * @param[out] state_map Mapping of trimmed states to new states.
 * @param params[in] Optional parameters to control the reduction algorithm:
 * - "algorithm": "simulation".
 * @return Reduced automaton.
 */
Nfa reduce(
        const Nfa &aut,
        bool trim_input = true,
        StateToStateMap *state_map = nullptr,
        const StringMap&  params = {{"algorithm", "simulation"}});

/// Is the language of the automaton universal?
bool is_universal(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        Run*              cex = nullptr,
        const StringMap&  params = {{"algorithm", "antichains"}});

inline bool is_universal(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringMap&  params)
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
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_included(
        const Nfa&         smaller,
        const Nfa&         bigger,
        Run*               cex,
        const Alphabet*    alphabet = nullptr,
        const StringMap&   params = {{"algorithm", "antichains"}});

/**
 * @brief Checks inclusion of languages of two NFAs: @p smaller and @p bigger (smaller <= bigger).
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 */
inline bool is_included(
        const Nfa&             smaller,
        const Nfa&             bigger,
        const Alphabet* const  alphabet = nullptr,
        const StringMap&      params = {{"algorithm", "antichains"}})
{ // {{{
    return is_included(smaller, bigger, nullptr, alphabet, params);
} // }}}

/**
 * @brief Perform equivalence check of two NFAs: @p lhs and @p rhs.
 *
 * @param lhs[in] First automaton to concatenate.
 * @param rhs[in] Second automaton to concatenate.
 * @param alphabet[in] Alphabet of both NFAs to compute with.
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const Alphabet* alphabet,
                    const StringMap& params = {{"algorithm", "antichains"}});

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
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const StringMap& params = {{"algorithm", "antichains"}});

/// Reverting the automaton
Nfa revert(const Nfa& aut);

/// Removing epsilon transitions
Nfa remove_epsilon(const Nfa& aut, Symbol epsilon = EPSILON);

/// Test whether an automaton is deterministic, i.e., whether it has exactly
/// one initial state and every state has at most one outgoing transition over
/// every symbol.  Checks the whole automaton, not only the reachable part
bool is_deterministic(const Nfa& aut);

/// Test for automaton completeness wrt an alphabet.  An automaton is complete
/// if every reachable state has at least one outgoing transition over every
/// symbol.
bool is_complete(const Nfa& aut, const Alphabet& alphabet);

std::pair<Run, bool> get_word_for_path(const Nfa& aut, const Run& run);

/// Checks whether a string is in the language of an automaton
bool is_in_lang(const Nfa& aut, const Run& word);

/// Checks whether the prefix of a string is in the language of an automaton
bool is_prfx_in_lang(const Nfa& aut, const Run& word);

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
 // TODO: rename to something, but no idea to what.
 // Maybe we need some terminology - Symbols and Words are made of numbers.
 // What are the symbol names and their sequences?
inline Run encode_word(
	const StringToSymbolMap&         symbol_map,
	const std::vector<std::string>&  input)
{ // {{{
	Run result;
	for (const auto& str : input) { result.word.push_back(symbol_map.at(str)); }
	return result;
} // encode_word }}}

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
    StringToSymbolMap tmp_symbol_map;
    if (symbol_map) {
        tmp_symbol_map = *symbol_map;
    }
    Mata::OnTheFlyAlphabet alphabet(tmp_symbol_map);

    Nfa aut = construct(parsed, &alphabet, state_map);

    if (symbol_map) {
        *symbol_map = alphabet.get_symbol_map();
    }
    return aut;
}

} // namespace Mata::Nfa.

namespace std
{ // {{{
template <>
struct hash<Mata::Nfa::Trans>
{
	inline size_t operator()(const Mata::Nfa::Trans& trans) const
	{
		size_t accum = std::hash<Mata::Nfa::State>{}(trans.src);
		accum = Mata::Util::hash_combine(accum, trans.symb);
		accum = Mata::Util::hash_combine(accum, trans.tgt);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Nfa& nfa);
} // std }}}


#endif /* _MATA_NFA_HH_ */
