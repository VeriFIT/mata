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
#include <mata/ord-vector.hh>
#include <mata/inter-aut.hh>
#include <simlib/util/binary_relation.hh>
#include <mata/synchronized-iterator.hh>

namespace Mata
{
namespace Nfa
{
extern const std::string TYPE_NFA;

using State = unsigned long;
using Symbol = unsigned long;

using StateSet = Mata::Util::OrdVector<State>;

template<typename T> using Set = Mata::Util::OrdVector<T>;

using WordSet = std::set<std::vector<Symbol>>;
struct Run {
    std::vector<Symbol> word; ///< A finite-length word.
    std::vector<State> path; ///< A finite-length path through automaton.
};

using StringToStateMap = std::unordered_map<std::string, State>;
using StringToSymbolMap = std::unordered_map<std::string, Symbol>;

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

/**
 * The abstract interface for NFA alphabets.
 */
class Alphabet {
public:
    /// translates a string into a symbol
    virtual Symbol translate_symb(const std::string& symb) = 0;

    /**
     * @brief Translate internal @p symbol representation back to its original string name.
     *
     * Throws an exception when the @p symbol is missing in the alphabet.
     * @param[in] symbol Symbol to translate.
     * @return @p symbol original name.
     */
    virtual std::string reverse_translate_symbol(Symbol symbol) const = 0;

    /// also translates strings to symbols
    Symbol operator[](const std::string& symb) { return this->translate_symb(symb); }

    /**
     * @brief Get a set of all symbols in the alphabet.
     *
     * The result does not have to equal the list of symbols in the automaton using this alphabet.
     */
    virtual Util::OrdVector<Symbol> get_alphabet_symbols() const
    { // {{{
        throw std::runtime_error("Unimplemented");
    } // }}}

    /// complement of a set of symbols wrt the alphabet
    virtual std::list<Symbol> get_complement(const std::set<Symbol>& syms) const
    { // {{{
        (void)syms;
        throw std::runtime_error("Unimplemented");
    } // }}}

    virtual ~Alphabet() = default;

    /**
     * @brief Check whether two alphabets are equal.
     *
     * In general, two alphabets are equal if and only if they are of the same class instance.
     * @param other_alphabet The other alphabet to compare with for equality.
     * @return True if equal, false otherwise.
     */
    virtual bool is_equal(const Alphabet& other_alphabet) const { return address() == other_alphabet.address(); }
    /**
     * @brief Check whether two alphabets are equal.
     *
     * In general, two alphabets are equal if and only if they are of the same class instance.
     * @param other_alphabet The other alphabet to compare with for equality.
     * @return True if equal, false otherwise.
     */
    virtual bool is_equal(const Alphabet* const other_alphabet) const { return address() == other_alphabet->address(); }

