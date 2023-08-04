/* nfa.hh -- Nondeterministic finite automaton (over finite words).
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

#ifndef MATA_NFA_HH_
#define MATA_NFA_HH_

// Static data structures, such as search stack, in algorithms. Might have some effect on some algorithms (like
//  fragile_revert).
//#define _STATIC_STRUCTURES_

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

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"
#include "mata/utils/util.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/utils/sparse-set.hh"
#include "types.hh"
#include "delta.hh"

/**
 * Nondeterministic Finite Automata including structures, transitions and algorithms.
 *
 * In particular, this includes:
 *   1. Structures (Automaton, Transitions, Results, Delta),
 *   2. Algorithms (operations, checks, tests),
 *   3. Constructions.
 *
 * Other algorithms are included in Mata::Nfa::Plumbing (simplified API for, e.g., binding)
 * and Mata::Nfa::Algorithms (concrete implementations of algorithms, such as for complement).
 */
namespace Mata::Nfa {

/**
 * A struct representing an NFA.
 */
struct Nfa {
public:
    /**
     * @brief For state q, delta[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to the number of states minus one.
     */
    Delta delta;
    Util::SparseSet<State> initial{};
    Util::SparseSet<State> final{};
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
    explicit Nfa(Delta delta = {}, Util::SparseSet<State> initial_states = {},
                 Util::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr)
        : delta(std::move(delta)), initial(std::move(initial_states)), final(std::move(final_states)), alphabet(alphabet) {}

    /**
     * @brief Construct a new explicit NFA with num_of_states states and optionally set initial and final states.
     *
     * @param[in] num_of_states Number of states for which to preallocate Delta.
     */
    explicit Nfa(const unsigned long num_of_states, StateSet initial_states = {},
                 StateSet final_states = {}, Alphabet* alphabet = nullptr)
        : delta(num_of_states), initial(initial_states), final(final_states), alphabet(alphabet) {}

    /**
     * @brief Construct a new explicit NFA from other NFA.
     */
    Nfa(const Mata::Nfa::Nfa& other) = default;

    Nfa(Mata::Nfa::Nfa&& other) noexcept
        : delta{ std::move(other.delta) }, initial{ std::move(other.initial) }, final{ std::move(other.final) },
          alphabet{ other.alphabet }, attributes{ std::move(other.attributes) } { other.alphabet = nullptr; }

    Nfa& operator=(const Mata::Nfa::Nfa& other) = default;
    Nfa& operator=(Mata::Nfa::Nfa&& other) noexcept;

    /**
     * Clear transitions but keep the automata states.
     */
    //method of delta?
    void clear_transitions();

    /**
     * Add a new (fresh) state to the automaton.
     * @return The newly created state.
     */
     //obsolete?
    State add_state();

    /**
     * Add state @p state to @c delta if @p state is not in @c delta yet.
     * @return The requested @p state.
     */
     //obsolete?
    State add_state(State state);

    /**
     * @brief Get the current number of states in the whole automaton.
     *
     * This includes the initial and final states as well as states in the transition relation.
     * @return The number of states.
     */
     //rename, again, to num_of_states?
     size_t size() const;

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
     //used?
    void clear();

    /**
     * @brief Check if @c this is exactly identical to @p aut.
     *
     * This is exact equality of automata, including state numbering (so even stronger than isomorphism),
     *  essentially only useful for testing purposes.
     * @return True if automata are exactly identical, false otherwise.
     */
    bool is_identical(const Nfa & aut);

    /**
     * @brief Get the set of symbols used on the transitions in the automaton.
     *
     * Does not necessarily have to equal the set of symbols in the alphabet used by the automaton.
     * @return Set of symbols used on the transitions.
     * TODO: this should be a method of Delta?
     */
    Util::OrdVector<Symbol> get_used_symbols() const;

