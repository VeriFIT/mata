/* nfa.hh -- Nondeterministic finite automaton (over finite words).
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
#include <queue>

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"
#include "mata/utils/utils.hh"
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
 * Other algorithms are included in mata::nfa::Plumbing (simplified API for, e.g., binding)
 * and mata::nfa::algorithms (concrete implementations of algorithms, such as for complement).
 */
namespace mata::nfa {

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
    utils::SparseSet<State> initial{};
    utils::SparseSet<State> final{};
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
    explicit Nfa(Delta delta = {}, utils::SparseSet<State> initial_states = {},
                 utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr)
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
    Nfa(const Nfa& other) = default;

    Nfa(Nfa&& other) noexcept
        : delta{ std::move(other.delta) }, initial{ std::move(other.initial) }, final{ std::move(other.final) },
          alphabet{ other.alphabet }, attributes{ std::move(other.attributes) } { other.alphabet = nullptr; }

    Nfa& operator=(const Nfa& other) = default;
    Nfa& operator=(Nfa&& other) noexcept;

    /**
     * Add a new (fresh) state to the automaton.
     * @return The newly created state.
     */
    State add_state();

    /**
     * Add state @p state to @c delta if @p state is not in @c delta yet.
     * @return The requested @p state.
     */
    State add_state(State state);

    /**
     * @brief Get the current number of states in the whole automaton.
     *
     * This includes the initial and final states as well as states in the transition relation.
     * @return The number of states.
     */
     size_t num_of_states() const;

    /**
     * Unify initial states into a single new initial state.
     */
    void unify_initial();

    /**
     * Unify final states into a single new final state.
     */
    void unify_final();

    /**
     * Swap final and non-final states in-place.
     */
    Nfa& swap_final_nonfinal() { final.complement(num_of_states()); return *this; }

    bool is_state(const State &state_to_check) const { return state_to_check < num_of_states(); }

    /**
     * @brief Clear the underlying NFA to a blank NFA.
     *
     * The whole NFA is cleared, each member is set to its zero value.
     */
    void clear();

    /**
     * @brief Check if @c this is exactly identical to @p aut.
     *
     * This is exact equality of automata, including state numbering (so even stronger than isomorphism),
     *  essentially only useful for testing purposes.
     * @return True if automata are exactly identical, false otherwise.
     */
    bool is_identical(const Nfa& aut) const;

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
     * @brief Get the useful states using a modified Tarjan's algorithm. A state
     * is useful if it is reachable from an initial state and can reach a final state.
     *
     * @return BoolVector Bool vector whose ith value is true iff the state i is useful.
     */
    BoolVector get_useful_states() const;

    /**
     * @brief Structure for storing callback functions (event handlers) utilizing 
     * Tarjan's SCC discover algorithm.
     */
    struct TarjanDiscoverCallback {
        // event handler for the first-time state discovery
        std::function<bool(State)> state_discover;
        // event handler for SCC discovery (together with the whole Tarjan stack)
        std::function<bool(const std::vector<State>&, const std::vector<State>&)> scc_discover;
        // event handler for state in SCC discovery
        std::function<void(State)> scc_state_discover;
        // event handler for visiting of the state successors
        std::function<void(State,State)> succ_state_discover;
    };

    /**
     * @brief Tarjan's SCC discover algorihm.
     * 
     * @param callback Callbacks class to instantiate callbacks for the Tarjan's algorithm.
     */
    void tarjan_scc_discover(const TarjanDiscoverCallback& callback) const;

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states in-place.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @param[out] state_renaming Mapping of trimmed states to new states.
     * @return @c this after trimming.
     */
    Nfa& trim(StateRenaming* state_renaming = nullptr);

    std::vector<State> distances_from_initial() const;

    /**
     * Remove epsilon transitions from the automaton.
     */
    void remove_epsilon(Symbol epsilon = EPSILON);

    /**
     * @brief In-place concatenation.
     */
    Nfa& concatenate(const Nfa& aut);

    /**
     * @brief In-place union
     */
    Nfa& uni(const Nfa &aut);

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
    StateSet post(const StateSet& states, const Symbol& symbol) const;