    bool operator==(const Alphabet&) const = delete;

protected:
    virtual const void* address() const { return this; }
}; // class Alphabet.

// const PostSymb EMPTY_POST{};

static constexpr struct Limits {//TODO: still needed?
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

//TODO: move alphabets to their own .h file
/**
* Direct alphabet (also identity alphabet or integer alphabet) using integers as symbols.
*
* This alphabet presumes that all integers are valid symbols.
* Therefore, calling member functions get_complement() and get_alphabet_symbols() makes no sense in this context and the methods
*  will throw exceptions warning about the inappropriate use of IntAlphabet. If one needs these functions, they should
*  use OnTheFlyAlphabet instead of IntAlphabet.
*/
class IntAlphabet : public Alphabet {
public:
    IntAlphabet(): alphabet_instance(IntAlphabetSingleton::get()) {}

    Symbol translate_symb(const std::string& symb) override {
        Symbol symbol;
        std::istringstream stream(symb);
        stream >> symbol;
        return symbol;
    }

    std::string reverse_translate_symbol(Symbol symbol) const override {
        return std::to_string(symbol);
    }

    Util::OrdVector<Symbol> get_alphabet_symbols() const override {
        throw std::runtime_error("Nonsensical use of get_alphabet_symbols() on IntAlphabet.");
    }

    std::list<Symbol> get_complement(const std::set<Symbol>& syms) const override {
        (void)syms;
        throw std::runtime_error("Nonsensical use of get_alphabet_symbols() on IntAlphabet.");
    }

    IntAlphabet(const IntAlphabet&) = default;
    IntAlphabet& operator=(const IntAlphabet& int_alphabet) = delete;
protected:
    const void* address() const override { return &alphabet_instance; }
private:
    /**
     * Singleton class implementing integer alphabet_instance for class IntAlphabet.
     *
     * Users have to use IntAlphabet instead which provides interface identical to other alphabets and can be used in
     *  places where an instance of the abstract class Alphabet is required.
     */
    class IntAlphabetSingleton {
    public:
        static IntAlphabetSingleton& get() {
            static IntAlphabetSingleton alphabet;
            return alphabet;
        }

        IntAlphabetSingleton(IntAlphabetSingleton&) = delete;
        IntAlphabetSingleton(IntAlphabetSingleton&&) = delete;
        IntAlphabetSingleton& operator=(const IntAlphabetSingleton&) = delete;
        IntAlphabetSingleton& operator=(IntAlphabetSingleton&&) = delete;

        ~IntAlphabetSingleton() = default;
    protected:
        IntAlphabetSingleton() = default;
    }; // class IntAlphabetSingleton.

    IntAlphabetSingleton& alphabet_instance;
}; // class IntAlphabet.

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

public:
    Delta() : post{} {}
    explicit Delta(size_t n) : post(n) {}

    void reserve(size_t n) { post.reserve(n); };

    Post & operator[] (State q)
    {
        assert(q < post.size() && "There is not transition for given state");
        return post[q];
    };

    const Post & operator[] (State q) const
    {
        assert(q < post.size() && "There is not transition for given state");
        return post[q];
    };

    void emplace_back() { post.emplace_back(); }

    void clear() { post.clear(); }
    bool empty() const { return post.empty(); }
    void resize(size_t n) { post.resize(n); }
    size_t size() const { return post.size(); }

    /**
     * Function removes empty indices in transition vector and renames states accordingly
     * @return Renaming of states where on index defined by old state number is a new number of the same state
     */
    std::vector<State> defragment()
    {
        std::vector<State> renaming(this->post.size());
        std::vector<State> removed{};

        size_t last_empty = 0;
        const size_t post_size = post.size();
        for (size_t i = 0; i < post_size; ++i) {
            if (post.at(i).empty()) {
                last_empty = i;
                removed.push_back(i);
            } else if (last_empty < i) {
                post[last_empty] = post[i];
                renaming[i] = last_empty;
                last_empty = i;
            } else { // there was no empty space, last_empty is synchronized with i
                renaming[i] = i; // nothing changed, no renaming needed
                last_empty += 1;
            }
        }

        if (!removed.empty())
            post.resize(post.size() - removed.size());
        else // no further renaming is needed, nothing has been done
            return renaming;

        const size_t removed_size = removed.size();
        for (size_t i = 0; i < removed_size; ++i) {
            renaming[removed[i]] = post.size()+i;
        }

        // rename states according to reindexing done above
        for (Post& p : this->post) {
            for (Move& m : p) {
                StateSet new_targets{};
                const size_t renaming_size = renaming.size();
                for (State i = 0; i < renaming_size; ++i) {
                    if (m.count(i)) {
                        m.remove(i);
                        new_targets.insert(renaming[i]);
                    }
                }
                m.insert(new_targets);
            }
        }

        return renaming;
    }

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
        explicit const_iterator(const std::vector<Post>& post_p, bool ise = false) :
                post(post_p), current_state(0), is_end(ise)
        {
            const size_t post_size = post.size();
            for (size_t i = 0; i < post_size; ++i) {
                if (!post[i].empty()) {
                    current_state = i;
                    post_iterator = post[i].begin();
                    targets_position = post_iterator->targets.begin();
                    return;
                }
            }

            // no transition found, an empty post
            is_end = true;
        }

        const_iterator(const std::vector<Post>& post_p, size_t as,
                       Post::const_iterator pi, StateSet::const_iterator ti, bool ise = false) :
                post(post_p), current_state(as), post_iterator(pi), targets_position(ti), is_end(ise) {};

        const_iterator(const const_iterator& other) = default;

        Trans operator*() const
        {
            return Trans{current_state, (*post_iterator).symbol, *targets_position};
        }

        // Prefix increment
        const_iterator& operator++()
        {
            assert(post.begin() != post.end());

            ++targets_position;
            if (targets_position != post_iterator->targets.end())
                return *this;

            ++post_iterator;
            if (post_iterator != post[current_state].cend()) {
                targets_position = post_iterator->targets.begin();
                return *this;
            }

            ++current_state;
            while (current_state < post.size() && post[current_state].empty()) // skip empty posts
                current_state++;

            if (current_state >= post.size())
                is_end = true;
            else {
                post_iterator = post[current_state].begin();
                targets_position = post_iterator->targets.begin();
            }

            return *this;
        }

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
};

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = limits.maxSymbol;

/**
 * A struct representing an NFA.
 */
struct Nfa
{
    /**
     * @brief For state q, delta[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to the number of states minus one.
     *
     */
    Delta delta;
    Util::NumberPredicate<State> initial = {};
    Util::NumberPredicate<State> final = {};
    Alphabet* alphabet = nullptr; ///< The alphabet which can be shared between multiple automata.
    /// Key value store for additional attributes for the NFA. Keys are attribute names as strings and the value types
    ///  are up to the user.
    /// For example, we can set up attributes such as "state_dict" for state dictionary attribute mapping states to their
    ///  respective names, or "transition_dict" for transition dictionary adding a human-readable meaning to each
    ///  transition.
    // TODO: When there is a need for state dictionary, consider creating default library implementation of state
    //  dictionary in the attributes.
    std::unordered_map<std::string, void*> attributes{};

public:
    Nfa() : delta(), initial(), final() {}