    //this is to be debordelized, the variants were here just to play with data structures
    //should be a method of delta
    Mata::Util::OrdVector<Symbol> get_used_symbols_vec() const;
    std::set<Symbol> get_used_symbols_set() const;
    Mata::Util::SparseSet<Symbol> get_used_symbols_sps() const;
    std::vector<bool> get_used_symbols_bv() const;
    BoolVector get_used_symbols_chv() const;

    /**
     * @brief Get the maximum non-e used symbol.
     * TODO: this should be a method of Delta?
     */
    Symbol get_max_symbol() const;
    /**
     * @brief Get set of reachable states.
     *
     * Reachable states are states accessible from any initial state.
     * @return Set of reachable states.
     * TODO: with the new get_useful_states, it might be useless now.
     */
    StateSet get_reachable_states() const;

    /**
     * @brief Get set of terminating states.
     *
     * Terminating states are states leading to any final state.
     * @return Set of terminating states.
     * TODO: with the new get_useful_states, it might be useless now.
     */
    StateSet get_terminating_states() const;

    /**
     * @brief Get a set of useful states.
     *
     * Useful states are reachable and terminating states.
     * @return Set of useful states.
     * TODO: with the new get_useful_states, we can delete this probably.
     */
     //keep just the new one
    StateSet get_useful_states_old() const;

    /**
     * @brief Get the useful states using a modified Tarjan's algorithm. A state 
     * is useful if it is reachable from an initial state and can reach a final state.
     * 
     * @return BoolVector Bool vector whose ith value is true iff the state i is useful.
     */
    BoolVector get_useful_states() const;

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @param[out] state_map Mapping of trimmed states to new states.
     * TODO: we can probably keep just trim_reverting, much faster. But the speed difference and how it is achieved is interesting. Keeping as a demonstration for now.
     */
     //to be compared on some good data
    void trim_inplace(StateToStateMap* state_map = nullptr);
    void trim_reverting(StateToStateMap* state_map = nullptr);
    void trim(StateToStateMap* state_map = nullptr) { trim_inplace(state_map); }

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
    Nfa get_trimmed_automaton(StateToStateMap* state_map = nullptr) const;

    /**
     * Remove epsilon transitions from the automaton.
     */
    void remove_epsilon(Symbol epsilon = EPSILON);

    /**
     * @brief In-place concatenation.
     */
    Mata::Nfa::Nfa& concatenate(const Mata::Nfa::Nfa& aut);

    /**
     * @brief Get a number of transitions in the whole automaton.
     *
     * The operation has constant time complexity.
     */
     //remove get_ ?
    size_t get_num_of_trans() const { return static_cast<size_t>(std::distance(delta.begin(), delta.end())); }

    /**
     * Get transitions as a sequence of @c Trans.
     * @return Sequence of transitions as @c Trans.
     */
     //this will become obsolete, we will have iterators in delta
    TransSequence get_trans_as_sequence() const;

    /**
     * Get transitions from @p state_from as a sequence of @c Trans.
     * @param state_from[in] Source state_from of transitions to get.
     * @return Sequence of transitions as @c Trans from @p state_from.
     */
    //this will become obsolete, we will have iterators in delta
    TransSequence get_trans_from_as_sequence(State state_from) const;

    /**
     * Get transitions leading from @p state_from.
     *
     * If we try to access a state which is present in the delta as a target state, yet does not have allocated space
     *  for itself in @c post, @c post is resized to include @p state_from.
     * @param state_from[in] Source state for transitions to get.
     * @return List of transitions leading from @p state_from.
     */
    //this will become obsolete, we will have iterators in delta
    const Post& get_moves_from(const State state_from) const {
        assert(state_from < size());
        return delta[state_from];
    }

    /**
     * Get transitions leading to @p state_to.
     * @param state_to[in] Target state for transitions to get.
     * @return Sequence of @c Trans transitions leading to @p state_to.
     * (!slow!, traverses the entire delta)
     */
     //this function somehow does not fit with the planned iterator interface, but I don't know what to do
    TransSequence get_transitions_to(State state_to) const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     * @param[in] abstract_symbol Abstract symbol to use for transitions in digraph.
     * @return An automaton representing a directed graph.
     */
     //why the get_ everywhere?
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