    /**
     * Check whether the language of NFA is empty. 
     * Currently calls is_lang_empty_scc if cex is null 
     * @param[out] cex Counter-example path for a case the language is not empty.
     * @return True if the language is empty, false otherwise.
     */
    bool is_lang_empty(Run* cex = nullptr) const;

    /**
     * @brief Check if the language is empty using Tarjan's SCC discover algorithm.
     * 
     * @return Language empty <-> True
     */
    bool is_lang_empty_scc() const;

    /**
     * @brief Test whether an automaton is deterministic.
     *
     * I.e., whether it has exactly one initial state and every state has at most one outgoing transition over every
     *  symbol.
     * Checks the whole automaton, not only the reachable part
     */
    bool is_deterministic() const;

    /**
     * @brief Test for automaton completeness with regard to an alphabet.
     *
     * An automaton is complete if every reachable state has at least one outgoing transition over every symbol.
     */
    bool is_complete(Alphabet const* alphabet = nullptr) const;

    /**
     * @brief Is the automaton graph acyclic? Used for checking language finiteness.
     * 
     * @return true <-> Automaton graph is acyclic.
     */
    bool is_acyclic() const;

    /**
     * Fill @p alphabet_to_fill with symbols from @p nfa.
     * @param[in] nfa NFA with symbols to fill @p alphabet_to_fill with.
     * @param[out] alphabet_to_fill Alphabet to be filled with symbols from @p nfa.
     */
    void fill_alphabet(mata::OnTheFlyAlphabet& alphabet_to_fill) const;

    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, Run* cex = nullptr,
                      const ParameterMap& params = {{ "algorithm", "antichains" }}) const;
    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, const ParameterMap& params) const;

    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Run& word) const;
    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Word& word) { return is_in_lang(Run{ word, {} }); }

    /// Checks whether the prefix of a string is in the language of an automaton
    bool is_prfx_in_lang(const Run& word) const;

    std::pair<Run, bool> get_word_for_path(const Run& run) const;

    /**
     * @brief Get the set of all words in the language of the automaton whose length is <= @p max_length
     * 
     * If you have an automaton with finite language (can be checked using @ref is_acyclic),
     * you can get all words by calling
     *      get_words(aut.num_of_states())
     */
    std::set<Word> get_words(unsigned max_length);

    /**
     * @brief Get any arbitrary accepted word in the language of the automaton.
     *
     * The automaton is searched using DFS, returning a word for the first reached final state.
     */
    std::optional<Word> get_word(Symbol first_epsilon = EPSILON) const;

    /**
     * @brief Make NFA complete in place.
     *
     * For each state 0,...,this->num_of_states()-1, add transitions with "missing" symbols from @p alphabet
     *  (symbols that do not occur on transitions from given state) to @p sink_state. If @p sink_state does not belong
     *  to the NFA, it is added to it, but only in the case that some transition to @p sink_state was added.
     * In the case that NFA does not contain any states, this function does nothing.
     *
     * @param[in] alphabet Alphabet to use for computing "missing" symbols.
     * @param[in] sink_state The state into which new transitions are added.
     * @return True if some new transition was added to the NFA.
     */
    bool make_complete(const Alphabet& alphabet, State sink_state);

    /**
     * @brief Make NFA complete in place.
     *
     * For each state 0,...,this->num_of_states()-1, add transitions with "missing" symbols from @p alphabet
     *  (symbols that do not occur on transitions from given state) to @p sink_state. If @p sink_state does not belong
     *  to the NFA, it is added to it, but only in the case that some transition to @p sink_state was added.
     * In the case that NFA does not contain any states, this function does nothing.
     *
     * This overloaded version is a more efficient version which does not need to compute the set of symbols to
     *  complete to from the alphabet. Prefer this version when you already have the set of symbols precomputed or plan
     *  to complete multiple automata over the same set of symbols.
     *
     * @param[in] symbols Symbols to compute missing symbols from.
     * @param[in] sink_state The state into which new transitions are added.
     * @return True if some new transition was added to the automaton.
     */
    bool make_complete(const utils::OrdVector<Symbol>& symbols, State sink_state);

    /**
     * @brief Make NFA complete in place.
     *
     * For each state 0,...,this->num_of_states()-1, add transitions with "missing" symbols from @p alphabet
     *  (symbols that do not occur on transitions from given state) to new sink state (if no new transitions are added,
     *  this sink state is not created).
     * In the case that NFA does not contain any states, this function does nothing.
     *
     * @param[in] alphabet Alphabet to use for computing "missing" symbols.
     * @return True if some new transition (and sink state) was added to the automaton.
     */
    bool make_complete(const Alphabet& alphabet) { return this->make_complete(alphabet, this->num_of_states()); }

    /**
     * Complement deterministic automaton in-place by adding a sink state and swapping final and non-final states.
     * @param[in] symbols Symbols needed to make the automaton complete.
     * @param[in] sink_state State to be used as a sink state. Adds a new sink state when not specified.
     * @return DFA complemented in-place.
     * @pre @c this is a deterministic automaton.
     */
    Nfa& complement_deterministic(const mata::utils::OrdVector<Symbol>& symbols, std::optional<State> sink_state = std::nullopt);
}; // struct Nfa.