    /**
     * @brief Construct a new explicit NFA with num_of_states states and optionally set initial and final states.
     */
    explicit Nfa(const unsigned long num_of_states, const StateSet& initial_states = StateSet{},
                 const StateSet& final_states = StateSet{}, Alphabet* alphabet_p = new IntAlphabet())
        : delta(num_of_states), initial(initial_states), final(final_states),
          alphabet(alphabet_p) {}

    /**
     * @brief Construct a new explicit NFA from other NFA.
     */
    Nfa(const Mata::Nfa::Nfa& other) = default;
    Nfa& operator=(const Mata::Nfa::Nfa& other) = default;

    /**
     * Clear transitions but keep the automata states.
     */
    void clear_transitions() {
        const size_t delta_size = delta.size();
        for (size_t i = 0; i < delta_size; ++i) {
            delta[i] = Post();
        }
    }

    auto states_number() const { return delta.size(); }

    //TODO: why this? Maybe we could have add_state(int state)
    //Btw, why do we need adding states actually, we could just add states with transitions, or with initial/final states.
    void increase_size(size_t size)
    {
        assert(this->states_number() <= size);
        delta.resize(size);
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
     * Add a new state to the automaton.
     * @return The newly created state.
     */
    State add_state();

    /**
     * Unify initial states into a single new initial state.
     */
    void unify_initial();

    /**
     * Unify final states into a single new final state.
     */
    void unify_final();

    bool is_state(const State &state_to_check) const { return state_to_check < delta.size(); }

    /**
     * @brief Clear the underlying NFA to a blank NFA.
     *
     * The whole NFA is cleared, each member is set to its zero value.
     */
    void clear() {
        delta.clear();
        initial.clear();
        final.clear();
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
     * But I don't know how to do a similar thing inside Moves.
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
     * Add transitions from @p state_from with @p symbol to @p targets to automaton.
     * @param state_from Source state.
     * @param symbol Symbol on transitions.
     * @param states_to Set of target states.
     */
    void add_trans(State state_from, Symbol symbol, const StateSet& states_to);
    //TODO: rename all "trans" to "transition". At lest function names.

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
    void remove_epsilon(Symbol epsilon = EPSILON);

    bool has_trans(State src, Symbol symb, State tgt) const
    { // {{{
        if (delta.empty()) {
            return false;
        }

        const Post& tl = get_moves_from(src);
        if (tl.empty()) {
            return false;
        }
        auto symbol_transitions{ tl.find(Move{symb} ) };
        if (symbol_transitions == tl.cend()) {
            return false;
        }

        if (symbol_transitions->targets.find(tgt) == symbol_transitions->targets.end()) {
            return false;
        }

        return true;
    } // }}}

    /**
     * Check whether automaton has no transitions.
     * @return True if there are no transitions in the automaton, false otherwise.
     */
    bool has_no_transitions() const;

    size_t get_num_of_trans() const; ///< Number of transitions; has linear time complexity.

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
    const Post& get_moves_from(const State state_from) const
    {
        assert(states_number() >= state_from + 1);
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
        assert(state < delta.size());

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
     * TODO: once merged with new initial and final state predicate, do renaming of these sets of states
     */
    void defragment() { delta.defragment();}
}; // Nfa

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

// assumes deterministic automaton
void complement_in_place(Nfa &aut);

/// Co
Nfa complement(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringMap&  params = {{"algo", "classical"}},
        std::unordered_map<StateSet, State> *subset_map = nullptr);

Nfa minimize(const Nfa &aut);

/// Determinize an automaton
Nfa determinize(
        const Nfa&  aut,
        std::unordered_map<StateSet, State> *subset_map = nullptr);

Simlib::Util::BinaryRelation compute_relation(
        const Nfa& aut,
        const StringMap&  params = {{"relation", "simulation"}, {"direction", "forward"}});

// Reduce the size of the automaton
Nfa reduce(
        const Nfa &aut,
        StateToStateMap *state_map = nullptr,
        const StringMap&  params = {{"algorithm", "simulation"}});

/// Is the language of the automaton universal?
bool is_universal(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        Run*              cex = nullptr,
        const StringMap&  params = {{"algo", "antichains"}});

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
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_included(
        const Nfa&         smaller,
        const Nfa&         bigger,
        Run*               cex,
        const Alphabet*    alphabet = nullptr,
        const StringMap&   params = {{"algo", "antichains"}});

/**
 * @brief Checks inclusion of languages of two NFAs: @p smaller and @p bigger (smaller <= bigger).
 * @param params[in] Optional parameters to control the equivalence check algorithm:
 * - "algo": "naive", "antichains" (Default: "antichains")
 */
inline bool is_included(
        const Nfa&             smaller,
        const Nfa&             bigger,
        const Alphabet* const  alphabet = nullptr,
        const StringMap&      params = {{"algo", "antichains"}})
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
 * - "algo": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const Alphabet* alphabet,
                    const StringMap& params = {{"algo", "antichains"}});

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
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const StringMap& params = {{"algo", "antichains"}});

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

/**
 * An alphabet constructed 'on the fly'.
 * Should be use anytime the automata have a specific names for the symbols.
 */
class OnTheFlyAlphabet : public Alphabet {
public:
    using InsertionResult = std::pair<StringToSymbolMap::const_iterator, bool>; ///< Result of the insertion of a new symbol.