    /**
     * @brief Prints the automaton in DOT format
     *
     * @return automaton in DOT format
     */
    std::string print_to_DOT() const;
    /**
     * @brief Prints the automaton to the output stream in DOT format
     */
    void print_to_DOT(std::ostream &output) const;
    /**
     * @brief Prints the automaton in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * @return automaton in mata format
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    std::string print_to_mata() const;
    /**
     * @brief Prints the automaton to the output stream in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    void print_to_mata(std::ostream &output) const;

    // TODO: Relict from VATA. What to do with inclusion/ universality/ this post function? Revise all of them.
    // One usage in universality, should disappear and this function should be removed.
    StateSet post(const StateSet& states, const Symbol& symbol) const;

    struct const_iterator {
        const Nfa* nfa;
        size_t trIt;
        Post::const_iterator tlIt;
        StateSet::const_iterator ssIt;
        Trans trans;
        bool is_end = { false };

        const_iterator() : nfa(), trIt(0), tlIt(), ssIt(), trans() { };
        static const_iterator for_begin(const Nfa* nfa);
        static const_iterator for_end(const Nfa* nfa);

        // FIXME: He, what is this? Some comment would help.
        // I am thinking about that removing everything having to do with Transition might be a good thing. Transition
        //  adds clutter and makes people write inefficient code.
        void refresh_trans() { this->trans = {trIt, this->tlIt->symbol, *(this->ssIt)}; }

        const Trans& operator*() const { return this->trans; }

        bool operator==(const const_iterator& rhs) const;
        bool operator!=(const const_iterator& rhs) const { return !(*this == rhs);}
        const_iterator& operator++();
    }; // }}}

    const_iterator begin() const { return const_iterator::for_begin(this); }
    const_iterator end() const { return const_iterator::for_end(this); }

    /**
     * Return all epsilon transitions from epsilon symbol under a given state.
     * @param[in] state State from which are epsilon transitions checked
     * @param[in] epsilon User can define his favourite epsilon or used default
     * @return Returns reference element of transition list with epsilon transitions or end of transition list when
     * there are no epsilon transitions.
     */
     //These things should be methods of delta?
    Post::const_iterator get_epsilon_transitions(State state, Symbol epsilon = EPSILON) const;

    /**
     * Return all epsilon transitions from epsilon symbol under given state transitions.
     * @param[in] post Post from which are epsilon transitions checked.
     * @param[in] epsilon User can define his favourite epsilon or used default
     * @return Returns reference element of transition list with epsilon transitions or end of transition list when
     * there are no epsilon transitions.
     */
    //These things should be methods of delta?
    static Post::const_iterator get_epsilon_transitions(const Post& post, Symbol epsilon = EPSILON);

    /**
     * @brief Expand alphabet by symbols from this automaton to given alphabet
     *
     * The value of the already existing symbols will NOT be overwritten.
     */
     //A method of delta as well?
    void add_symbols_to(OnTheFlyAlphabet& target_alphabet) const;
}; // struct Nfa.

/**
  * Fill @p alphabet with symbols from @p nfa.
  * @param[in] nfa NFA with symbols to fill @p alphabet with.
  * @param[out] alphabet Alphabet to be filled with symbols from @p nfa.
  */
    //A method of delta as well?
void fill_alphabet(const Mata::Nfa::Nfa& nfa, Mata::OnTheFlyAlphabet& alphabet);

// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
//Please explain in a comment here what this is.
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
 //What is this, what are these types? Is it sane?
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
 //We are to refactor alphabets, right?
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
 //Do we need all the variants? If yes, can we have one with iterators, or a template?
OnTheFlyAlphabet create_alphabet(const AutPtrSequence& nfas);

/// Do the automata have disjoint sets of states?
bool are_state_disjoint(const Nfa& lhs, const Nfa& rhs);

/**
 * Check whether is the language of the automaton empty.
 * @param[in] aut Automaton to check.
 * @param[out] cex Counter-example path for a case the language is not empty.
 * @return True if the language is empty, false otherwise.
 */
 //should be a method?
bool is_lang_empty(const Nfa& aut, Run* cex = nullptr);

//could be in-place method
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

//preserve_epsilon optional too? Do we need both options?
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
// choice of optional parameters using structure?
Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon = false,
                StateToStateMap* lhs_result_states_map = nullptr, StateToStateMap* rhs_result_states_map = nullptr);