// Allow variadic number of arguments of the same type.
//
// Using parameter pack and variadic arguments.
// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
/// Pack of bools for reasoning about a sequence of parameters.
template<bool...> struct bool_pack{};
/// Check that for all values in a pack @p Ts are 'true'.
template<typename... Ts> using conjunction = std::is_same<bool_pack<true,Ts::value...>, bool_pack<Ts::value..., true>>;
/// Check that all types in a sequence of parameters @p Ts are of type @p T.
template<typename T, typename... Ts> using AreAllOfType = typename conjunction<std::is_same<Ts, T>...>::type;

/**
 * Create alphabet from variadic number of NFAs given as arguments.
 * @tparam[in] Nfas Type Nfa.
 * @param[in] nfas NFAs to create alphabet from.
 * @return Created alphabet.
 */
template<typename... Nfas, typename = AreAllOfType<const Nfa&, Nfas...>>
inline OnTheFlyAlphabet create_alphabet(const Nfas&... nfas) {
    mata::OnTheFlyAlphabet alphabet{};
    auto f = [&alphabet](const Nfa& aut) {
        aut.fill_alphabet(alphabet);
    };
    (f(nfas), ...);
    return alphabet;
}

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const std::vector<std::reference_wrapper<const Nfa>>& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const std::vector<std::reference_wrapper<Nfa>>& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const std::vector<Nfa*>& nfas);

/**
 * Create alphabet from a vector of NFAs.
 * @param[in] nfas Vector of pointers to NFAs to create alphabet from.
 * @return Created alphabet.
 */
OnTheFlyAlphabet create_alphabet(const std::vector<const Nfa*>& nfas);

Nfa uni(const Nfa &lhs, const Nfa &rhs);

/**
 * @brief Compute intersection of two NFAs.
 *
 * Both automata can contain ε-transitions. The product preserves the ε-transitions, i.e.,
 * for each each product state `(s, t)` with`s -ε-> p`, `(s, t) -ε-> (p, t)` is created, and vice versa.
 *
 * Automata must share alphabets. //TODO: this is not implemented yet.
 *
 * @param[in] lhs First NFA to compute intersection for.
 * @param[in] rhs Second NFA to compute intersection for.
 * @param[in] first_epsilon smallest epsilon. //TODO: this should eventually be taken from the alphabet as anything larger than the largest symbol?
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states (not used internally, allocated only when !=nullptr, expensive).
 * @return NFA as a product of NFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Nfa intersection(const Nfa& lhs, const Nfa& rhs,
                 const Symbol first_epsilon = EPSILON, std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two NFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
// TODO: check how fast is using just concatenate over epsilon and then call remove_epsilon().
Nfa concatenate(const Nfa& lhs, const Nfa& rhs, bool use_epsilon = false,
                StateRenaming* lhs_state_renaming = nullptr, StateRenaming* rhs_state_renaming = nullptr);

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] alphabet Alphabet used for complementation.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm":
 *      - "classical": The classical algorithm determinizes the automaton, makes it complete, and swaps final and
 *                      non-final states.
 *      - "brzozowski": The Brzozowski algorithm determinizes the automaton using Brzozowski minimization, makes it
 *                       complete, and swaps final and non-final states.
 * @return Complemented automaton.
 */