    explicit OnTheFlyAlphabet(Symbol init_symbol = 0) : next_symbol_value(init_symbol) {};
    OnTheFlyAlphabet(const OnTheFlyAlphabet& rhs) : symbol_map(rhs.symbol_map), next_symbol_value(rhs.next_symbol_value) {}

    explicit OnTheFlyAlphabet(const StringToSymbolMap& str_sym_map)
            : symbol_map(str_sym_map) {}

    /**
     * Create alphabet from a list of symbol names.
     * @param symbol_names Names for symbols on transitions.
     * @param init_symbol Start of a sequence of values to use for new symbols.
     */
    explicit OnTheFlyAlphabet(const std::vector<std::string>& symbol_names, Symbol init_symbol = 0)
            : symbol_map(), next_symbol_value(init_symbol) { add_symbols_from(symbol_names); }

    Util::OrdVector<Symbol> get_alphabet_symbols() const override;
    std::list<Symbol> get_complement(const std::set<Symbol>& syms) const override;

    std::string reverse_translate_symbol(const Symbol symbol) const override {
        for (const auto& symbol_mapping: symbol_map) {
            if (symbol_mapping.second == symbol) {
                return symbol_mapping.first;
            }
        }
        throw std::runtime_error("symbol '" + std::to_string(symbol) + "' is out of range of enumeration");
    }

private:
    OnTheFlyAlphabet& operator=(const OnTheFlyAlphabet& rhs);

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
    static OnTheFlyAlphabet from_nfas(const Nfas&... nfas) {
        OnTheFlyAlphabet alphabet{};
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
    static OnTheFlyAlphabet from_nfas(const ConstAutRefSequence& nfas) {
        OnTheFlyAlphabet alphabet{};
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
    static OnTheFlyAlphabet from_nfas(const AutRefSequence& nfas) {
        OnTheFlyAlphabet alphabet{};
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
    static OnTheFlyAlphabet from_nfas(const ConstAutPtrSequence& nfas) {
        OnTheFlyAlphabet alphabet{};
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
    static OnTheFlyAlphabet from_nfas(const AutPtrSequence& nfas) {
        OnTheFlyAlphabet alphabet{};
        for (const Nfa* const nfa: nfas) {
            fill_alphabet(*nfa, alphabet);
        }
        return alphabet;
    }

    /**
     * @brief Expand alphabet by symbols from the passed @p nfa.
     *
     * The value of the already existing symbols will NOT be overwritten.
     * @param[in] nfa Automaton with whose transition symbols to expand the current alphabet.
     */
    void add_symbols_from(const Nfa& nfa);

    /**
     * @brief Expand alphabet by symbols from the passed @p symbol_names.
     *
     * Adding a symbol name which already exists will throw an exception.
     * @param[in] symbol_names Vector of symbol names.
     */
    void add_symbols_from(const std::vector<std::string>& symbol_names) {
        for (const std::string& symbol_name: symbol_names) {
            add_new_symbol(symbol_name);
        }
    }

    /**
     * @brief Expand alphabet by symbols from the passed @p symbol_map.
     *
     * The value of the already existing symbols will NOT be overwritten.
     * @param[in] new_symbol_map Map of strings to symbols.
     */
    void add_symbols_from(const StringToSymbolMap& new_symbol_map);

    template <class InputIt>
    OnTheFlyAlphabet(InputIt first, InputIt last) : OnTheFlyAlphabet() {
        for (; first != last; ++first) {
            add_new_symbol(*first, next_symbol_value);
        }
    }

    OnTheFlyAlphabet(std::initializer_list<std::string> l) : OnTheFlyAlphabet(l.begin(), l.end()) {}

    Symbol translate_symb(const std::string& str) override
    {
        const auto it_insert_pair = symbol_map.insert({str, next_symbol_value});
        if (it_insert_pair.second) {
            return next_symbol_value++;
        } else {
            return it_insert_pair.first->second;
        }

        // TODO: How can the user specify to throw exceptions when we encounter an unknown symbol? How to specify that
        //  the alphabet should have only the previously fixed symbols?
        //auto it = symbol_map.find(str);
        //if (symbol_map.end() == it)
        //{
        //    throw std::runtime_error("unknown symbol \'" + str + "\'");
        //}

        //return it->second;
    }

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
     * Get the next value for a potential new symbol.
     * @return Next Symbol value.
     */
    Symbol get_next_value() const { return next_symbol_value; }

    /**
     * Get the number of existing symbols, epsilon symbols excluded.
     * @return The number of symbols.
     */
    size_t get_number_of_symbols() const { return next_symbol_value; }

    /**
     * Get the symbol map used in the alphabet.
     * @return Map mapping strings to symbols used internally in Mata.
     */
    const StringToSymbolMap& get_symbol_map() const { return symbol_map; }

private:
    StringToSymbolMap symbol_map{}; ///< Map of string transition symbols to symbol values.
    Symbol next_symbol_value{}; ///< Next value to be used for a newly added symbol.

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
    static void fill_alphabet(const Nfa& nfa, OnTheFlyAlphabet& alphabet) {
        size_t nfa_num_of_states{nfa.states_number() };
        for (State state{ 0 }; state < nfa_num_of_states; ++state) {
            for (const auto state_transitions: nfa.delta) {
                alphabet.update_next_symbol_value(state_transitions.symb);
                alphabet.try_add_new_symbol(std::to_string(state_transitions.symb), state_transitions.symb);
            }
        }
    }
}; // class OnTheFlyAlphabet.

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

    Mata::Nfa::OnTheFlyAlphabet alphabet(*symbol_map);

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
    if (!remove_symbol_map) {
        *symbol_map = alphabet.get_symbol_map();
    }
    return aut;
}

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
		accum = Mata::Util::hash_combine(accum, trans.symb);
		accum = Mata::Util::hash_combine(accum, trans.tgt);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Nfa& nfa);
std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Alphabet& alphabet);
} // std }}}


#endif /* _MATA_NFA_HH_ */