/**
 * Make @c aut complete in place.
 *
 * For each state 0,...,aut.size()-1, add transitions with "missing" symbols from @p alphabet (symbols that do not occur
 *  on transitions from given state) to @p sink_state. If @p sink_state does not belong to the automaton, it is added to
 *  it, but only in the case that some transition to @p sink_state was added.
 * In the case that @p aut does not contain any states, this function does nothing.
 *
 * @param[in] aut Automaton to make complete.
 * @param[in] alphabet Alphabet to use for computing "missing" symbols.
 * @param[in] sink_state The state into which new transitions are added.
 * @return True if some new transition was added to the automaton.
 */
 //method complete
 //alphabet optional, sink stata too?
bool make_complete(Nfa& aut, const Alphabet& alphabet, State sink_state);

/**
 * Make @c aut complete in place.
 *
 * For each state 0,...,aut.size()-1, add transitions with "missing" symbols from @p alphabet (symbols that do not occur
 *  on transitions from given state) to @p sink_state. If @p sink_state does not belong to the automaton, it is added to
 *  it, but only in the case that some transition to @p sink_state was added.
 * In the case that @p aut does not contain any states, this function does nothing.
 *
 * This overloaded version is a more efficient version which does not need to compute the set of symbols to complete to
 *  from the alphabet. Prefer this version when you already have the set of symbols precomputed or plan to complete
 *  multiple automata over the same set of symbols.
 *
 * @param[in] aut Automaton to make complete.
 * @param[in] symbols Symbols to compute missing symbols from.
 * @param[in] sink_state The state into which new transitions are added.
 * @return True if some new transition was added to the automaton.
 */
 //eventually only one variant, the version with the optional alphabet (some enum alphabet type) ?
bool make_complete(Nfa& aut, const Util::OrdVector<Symbol>& symbols, State sink_state);

/**
 * For each state 0,...,aut.size()-1, add transitions with "missing" symbols from @p alphabet (symbols that do not occur
 * on transitions from given state) to new sink state (if no new transitions are added, this sink state is not created).
 * In the case that @p aut does not contain any states, this function does nothing.
 *
 * @param[in] aut Automaton to make complete.
 * @param[in] alphabet Alphabet to use for computing "missing" symbols.
 * @return True if some new transition (and sink state) was added to the automaton.
 */
inline bool make_complete(Nfa& aut, const Alphabet& alphabet) { return make_complete(aut, alphabet, aut.size()); }

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] alphabet Alphabet used for complementation.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
 //method
Nfa complement(const Nfa& aut, const Alphabet& alphabet,
   const StringMap& params = {{"algorithm", "classical"}, {"minimize", "false"}});

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * This overloaded version complements over an already created ordered set of @p symbols instead of an alphabet.
 * This is a more efficient solution in case you already have @p symbols precomputed or want to complement multiple
 *  automata over the same set of @c symbols: the function does not need to compute the ordered set of symbols from
 *  the alphabet again (and for each automaton).
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] symbols Symbols to complement over.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
//Do we need this? How many algorithms do we have? If yes, maybe these syntactic sugar functions could really have their own file.
Nfa complement(const Nfa& aut, const Util::OrdVector<Symbol>& symbols,
   const StringMap& params = {{"algorithm", "classical"}, {"minimize", "false"}});