Nfa complement(const Nfa& aut, const Alphabet& alphabet, const ParameterMap& params = { { "algorithm", "classical" } });

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
 * - "algorithm":
 *      - "classical": The classical algorithm determinizes the automaton, makes it complete, and swaps final and
 *                      non-final states.
 *      - "brzozowski": The Brzozowski algorithm determinizes the automaton using Brzozowski minimization, makes it
 *                       complete, and swaps final and non-final states.
 * @return Complemented automaton.
 */
Nfa complement(const Nfa& aut, const utils::OrdVector<Symbol>& symbols,
               const ParameterMap& params = { { "algorithm", "classical" } });

/**
 * @brief Compute minimal deterministic automaton.
 *
 * @param[in] aut Automaton whose minimal version to compute.
 * @param[in] params Optional parameters to control the minimization algorithm:
 * - "algorithm": "brzozowski"
 * @return Minimal deterministic automaton.
 */
Nfa minimize(const Nfa &aut, const ParameterMap& params = { { "algorithm", "brzozowski" } });

/**
 * @brief Determinize automaton.
 *
 * @param[in] aut Automaton to determinize.
 * @param[out] subset_map Map that maps sets of states of input automaton to states of determinized automaton.
 * @return Determinized automaton.
 */
Nfa determinize(const Nfa& aut, std::unordered_map<StateSet, State> *subset_map = nullptr);

/**
 * @brief Reduce the size of the automaton.
 *
 * @param[in] aut Automaton to reduce.
 * @param[out] state_renaming Mapping of original states to reduced states.
 * @param[in] params Optional parameters to control the reduction algorithm:
 * - "algorithm": "simulation", "residual",
 *      and options to parametrize residual reduction, not utilized in simulation
 * - "type": "after", "with",
 * - "direction": "forward", "backward".
 * @return Reduced automaton.
 */
Nfa reduce(const Nfa &aut, StateRenaming *state_renaming = nullptr,
           const ParameterMap& params = {{ "algorithm", "simulation" }, { "type", "after" }, { "direction", "forward" } });

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
bool is_included(const Nfa& smaller, const Nfa& bigger, Run* cex, const Alphabet* alphabet = nullptr,
                 const ParameterMap& params = {{ "algorithm", "antichains" }});

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
inline bool is_included(const Nfa& smaller, const Nfa& bigger, const Alphabet* const alphabet = nullptr,
                        const ParameterMap& params = {{ "algorithm", "antichains" }}) {
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
                    const ParameterMap& params = {{ "algorithm", "antichains"}});

/**
 * @brief Perform equivalence check of two NFAs: @p lhs and @p rhs.
 *
 * The current implementation of @c Nfa does not accept input alphabet. For this reason, an alphabet
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
bool are_equivalent(const Nfa& lhs, const Nfa& rhs, const ParameterMap& params = {{ "algorithm", "antichains"}});

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
// It replaces random access addition to SymbolPost by push_back and sorting later, so far seems the slowest of all, except on
//  dense automata, where it is almost as slow as simple_revert. Candidate for removal.
Nfa somewhat_simple_revert(const Nfa& aut);

// Removing epsilon transitions
Nfa remove_epsilon(const Nfa& aut, Symbol epsilon = EPSILON);

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
 // TODO: rename to something, but no idea to what.
 // Maybe we need some terminology - Symbols and Words are made of numbers.
 // What are the symbol names and their sequences?
Run encode_word(const Alphabet* alphabet, const std::vector<std::string>& input);

} // namespace mata::nfa.

namespace std {
template <>
struct hash<mata::nfa::Transition> {
	inline size_t operator()(const mata::nfa::Transition& trans) const {
		size_t accum = std::hash<mata::nfa::State>{}(trans.source);
		accum = mata::utils::hash_combine(accum, trans.symbol);
		accum = mata::utils::hash_combine(accum, trans.target);
		return accum;
	}
};

std::ostream& operator<<(std::ostream& os, const mata::nfa::Transition& trans);
std::ostream& operator<<(std::ostream& os, const mata::nfa::Nfa& nfa);
} // namespace std.

#endif /* MATA_NFA_HH_ */