/**
 * @brief Compute minimal deterministic automaton.
 *
 * @param[in] aut Automaton whose minimal version to compute.
 * @param[in] params Optional parameters to control the minimization algorithm:
 * - "algorithm": "brzozowski"
 * @return Minimal deterministic automaton.
 */
Nfa minimize(const Nfa &aut, const StringMap& params = {{"algorithm", "brzozowski"}});

/**
 * @brief Determinize automaton.
 *
 * @param[in] aut Automaton to determinize.
 * @param[out] subset_map Map that maps sets of states of input automaton to states of determinized automaton.
 * @return Determinized automaton.
 */
Nfa determinize(const Nfa&  aut, std::unordered_map<StateSet, State> *subset_map = nullptr);

/**
 * Reduce the size of the automaton.
 *
 * @param[in] aut Automaton to reduce.
 * @param[in] trim_input Whether to trim the input automaton first or not.
 * @param[out] state_map Mapping of trimmed states to new states.
 * @param[in] params Optional parameters to control the reduction algorithm:
 * - "algorithm": "simulation".
 * @return Reduced automaton.
 */
Nfa reduce(const Nfa &aut, bool trim_input = true, StateToStateMap *state_map = nullptr,
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
        const StringMap&  params) {
    return is_universal(aut, alphabet, nullptr, params);
}

/**
 * @brief Checks inclusion of languages of two NFAs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[out] cex Counterexample for the inclusion.
 * @param[in] alphabet Alphabet of both NFAs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
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
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFAs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
inline bool is_included(
        const Nfa&             smaller,
        const Nfa&             bigger,
        const Alphabet* const  alphabet = nullptr,
        const StringMap&      params = {{"algorithm", "antichains"}}) {
    return is_included(smaller, bigger, nullptr, alphabet, params);
}

/**
 * @brief Perform equivalence check of two NFAs: @p lhs and @p rhs.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFAs to compute with.
 * @param[in] params[ Optional parameters to control the equivalence check algorithm:
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
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const StringMap& params = {{"algorithm", "antichains"}});

// Reverting the automaton by one of the three functions below,
// currently simple_revert seems best (however, not tested enough).
Nfa revert(const Nfa& aut);

// This revert algorithm is fragile, uses low level accesses to Nfa and static data structures,
// and it is potentially dangerous when there are used symbols with large numbers (allocates an array indexed by symbols)
// It is faster asymptotically and for somewhat dense automata,
// the same or a little bit slower than simple_revert otherwise.
// Not affected by pre-reserving vectors.
Nfa fragile_revert(const Nfa& aut);

// Reverting the automaton by a simple algorithm, which does a lot of random access addition to Post and Move.
//  Much affected by pre-reserving vectors.
Nfa simple_revert(const Nfa& aut);

// Reverting the automaton by a modification of the simple algorithm.
// It replaces random access addition to Move by push_back and sorting later, so far seems the slowest of all, except on dense automata, where it is almost as slow as simple_revert. Candidate for removal.
Nfa somewhat_simple_revert(const Nfa& aut);

// Removing epsilon transitions
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
Run encode_word(const StringToSymbolMap& symbol_map, const std::vector<std::string>& input);

} // namespace Mata::Nfa.

namespace std {
    //some comments needed
template <>
struct hash<Mata::Nfa::Trans> {
	inline size_t operator()(const Mata::Nfa::Trans& trans) const {
		size_t accum = std::hash<Mata::Nfa::State>{}(trans.src);
		accum = Mata::Util::hash_combine(accum, trans.symb);
		accum = Mata::Util::hash_combine(accum, trans.tgt);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Trans& trans);
std::ostream& operator<<(std::ostream& os, const Mata::Nfa::Nfa& nfa);
} // namespace std.

#endif /* MATA_NFA_HH_ */
